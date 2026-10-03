# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the locked VIXY short. Does not import the study engine.

Matches every monthly holding in trades.csv, recomputes the UVXY out-of-sample
Sharpe against results.json, and writes jumps.json for sessions whose adjusted
close still jumps with the raw close. Appends one RUNLOG entry.
"""

import csv
import hashlib
import json
import math
import random
import subprocess
import sys
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, session_date_of  # noqa: E402

HERE = Path(__file__).resolve().parent
LAST = date(2026, 10, 1)
OOS_START = date(2024, 7, 1)
ANNUAL = 252
WEIGHT = -1.0
COST_BPS = 5.0
BORROW = 0.01
SEED = 20261074
TOL_PX = 1e-6
TOL_PNL = 1e-9


def rules_hash():
    lock = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()
    want = lock[0].split()[1]
    have = hashlib.sha256((HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()
    if want != have:
        sys.exit(f"RULES.md hash {have} != RULES.lock {want}; refusing to run")
    return have


def stamp(day, at_close):
    hh = "09:30" if not at_close else ("13:00" if day in EARLY_CLOSES else "16:00")
    return f"{day.isoformat()} {hh}"


def load_book(md, symbol):
    adj = md.bars(symbol, "1d", adjust=True)
    raw = md.bars(symbol, "1d", adjust=False)
    book = []
    bars = {}
    raw_close = {}
    raw_volume = {}
    adj_close = {}
    for b in adj:
        d = session_date_of(b.ts)
        if d > LAST:
            continue
        book.append(d)
        bars[d] = (b.open, b.close)
        adj_close[d] = b.close
    for b in raw:
        d = session_date_of(b.ts)
        if d > LAST:
            continue
        raw_close[d] = b.close
        raw_volume[d] = b.volume
    return book, bars, adj_close, raw_close, raw_volume


def month_end_fills(book, bars):
    """Last session of each calendar month fills at the next session that has a bar."""
    last = {}
    for d in book:
        last[(d.year, d.month)] = d
    pos = {d: i for i, d in enumerate(book)}
    fills = {}
    for key in sorted(last):
        sd = last[key]
        fi = pos[sd] + 1
        while fi < len(book) and book[fi] not in bars:
            fi += 1
        if fi >= len(book):
            continue
        fd = book[fi]
        if fd in fills:
            raise RuntimeError(f"two signals fill on {fd}")
        fills[fd] = sd
    return fills


def replay(book, bars):
    """Naive next-open short. Cash is the source of truth. Weight stays -1."""
    fills = month_end_fills(book, bars)
    first = min(fills)
    pos = {d: i for i, d in enumerate(book)}
    rate = COST_BPS / 10000.0
    cash = 1.0
    shares = 0.0
    last_close = None
    held = False
    active = None
    closed = []
    frozen = False
    ruin_date = None
    days = []

    def finish(day, px, reason, inclusive):
        nonlocal active
        hold = pos[day] - pos[active["entry_date"]] + (1 if inclusive else 0)
        active.update(exit_date=day, exit_px=px, reason=reason, at_close=inclusive, hold=hold)
        closed.append(active)
        active = None

    for day in book:
        if day < first:
            continue
        if frozen:
            days.append((day, 0.0, cash))
            continue
        bar = bars.get(day)
        if bar is None:
            equity = cash if last_close is None else cash + shares * last_close
            days.append((day, 0.0, equity))
            continue
        o, c = bar
        e0 = cash if not held else cash + shares * last_close
        if held and shares != 0.0:
            borrow = abs(shares) * last_close * BORROW / ANNUAL
            cash -= borrow
            active["net"] -= borrow
            gap = shares * (o - last_close)
            active["gross"] += gap
            active["net"] += gap
        e_open = cash + shares * o
        if day in fills and e_open <= 0.0 and shares != 0.0:
            cost = abs(shares) * o * rate
            cash = cash - (-shares) * o - cost
            active["net"] -= cost
            shares = 0.0
            held = False
            finish(day, o, "ruin", False)
            frozen = True
            ruin_date = day
            days.append((day, cash / e0 - 1.0, cash))
            continue
        if day in fills and e_open > 0.0:
            if active is not None:
                finish(day, o, "next_open", False)
            s_new = WEIGHT * e_open / o
            if s_new >= 0.0:
                raise RuntimeError("refusing to open a long")
            delta = s_new - shares
            cost = abs(delta) * o * rate
            cash = cash - delta * o - cost
            shares = s_new
            held = True
            active = {
                "side": "short",
                "entry_date": day,
                "entry_px": o,
                "entry_equity": e_open,
                "gross": 0.0,
                "net": -cost,
                "exit_date": None,
                "exit_px": None,
                "reason": None,
                "hold": None,
                "at_close": False,
            }
        if shares != 0.0:
            oc = shares * (c - o)
            active["gross"] += oc
            active["net"] += oc
        e1 = cash + shares * c
        last_close = c
        if e1 <= 0.0:
            if active is not None:
                finish(day, c, "ruin", True)
            cash = e1
            shares = 0.0
            held = False
            frozen = True
            ruin_date = day
        ret = e1 / e0 - 1.0 if e0 > 0.0 else 0.0
        days.append((day, ret, e1 if not frozen else cash))

    if active is not None:
        finish(book[-1], last_close, "end_of_sample", True)
    return {
        "trips": closed,
        "days": days,
        "first_fill": first,
        "ruined": frozen,
        "ruin_date": ruin_date,
        "terminal_equity": days[-1][2],
    }


def sharpe(rs):
    r = np.asarray(rs, float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0 or math.isnan(sd):
        return None
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def jumps(book, adj_close, raw_close, raw_volume):
    rows = []
    for prev, day in zip(book, book[1:]):
        if prev not in raw_close or day not in raw_close:
            continue
        if raw_close[prev] == 0.0:
            continue
        raw_ratio = raw_close[day] / raw_close[prev]
        adj_ratio = adj_close[day] / adj_close[prev]
        if raw_ratio > 3.0 and adj_ratio > 3.0:
            rows.append({
                "date": day.isoformat(),
                "prev_date": prev.isoformat(),
                "raw_prev_close": raw_close[prev],
                "raw_close": raw_close[day],
                "raw_ratio": raw_ratio,
                "adj_prev_close": adj_close[prev],
                "adj_close": adj_close[day],
                "adj_ratio": adj_ratio,
                "raw_volume_prev": raw_volume.get(prev),
                "raw_volume": raw_volume.get(day),
            })
    return rows


def read_trades():
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def append_log(text):
    with (HERE / "RUNLOG.md").open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    digest = rules_hash()
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = "yes" if subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip() else "no"
    saved = read_trades()
    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    with MarketData() as md:
        vixy = load_book(md, "VIXY")
        uvxy = load_book(md, "UVXY")
    got = replay(vixy[0], vixy[1])
    uv = replay(uvxy[0], uvxy[1])
    bad = []
    if len(got["trips"]) != len(saved):
        bad.append(f"count saved {len(saved)} rebuilt {len(got['trips'])}")
    for a, b in zip(saved, got["trips"]):
        entry = stamp(b["entry_date"], False)
        exit_ = stamp(b["exit_date"], b["at_close"])
        if a["side"] != b["side"] or a["entry_time"] != entry or a["exit_time"] != exit_:
            bad.append(f"times {a['entry_time']} {a['exit_time']} vs {entry} {exit_}")
            continue
        if a["exit_reason"] != b["reason"]:
            bad.append(f"reason {a['entry_time']} {a['exit_reason']} vs {b['reason']}")
        if abs(float(a["entry_px"]) - b["entry_px"]) > TOL_PX or abs(float(a["exit_px"]) - b["exit_px"]) > TOL_PX:
            bad.append(f"price {a['entry_time']}")
        if abs(float(a["net"]) - b["net"]) > TOL_PNL or abs(float(a["gross"]) - b["gross"]) > TOL_PNL:
            bad.append(f"pnl {a['entry_time']} net {a['net']} vs {b['net']}")
        if int(a["hold_sessions"]) != b["hold"]:
            bad.append(f"hold {a['entry_time']} {a['hold_sessions']} vs {b['hold']}")
    rng = random.Random(SEED)
    n = len(got["trips"])
    if n >= 40:
        sample = rng.sample(range(n), 40)
    else:
        sample = rng.choices(range(n), k=40) if n else []
    for i in sample:
        a = saved[i]
        b = got["trips"][i]
        if a["side"] != "short" or abs(float(a["net"]) - b["net"]) > TOL_PNL:
            bad.append(f"seeded row {i}")
    uvxy_oos = [ret for day, ret, _eq in uv["days"] if day >= OOS_START]
    uvxy_sharpe = sharpe(uvxy_oos)
    stored_uvxy = results["uvxy"]["windows"]["oos"]["strategy"]["sharpe"]
    if uvxy_sharpe != stored_uvxy:
        bad.append(f"UVXY OOS Sharpe rebuilt {uvxy_sharpe} stored {stored_uvxy}")
    if got["ruin_date"] is None or got["ruin_date"].isoformat() != results["ruin_date"]:
        bad.append(f"VIXY ruin {got['ruin_date']} stored {results['ruin_date']}")
    if abs(got["terminal_equity"] - results["terminal_equity"]) > 1e-8:
        bad.append(f"VIXY equity {got['terminal_equity']} stored {results['terminal_equity']}")
    if uv["ruin_date"] is None or uv["ruin_date"].isoformat() != results["uvxy"]["ruin_date"]:
        bad.append(f"UVXY ruin {uv['ruin_date']} stored {results['uvxy']['ruin_date']}")
    jump_doc = {
        "note": "Adjusted close ratio above 3 on the same day as the raw close ratio above 3. mdq adjust=True did not remove these jumps. This is not a corrected price series and not a backtest.",
        "VIXY": jumps(vixy[0], vixy[2], vixy[3], vixy[4]),
        "UVXY": jumps(uvxy[0], uvxy[2], uvxy[3], uvxy[4]),
    }
    (HERE / "jumps.json").write_text(json.dumps(jump_doc, indent=2) + "\n", encoding="utf-8", newline="\n")
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    if bad:
        text = (
            f"\n## {now}\n"
            f"- rules_sha256 {digest}\n"
            f"- git_head {head} dirty={dirty}\n"
            f"- reason: verify.py mismatch\n"
            f"- mismatches {len(bad)}; first: {bad[:3]}\n"
        )
        append_log(text)
        print(text)
        for line in bad[:20]:
            print(line)
        sys.exit(1)
    text = (
        f"\n## {now}\n"
        f"- rules_sha256 {digest}\n"
        f"- git_head {head} dirty={dirty}\n"
        f"- reason: verify.py independent replay\n"
        f"- matched {n}/{n} VIXY holdings on side, entry time and price, exit time and price, "
        f"exit reason, hold, gross, and net. Seed {SEED} redrew {len(sample)} indexes "
        f"from a population of {n}. UVXY OOS Sharpe {uvxy_sharpe} matches results.json. "
        f"VIXY ruin {got['ruin_date'].isoformat()}. "
        f"Unadjusted jumps: VIXY {len(jump_doc['VIXY'])}, UVXY {len(jump_doc['UVXY'])}.\n"
    )
    append_log(text)
    print(text)


if __name__ == "__main__":
    main()
