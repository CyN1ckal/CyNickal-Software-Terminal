# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Naive reimplementation of the commodity-ETF momentum book.

Shares no signal code with backtest.py. Matches every trade in trades.csv
on symbol, side, entry and exit date and price, and exit reason.
"""

import bisect
import csv
import datetime as dt
import hashlib
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

NAMES = ("GLD", "SLV", "USO", "UNG", "DBA", "DBB")
COST_BP = {"GLD": 1.0, "SLV": 1.0, "USO": 5.0, "UNG": 5.0, "DBA": 5.0, "DBB": 5.0}
K = 252
BOOK = 2
SKIP = {dt.date(2012, 10, 29), dt.date(2012, 10, 30), dt.date(2018, 12, 5)}
FIRST = dt.date(2011, 1, 4)
LAST = dt.date(2026, 10, 1)
OOS_START = dt.date(2024, 7, 1)
SEED = 20261024


def require_lock() -> str:
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    have = hashlib.sha256(raw).hexdigest()
    want = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()[0].split()[1]
    if have != want:
        sys.exit(f"RULES.md hash {have} != RULES.lock {want}; refusing to run")
    return have


def sgn(x: float) -> int:
    if x > 0.0:
        return 1
    if x < 0.0:
        return -1
    return 0


def main() -> None:
    digest = require_lock()
    opens = {s: {} for s in NAMES}
    closes = {s: {} for s in NAMES}
    own_dates = {s: [] for s in NAMES}
    own_closes = {s: [] for s in NAMES}
    with MarketData() as md:
        for s in NAMES:
            for bar in md.bars(s, "1d", start=FIRST, end=LAST):
                opens[s][bar.session] = bar.open
                closes[s][bar.session] = bar.close
        spy_days = {b.session for b in md.bars("SPY", "1d", start=FIRST, end=LAST)}
    sessions = [d for d in nyse_sessions(FIRST, LAST) if d not in SKIP and d in spy_days]

    for s in NAMES:
        own_dates[s] = sorted(closes[s])
        own_closes[s] = [closes[s][d] for d in own_dates[s]]

    def form(sym, day):
        dates = own_dates[sym]
        i = bisect.bisect_right(dates, day) - 1
        if i < K:
            return None
        return own_closes[sym][i] / own_closes[sym][i - K] - 1.0

    def rank(day):
        values = {s: form(s, day) for s in NAMES}
        if any(values[s] is None for s in NAMES):
            return None
        order = sorted(NAMES, key=lambda s: (-values[s], s))
        w = {s: 0.0 for s in NAMES}
        for i in range(BOOK):
            w[order[i]] = 0.5
            w[order[-1 - i]] = -0.5
        return w

    ends = {}
    for day in sessions:
        ends[(day.year, day.month)] = day
    signals = [ends[k] for k in sorted(ends)]
    first = next(day for day in signals if rank(day) is not None)
    sessions = [d for d in sessions if d >= first]
    signals = [d for d in signals if d >= first]
    signal_set = set(signals)
    final = sessions[-1]

    cash = 1.0
    shares = {s: 0.0 for s in NAMES}
    mark = {s: None for s in NAMES}
    pending = {}
    live = {s: None for s in NAMES}
    done = []
    rate = {s: COST_BP[s] / 10000.0 for s in NAMES}

    def shut(sym, price, day, reason, cost):
        tr = live[sym]
        tr["cost"] += cost
        tr["exit_date"] = day
        tr["exit_price"] = price
        tr["exit_reason"] = reason
        tr["exit_shares"] = shares[sym]
        done.append(tr)
        live[sym] = None

    def deal(sym, target, price, day, end_reason):
        nonlocal cash
        old = shares[sym]
        if old == target:
            return
        delta = target - old
        cost = abs(delta) * price * rate[sym]
        cash -= delta * price + cost
        old_sgn = sgn(old)
        new_sgn = sgn(target)
        if old_sgn == 0 and new_sgn != 0:
            live[sym] = {
                "symbol": sym,
                "side": "long" if new_sgn > 0 else "short",
                "entry_date": day,
                "entry_price": price,
                "entry_shares": target,
                "gross": 0.0,
                "cost": cost,
            }
            shares[sym] = target
        elif old_sgn != 0 and new_sgn == 0:
            shut(sym, price, day, end_reason or "flat", cost)
            shares[sym] = 0.0
        elif old_sgn == new_sgn:
            live[sym]["cost"] += cost
            shares[sym] = target
        else:
            closed_n = abs(old) * price
            opened_n = abs(target) * price
            live[sym]["cost"] += cost * closed_n / (closed_n + opened_n)
            shut(sym, price, day, "flip", 0.0)
            live[sym] = {
                "symbol": sym,
                "side": "long" if new_sgn > 0 else "short",
                "entry_date": day,
                "entry_price": price,
                "entry_shares": target,
                "gross": 0.0,
                "cost": cost * opened_n / (closed_n + opened_n),
            }
            shares[sym] = target

    started = False
    for day in sessions:
        gap_pnl = {s: 0.0 for s in NAMES}
        eq_open = cash
        for s in NAMES:
            if day in opens[s] and mark[s] is not None and shares[s] != 0.0:
                gap_pnl[s] = shares[s] * (opens[s][day] - mark[s])
            if shares[s] != 0.0:
                px = opens[s][day] if day in opens[s] else mark[s]
                eq_open += shares[s] * px
            if live[s] is not None and gap_pnl[s] != 0.0:
                live[s]["gross"] += gap_pnl[s]

        for s in list(pending):
            if day not in opens[s]:
                continue
            pending[s]["seen"] += 1
            if pending[s]["seen"] < 1:
                continue
            info = pending.pop(s)
            deal(s, info["w"] * eq_open / opens[s][day], opens[s][day], day, None)
            started = True

        for s in NAMES:
            if day not in opens[s]:
                continue
            oc = shares[s] * (closes[s][day] - opens[s][day])
            if live[s] is not None and oc != 0.0:
                live[s]["gross"] += oc
            mark[s] = closes[s][day]

        if day in signal_set and day != final:
            weights = rank(day)
            if weights is not None:
                pending = {s: {"w": weights[s], "seen": 0} for s in NAMES}

        if day == final:
            for s in NAMES:
                if shares[s] != 0.0:
                    deal(s, 0.0, mark[s], day, "end")

    if any(live[s] is not None for s in NAMES):
        sys.exit("verify left a trade open")

    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        stored = list(csv.DictReader(handle))
    got = sorted(done, key=lambda t: (t["entry_date"], t["symbol"], t["side"]))
    if len(got) != len(stored):
        sys.exit(f"verify trade count {len(got)} != csv {len(stored)}")

    mismatches = []
    for i, (a, b) in enumerate(zip(got, stored)):
        checks = [
            a["symbol"] == b["symbol"],
            a["side"] == b["side"],
            a["entry_date"].isoformat() == b["entry_date"],
            a["exit_date"].isoformat() == b["exit_date"],
            a["exit_reason"] == b["exit_reason"],
            abs(a["entry_price"] - float(b["entry_price"])) <= 1e-6,
            abs(a["exit_price"] - float(b["exit_price"])) <= 1e-6,
            abs(a["gross"] - float(b["gross_pnl"])) <= 1e-6,
            abs((a["gross"] - a["cost"]) - float(b["net_pnl"])) <= 1e-6,
        ]
        if not all(checks):
            mismatches.append((i, a["symbol"], a["entry_date"], b["symbol"], b["entry_date"], a["exit_reason"], b["exit_reason"]))
            if len(mismatches) >= 8:
                break
    if mismatches:
        for row in mismatches:
            print("mismatch", row)
        sys.exit(f"verify mismatches: {len(mismatches)} shown, lists differ")

    rebalance_dates = sorted({t["entry_date"] for t in got})
    rng = np.random.default_rng(SEED)
    take = min(40, len(rebalance_dates))
    picked = set(rng.choice(len(rebalance_dates), size=take, replace=False).tolist())
    sample_dates = {rebalance_dates[i] for i in picked}
    sample_n = sum(1 for t in got if t["entry_date"] in sample_dates)
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True).stdout.strip())
    when = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    log = HERE / "RUNLOG.md"
    with log.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(
            f"\n## {when}\n"
            f"- rules_sha256 {digest}\n"
            f"- git_head {head} dirty={'yes' if dirty else 'no'}\n"
            f"- reason: verification, independent rebuild of every trade\n"
            f"- verify matched {len(got)}/{len(stored)} trades on symbol, side, entry and exit "
            f"date and price, and exit reason. Seed {SEED} drew {take} entry dates "
            f"covering {sample_n} of those trades; the pass condition is the full list. "
            f"OOS entries {sum(1 for t in got if t['entry_date'] >= OOS_START)}.\n"
        )
    print(f"verify matched {len(got)} trades, including a {take}-date draw of {sample_n} trades")


if __name__ == "__main__":
    main()
