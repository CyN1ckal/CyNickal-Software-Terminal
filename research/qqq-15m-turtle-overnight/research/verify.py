# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent, deliberately naive replay of the locked rule.

Builds its own 15-minute bars from stored 1-minute bars, recomputes every
indicator with plain loops, walks the position bar by bar, and compares each
trade with trades.csv on side, entry time/price, exit time/price, and reason.
Shares no signal code with backtest.py. Run from the repo root:

    python research/qqq-15m-turtle-overnight/research/verify.py
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, ny_datetime  # noqa: E402

HERE = Path(__file__).resolve().parent
SYMBOLS = ("QQQ", "SPY", "IGV")
ENTRY_LEN, EXIT_LEN, ATR_LEN, STOP_K, WARM = 55, 20, 20, 2.0, 20
LAST = "2026-09-25"


def build_15m(minutes):
    buckets = {}
    order = []
    for m in minutes:
        t = ny_datetime(m.ts)
        mins = t.hour * 60 + t.minute - (9 * 60 + 30)
        if mins < 0 or mins >= 390:
            continue
        key = (t.date(), mins // 15)
        if key not in buckets:
            # label the bucket by its aligned open (09:30 + 15k), not by its first print
            buckets[key] = {"ts": m.ts - (mins % 15) * 60 - t.second, "day": t.date(),
                            "o": m.open, "h": m.high, "l": m.low, "c": m.close}
            order.append(key)
        else:
            b = buckets[key]
            b["h"] = max(b["h"], m.high)
            b["l"] = min(b["l"], m.low)
            b["c"] = m.close
    return [buckets[k] for k in order]


def replay(bars):
    n = len(bars)
    days = []
    for b in bars:
        if not days or days[-1] != b["day"]:
            days.append(b["day"])
    start_day = days[WARM]
    start = next(i for i, b in enumerate(bars) if b["day"] == start_day)

    atr = [None] * n
    trs = []
    for i, b in enumerate(bars):
        if i == 0:
            tr = b["h"] - b["l"]
        else:
            pc = bars[i - 1]["c"]
            tr = max(b["h"] - b["l"], abs(b["h"] - pc), abs(b["l"] - pc))
        trs.append(tr)
        if i == ATR_LEN - 1:
            atr[i] = sum(trs) / ATR_LEN
        elif i >= ATR_LEN:
            atr[i] = (atr[i - 1] * (ATR_LEN - 1) + tr) / ATR_LEN

    def hi(i, k):
        return max(bars[j]["h"] for j in range(i - k, i))

    def lo(i, k):
        return min(bars[j]["l"] for j in range(i - k, i))

    trades = []
    side = None          # "long" / "short" / None
    entry = None
    order = None         # (action, reason, new_side, atr)
    for i in range(start, n):
        b = bars[i]
        if order is not None:
            action, reason, new_side, a = order
            if action in ("exit", "flip"):
                entry.update(exit_i=i, exit_px=b["o"], exit_at="open", reason=reason)
                trades.append(entry)
                side, entry = None, None
            if action in ("enter", "flip"):
                px = b["o"]
                stop = px - STOP_K * a if new_side == "long" else px + STOP_K * a
                side, entry = new_side, {"side": new_side, "entry_i": i, "entry_px": px, "entry_at": "open", "stop": stop}
            order = None
        c = b["c"]
        if side is None:
            if c > hi(i, ENTRY_LEN):
                order = ("enter", None, "long", atr[i])
            elif c < lo(i, ENTRY_LEN):
                order = ("enter", None, "short", atr[i])
        elif side == "long":
            reason = "stop" if c <= entry["stop"] else ("channel" if c < lo(i, EXIT_LEN) else None)
            if reason:
                order = ("flip", reason, "short", atr[i]) if c < lo(i, ENTRY_LEN) else ("exit", reason, None, atr[i])
        else:
            reason = "stop" if c >= entry["stop"] else ("channel" if c > hi(i, EXIT_LEN) else None)
            if reason:
                order = ("flip", reason, "long", atr[i]) if c > hi(i, ENTRY_LEN) else ("exit", reason, None, atr[i])
    last = n - 1
    if order is not None and order[0] in ("exit", "flip") and side is not None:
        entry.update(exit_i=last, exit_px=bars[last]["c"], exit_at="close", reason=order[1])
        trades.append(entry)
        side = None
    if side is not None:
        entry.update(exit_i=last, exit_px=bars[last]["c"], exit_at="close", reason="end")
        trades.append(entry)
    return trades


def main() -> None:
    expected = {s: [] for s in SYMBOLS}
    with open(HERE / "trades.csv", newline="") as fh:
        for row in csv.DictReader(fh):
            expected[row["symbol"]].append(row)
    ok = True
    with MarketData() as md:
        for sym in SYMBOLS:
            bars = build_15m(md.bars(sym, "1m", end=LAST))
            got = replay(bars)
            want = expected[sym]
            mism = 0
            if len(got) != len(want):
                print(f"{sym}: trade count {len(got)} vs {len(want)}")
                mism += 1
            for g, w in zip(got, want):
                gs = (g["side"], ny_datetime(bars[g["entry_i"]]["ts"]).strftime("%Y-%m-%d %H:%M"), g["entry_at"],
                      round(g["entry_px"], 6), ny_datetime(bars[g["exit_i"]]["ts"]).strftime("%Y-%m-%d %H:%M"),
                      g["exit_at"], round(g["exit_px"], 6), g["reason"])
                ws = (w["side"], w["entry_time"], w["entry_at"], round(float(w["entry_price"]), 6), w["exit_time"],
                      w["exit_at"], round(float(w["exit_price"]), 6), w["reason"])
                if gs != ws:
                    mism += 1
                    if mism <= 5:
                        print(f"{sym} mismatch:\n  verify   {gs}\n  backtest {ws}")
            print(f"{sym}: {len(got)} trades replayed, {len(want)} in trades.csv, mismatches {mism}")
            ok &= mism == 0
    print("VERIFY PASSED" if ok else "VERIFY FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
