# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of country-bab. Shares no signal code with backtest.py.

    python research/country-bab/research/verify.py
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import subprocess
import sys
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import MarketData, nyse_sessions  # noqa: E402

UNIVERSE = [
    "EWA", "EWC", "EWG", "EWH", "EWJ", "EWS", "EWU", "EWW", "EWZ", "EWL", "EWT", "EWY",
]
WINDOW = 252
COST = 0.0005
SAMPLE_END = date(2026, 10, 1)
OOS_START = date(2024, 7, 1)
SEED = 20261064


def rules_ok() -> str:
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    got = hashlib.sha256(raw).hexdigest()
    locked = {}
    for line in (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines():
        if line.strip():
            k, v = line.split(" ", 1)
            locked[k] = v.strip()
    if locked.get("sha256") != got:
        raise SystemExit("RULES.md hash does not match RULES.lock; refusing to verify")
    return got


def month_ends(spy_dates: list[date]) -> list[date]:
    have = set(spy_dates)
    last = spy_dates[-1]
    y, m = spy_dates[0].year, spy_dates[0].month
    found = []
    while (y, m) <= (last.year, last.month):
        nxt = date(y + 1, 1, 1) if m == 12 else date(y, m + 1, 1)
        days = nyse_sessions(date(y, m, 1), nxt - timedelta(days=1))
        if days and days[-1] <= last and days[-1] in have:
            found.append(days[-1])
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    return found


def beta_of(name_px: list[float], spy_px: list[float]) -> float | None:
    """OLS slope with an intercept on the last 252 paired simple returns."""
    if len(name_px) < WINDOW + 1:
        return None
    name_px = name_px[-(WINDOW + 1) :]
    spy_px = spy_px[-(WINDOW + 1) :]
    ri = [name_px[i] / name_px[i - 1] - 1.0 for i in range(1, len(name_px))]
    rs = [spy_px[i] / spy_px[i - 1] - 1.0 for i in range(1, len(spy_px))]
    mean_i = sum(ri) / len(ri)
    mean_s = sum(rs) / len(rs)
    den = sum((x - mean_s) ** 2 for x in rs)
    if den == 0.0:
        return None
    num = sum((a - mean_i) * (b - mean_s) for a, b in zip(ri, rs))
    return num / den


def sharpe(rets: list[float]) -> float | None:
    if len(rets) < 2:
        return None
    mu = sum(rets) / len(rets)
    var = sum((x - mu) ** 2 for x in rets) / (len(rets) - 1)
    if var <= 0.0:
        return None
    return mu / math.sqrt(var) * math.sqrt(252.0)


def live_after(fill: str, saved: list[dict]) -> tuple[set[str], set[str]]:
    longs, shorts = set(), set()
    for tr in saved:
        entry, exit_ = tr["entry_date"], tr["exit_date"]
        if entry <= fill and exit_ > fill:
            (longs if tr["side"] == "long" else shorts).add(tr["symbol"])
        elif entry == fill and exit_ == fill:
            (longs if tr["side"] == "long" else shorts).add(tr["symbol"])
    return longs, shorts


def main() -> None:
    digest = rules_ok()
    saved = list(csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")))
    daily = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    with MarketData() as md:
        spy_bars = [b for b in md.bars("SPY", "1d") if b.session <= SAMPLE_END]
        sessions = [b.session for b in spy_bars]
        spy = [float(b.close) for b in spy_bars]
        opens: dict[str, dict[date, float]] = {}
        closes: dict[str, dict[date, float]] = {}
        for sym in UNIVERSE:
            bars = [b for b in md.bars(sym, "1d") if b.session <= SAMPLE_END]
            opens[sym] = {b.session: float(b.open) for b in bars}
            closes[sym] = {b.session: float(b.close) for b in bars}

    signals = month_ends(sessions)
    signal_set = set(signals)
    index = {d: i for i, d in enumerate(sessions)}
    cash = 1.0
    shares = {s: 0.0 for s in UNIVERSE}
    last = {s: None for s in UNIVERSE}
    target = {s: 0.0 for s in UNIVERSE}
    dirty = {s: False for s in UNIVERSE}
    scheduled = None
    started = False
    prev_eq = 1.0
    trips: dict[str, dict] = {}
    closed: list[dict] = []
    nets: list[float] = []
    dates: list[date] = []
    books: dict[date, tuple[set[str], set[str]]] = {}

    def side_of(sh: float) -> int:
        return 1 if sh > 0.0 else (-1 if sh < 0.0 else 0)

    for i, day in enumerate(sessions):
        op, cl, has = {}, {}, {}
        for sym in UNIVERSE:
            if day in opens[sym] and day in closes[sym]:
                has[sym] = True
                op[sym] = opens[sym][day]
                cl[sym] = closes[sym][day]
            else:
                has[sym] = False
        eq_open = cash
        for sym in UNIVERSE:
            if shares[sym] != 0.0:
                eq_open += shares[sym] * (op[sym] if has[sym] else last[sym])
        if scheduled is not None and i >= scheduled:
            filled = False
            for sym in UNIVERSE:
                if not dirty[sym] or not has[sym]:
                    continue
                px = op[sym]
                new_sh = 0.0 if target[sym] == 0.0 else target[sym] * eq_open / px
                old = shares[sym]
                delta = new_sh - old
                if delta != 0.0:
                    cost = abs(delta) * px * COST
                    cash -= delta * px + cost
                    old_side, new_side = side_of(old), side_of(new_sh)
                    if old_side == 0 and new_side != 0:
                        trips[sym] = {
                            "symbol": sym,
                            "side": "long" if new_sh > 0 else "short",
                            "entry_date": day.isoformat(),
                            "entry_price": px,
                        }
                    elif old_side != 0 and new_side == 0:
                        tr = trips.pop(sym)
                        tr["exit_date"] = day.isoformat()
                        tr["exit_price"] = px
                        closed.append(tr)
                    elif old_side != 0 and new_side == -old_side:
                        tr = trips.pop(sym)
                        tr["exit_date"] = day.isoformat()
                        tr["exit_price"] = px
                        closed.append(tr)
                        trips[sym] = {
                            "symbol": sym,
                            "side": "long" if new_sh > 0 else "short",
                            "entry_date": day.isoformat(),
                            "entry_price": px,
                        }
                    shares[sym] = new_sh
                dirty[sym] = False
                filled = True
            if filled:
                started = True
        for sym in UNIVERSE:
            if has[sym]:
                last[sym] = cl[sym]
        eq_close = cash
        for sym in UNIVERSE:
            if shares[sym] != 0.0:
                eq_close += shares[sym] * (cl[sym] if has[sym] else last[sym])
        if day in signal_set:
            scored = []
            for sym in UNIVERSE:
                paired_n, paired_s = [], []
                for j in range(i + 1):
                    d = sessions[j]
                    if d in closes[sym]:
                        paired_n.append(closes[sym][d])
                        paired_s.append(spy[j])
                # The signal date has to be one of the paired closes. A gap is not filled.
                if day not in closes[sym]:
                    continue
                b = beta_of(paired_n, paired_s)
                if b is not None:
                    scored.append((b, sym))
            scored.sort()
            new_target = {s: 0.0 for s in UNIVERSE}
            if len(scored) >= 6:
                for _, sym in scored[:3]:
                    new_target[sym] = 1.0 / 3.0
                for _, sym in scored[-3:]:
                    new_target[sym] = -1.0 / 3.0
            if i + 1 < len(sessions):
                target = new_target
                dirty = {s: shares[s] != 0.0 or target[s] != 0.0 for s in UNIVERSE}
                scheduled = i + 1
                books[sessions[i + 1]] = (
                    {s for s in UNIVERSE if target[s] > 0.0},
                    {s for s in UNIVERSE if target[s] < 0.0},
                )
        if started:
            nets.append(eq_close / prev_eq - 1.0)
            dates.append(day)
            prev_eq = eq_close

    for sym, tr in list(trips.items()):
        tr["exit_date"] = sessions[-1].isoformat() if dates else tr["entry_date"]
        tr["exit_price"] = last[sym]
        closed.append(tr)

    def key(row):
        return (row["symbol"], row["side"], row["entry_date"], row["exit_date"])

    got = sorted(closed, key=key)
    want = sorted(saved, key=key)
    mismatches = []
    if len(got) != len(want):
        mismatches.append(f"trade count {len(got)} != {len(want)}")
    for a, b in zip(got, want):
        if a["symbol"] != b["symbol"] or a["side"] != b["side"]:
            mismatches.append(f"id {a} vs {b['symbol']} {b['side']}")
            continue
        if a["entry_date"] != b["entry_date"] or a["exit_date"] != b["exit_date"]:
            mismatches.append(f"dates {a['symbol']} {a['entry_date']} {a['exit_date']} vs {b['entry_date']} {b['exit_date']}")
            continue
        for field in ("entry_price", "exit_price"):
            if abs(float(a[field]) - float(b[field])) > 1e-6:
                mismatches.append(f"price {a['symbol']} {field} {a[field]} vs {b[field]}")
    if [d.isoformat() for d in dates] != [r["date"] for r in daily]:
        mismatches.append("daily dates differ")
    else:
        for d, r, stored in zip(dates, nets, daily):
            if abs(r - float(stored["strategy_net"])) > 1e-8:
                mismatches.append(f"net {d} {r} vs {stored['strategy_net']}")
                break

    rng = np.random.default_rng(SEED)
    fill_dates = [d for d in books if d >= date(2012, 2, 1)]
    pick = rng.choice(len(fill_dates), size=min(40, len(fill_dates)), replace=False)
    checked = 0
    for k in pick:
        fill = fill_dates[int(k)]
        longs, shorts = books[fill]
        live_l, live_s = live_after(fill.isoformat(), saved)
        checked += 1
        if longs != live_l or shorts != live_s:
            mismatches.append(f"book {fill} long {sorted(longs)} vs {sorted(live_l)} short {sorted(shorts)} vs {sorted(live_s)}")

    oos = [r for d, r in zip(dates, nets) if d >= OOS_START]
    full_s = sharpe(nets)
    oos_s = sharpe(oos)
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
    dirty_git = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, cwd=ROOT).stdout.strip())
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    status = "match" if not mismatches else "MISMATCH"
    log = (
        f"## {now}\n"
        f"- rules_sha256 {digest}\n"
        f"- git_head {head} dirty={'yes' if dirty_git else 'no'}\n"
        f"- reason: verification, independent rebuild of every trade\n"
        f"- {status}: trades {len(got)} vs {len(want)}, signal books checked {checked}, "
        f"recomputed full Sharpe {full_s} OOS Sharpe {oos_s}\n"
    )
    if mismatches:
        log += "- first mismatches: " + " | ".join(mismatches[:8]) + "\n"
    path = HERE / "RUNLOG.md"
    with path.open("a", encoding="utf-8") as f:
        f.write(log)
    print(log)
    if mismatches:
        raise SystemExit(f"verify failed: {len(mismatches)} mismatches")


if __name__ == "__main__":
    main()
