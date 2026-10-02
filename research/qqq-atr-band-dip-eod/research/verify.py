# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent, deliberately naive re-implementation.

Shares no signal code with backtest.py. Recomputes Wilder ATR(14) from scratch,
finds the band touch by a different scan, and rebuilds each QQQ trade.
Replays EVERY evaluation session and compares side, entry time and price, exit
time and price, and reason against trades.csv. Aborts on any mismatch.
Read-only."""

import datetime as dt
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, ny_datetime, nyse_sessions  # noqa: E402

FIRST, LAST, IS_END = dt.date(2021, 10, 7), dt.date(2026, 9, 25), dt.date(2024, 6, 28)
K, COST_BP = 1.0, 1.0


def naive_atr_series(daily):
    """Wilder ATR(14) but computed with an explicit running list (slower, independent path)."""
    days, atrs = [], []
    trs = []
    for i, b in enumerate(daily):
        if i == 0:
            tr = b.high - b.low
        else:
            pc = daily[i - 1].close
            tr = max(b.high - b.low, abs(b.high - pc), abs(b.low - pc))
        if not atrs:
            trs.append(tr)
            if len(trs) == 14:
                atrs.append(sum(trs) / 14.0)
                days.append(b.session)
        else:
            atrs.append((13.0 * atrs[-1] + tr) / 14.0)
            days.append(b.session)
    return list(zip(days, atrs))


def prior_atr(series, day):
    val = None
    for d, a in series:
        if d < day:
            val = a
        else:
            break
    return val


def main():
    # read backtest.py's trades
    existing = {}
    with open(HERE / "trades.csv") as f:
        head = f.readline().strip().split(",")
        for line in f:
            parts = line.rstrip("\n").split(",")
            row = dict(zip(head, parts))
            existing[row["session"]] = row

    sessions = nyse_sessions(FIRST, LAST)
    rebuilt = {}
    with MarketData() as md:
        daily = md.bars("QQQ", "1d")
        series = naive_atr_series(daily)
        for d in sessions:
            bars = md.bars("QQQ", "1m", start=d, end=d)
            if not bars:
                continue
            A = prior_atr(series, d)
            if A is None or A <= 0:
                continue
            S = bars[0].open
            B = S - K * A
            fill_i = -1
            for i in range(len(bars)):
                if bars[i].low <= B:
                    fill_i = i
                    break
            if fill_i < 0:
                continue
            entry_px = min(B, bars[fill_i].open)
            rebuilt[str(d)] = dict(
                side="long",
                entry_time=str(ny_datetime(bars[fill_i].ts))[:16],
                entry_px=entry_px,
                exit_time=str(ny_datetime(bars[-1].ts))[:16],
                exit_px=bars[-1].close,
                reason="session")

    n_match = n_mismatch = 0
    only_verify, only_back = [], []
    for s in set(rebuilt) | set(existing):
        v, e = rebuilt.get(s), existing.get(s)
        if v is None:
            only_back.append(s)
            n_mismatch += 1
            continue
        if e is None:
            only_verify.append(s)
            n_mismatch += 1
            continue
        ok = (v["side"] == e["side"]
              and v["entry_time"] == e["entry_time"][:16]
              and abs(v["entry_px"] - float(e["entry_px"])) < 5e-7
              and v["exit_time"] == e["exit_time"][:16]
              and abs(v["exit_px"] - float(e["exit_px"])) < 5e-7
              and v["reason"] == e["reason"])
        if ok:
            n_match += 1
        else:
            n_mismatch += 1
            print(f"MISMATCH {s}: verify={v} backtest={e}")
    print(f"verify: sessions replayed={len(sessions)}, trades rebuilt={len(rebuilt)}, "
          f"matched={n_match}, mismatched={n_mismatch}")
    print(f"only in backtest ({len(only_back)}): {only_back[:5]}  "
          f"only in verify ({len(only_verify)}): {only_verify[:5]}")
    assert n_match >= 40, "fewer than 40 matched trades"
    if n_mismatch or only_back or only_verify:
        sys.exit("VERIFY FAILED: trade mismatch")
    print("VERIFY PASSED: every sampled session matches on side, entry time/price, exit time/price, reason")


if __name__ == "__main__":
    main()
