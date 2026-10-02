# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent, deliberately naive re-implementation of the SPY RSI(2) dip-buy.

Shares no code with backtest.py. Rebuilds every session's decision price and
close from 1-minute bars, recomputes Wilder RSI from scratch at every session,
replays every SPY and QQQ trade, and compares them with trades.csv. It also
recomputes the SPY daily net returns and the timing placebo p-value with a
different placement algorithm (rejection sampling), and appends a RUNLOG entry.

    python research/spy-rsi2-dip-buy/research/verify.py
"""
from __future__ import annotations

import csv
import math
import statistics
import sys
from datetime import date, datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, nyse_sessions  # noqa: E402

START, FIRST, END = date(2021, 9, 27), date(2021, 10, 25), date(2026, 9, 25)
COST = {"SPY": 1e-4, "QQQ": 1e-4}


def load(md, sym):
    days = {}
    for b in md.bars(sym, "1m", START, END):
        t = b.time
        days.setdefault(b.session, []).append((t.hour * 60 + t.minute, b.close))
    out = []
    for d in sorted(days):
        rows = sorted(days[d])
        cut = 769 if d in EARLY_CLOSES else 949
        before = [c for m, c in rows if m <= cut]
        out.append((d, before[-1], rows[-1][1]))
    return out


def rsi_from_scratch(closes):
    """Wilder RSI(2) of the full list, recomputed from the seed every time."""
    ch = [closes[k] - closes[k - 1] for k in range(1, len(closes))]
    ag = (max(ch[0], 0) + max(ch[1], 0)) / 2
    al = (max(-ch[0], 0) + max(-ch[1], 0)) / 2
    for x in ch[2:]:
        ag = (ag + max(x, 0)) / 2
        al = (al + max(-x, 0)) / 2
    if al == 0:
        return 50.0 if ag == 0 else 100.0
    return 100 - 100 / (1 + ag / al)


def replay(sess):
    trades, holding, entry = [], False, None
    for i, (d, p, c) in enumerate(sess):
        if d < FIRST:
            continue
        hist = [x[2] for x in sess[:i]]
        if holding:
            if p > (sum(hist[-4:]) + p) / 5:
                trades.append((entry[0], entry[1], d, c, "sma"))
                holding = False
            continue
        if rsi_from_scratch(hist + [p]) < 10:
            holding, entry = True, (d, c)
    if holding:
        trades.append((entry[0], entry[1], sess[-1][0], sess[-1][2], "end"))
    return trades


def daily(sess, trades, cost):
    idx = {d: k for k, (d, _, _) in enumerate(sess)}
    cal = [d for d in nyse_sessions(FIRST, END)]
    r = {d: 0.0 for d in cal}
    for ed, ep, xd, xp, _ in trades:
        i0, i1 = idx[ed], idx[xd]
        r[ed] += -cost
        for k in range(i0 + 1, i1 + 1):
            g = sess[k][2] / sess[k - 1][2]
            r[sess[k][0]] += g * (1 - cost) - 1 if k == i1 else g - 1
    return cal, r


def sharpe(x):
    return statistics.mean(x) / statistics.stdev(x) * math.sqrt(252)


def main():
    with MarketData() as md:
        data = {s: load(md, s) for s in ("SPY", "QQQ")}
    ref = {}
    with open(HERE / "trades.csv") as fh:
        for row in csv.DictReader(fh):
            ref.setdefault(row["symbol"], []).append(row)
    mismatches = 0
    for sym, sess in data.items():
        mine = replay(sess)
        theirs = ref[sym]
        if len(mine) != len(theirs):
            print(f"{sym}: trade count {len(mine)} vs {len(theirs)}")
            mismatches += 1
        for a, b in zip(mine, theirs):
            ok = (str(a[0]) == b["entry_day"] and abs(a[1] - float(b["entry_px"])) < 1e-9
                  and str(a[2]) == b["exit_day"] and abs(a[3] - float(b["exit_px"])) < 1e-9
                  and a[4] == b["exit_reason"] and b["side"] == "long")
            if not ok:
                mismatches += 1
                print(f"{sym} mismatch: {a} vs {b}")
        print(f"{sym}: {len(mine)} trades replayed, {len(theirs)} in trades.csv")
    # Daily returns for SPY against daily.csv.
    sess = data["SPY"]
    trades = replay(sess)
    cal, r = daily(sess, trades, COST["SPY"])
    with open(HERE / "daily.csv") as fh:
        rows = {row["date"]: float(row["strategy_net"]) for row in csv.DictReader(fh)}
    worst = max(abs(r[d] - rows[str(d)]) for d in cal)
    print(f"SPY daily net max abs diff vs daily.csv: {worst:.2e}")
    if worst > 1e-9:
        mismatches += 1
    full = [r[d] for d in cal]
    oos = [r[d] for d in cal if d >= date(2024, 7, 1)]
    print(f"SPY Sharpe full {sharpe(full):.4f}, OOS {sharpe(oos):.4f}")

    # Timing placebo, recomputed with rejection sampling of start sessions.
    import random
    rng = random.Random(20260927)
    idx = {d: k for k, (d, _, _) in enumerate(sess)}
    pos = {d: k for k, d in enumerate(cal)}
    bh = [(sess[idx[d]][2] / sess[idx[d] - 1][2] - 1) if d in idx else 0.0 for d in cal]
    gross = [0.0] * len(cal)
    occ = []
    for ed, ep, xd, xp, _ in trades:
        occ.append(pos[xd] - pos[ed] + 1)
        for k in range(pos[ed] + 1, pos[xd] + 1):
            gross[k] = bh[k]
    actual = sharpe(gross)
    draws, n_draws = [], 2000
    while len(draws) < n_draws:
        order = occ[:]
        rng.shuffle(order)
        taken = [False] * len(cal)
        mask = [0.0] * len(cal)
        ok = True
        for k in order:
            for _ in range(10000):
                s = rng.randrange(0, len(cal) - k + 1)
                if not any(taken[s:s + k]):
                    break
            else:
                ok = False
                break
            for j in range(s, s + k):
                taken[j] = True
            for j in range(s + 1, s + k):
                mask[j] = 1.0
        if ok:
            draws.append(sharpe([b * m for b, m in zip(bh, mask)]))
    p = (1 + sum(x >= actual for x in draws)) / (n_draws + 1)
    print(f"Timing placebo (rejection sampling, seed 20260927): actual {actual:.4f}, p = {p:.4f}")

    status = "all trades match" if mismatches == 0 else f"{mismatches} mismatches"
    entry = (f"\n## {datetime.now(timezone.utc).isoformat(timespec='seconds')} (verify.py)\n\n"
             f"- Reason: independent replay of every SPY and QQQ trade\n"
             f"- Result: {status}; SPY daily max abs diff {worst:.2e}\n"
             f"- SPY Sharpe full / OOS (naive): {sharpe(full):.3f} / {sharpe(oos):.3f}\n"
             f"- Timing placebo p with a different placement method: {p:.4f}\n")
    with open(HERE / "RUNLOG.md", "a") as fh:
        fh.write(entry)
    print(entry)
    if mismatches:
        sys.exit(1)


if __name__ == "__main__":
    main()
