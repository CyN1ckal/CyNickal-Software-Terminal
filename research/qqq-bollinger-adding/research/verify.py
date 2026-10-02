# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the primary QQQ rule, checked against trades.csv.

Shares no code with backtest.py. It builds its own 5-minute bars from mdq's
1-minute bars, computes the bands with explicit loops, and walks each session
with a separate state machine. Every QQQ session is replayed. Every campaign
must match on side, units, leg times and prices, exit time, exit price, and
reason.

    python research/qqq-bollinger-adding/research/verify.py
"""
from __future__ import annotations

import csv
import sys
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, ny_datetime  # noqa: E402

LOOKBACK, WIDTH, ADD_GAP, CAP = 20, 2.0, 1.0, 3


def five_minute_sessions(minute_bars):
    """{session: [(hh:mm, open, close)]} from 1-minute bars, 09:30-anchored buckets."""
    sessions: dict = {}
    for b in minute_bars:
        t = ny_datetime(b.ts)
        mins = t.hour * 60 + t.minute - (9 * 60 + 30)
        if mins < 0 or mins >= 390:
            continue
        bucket = mins // 5 * 5 + 9 * 60 + 30
        key = f"{bucket // 60:02d}:{bucket % 60:02d}"
        day = sessions.setdefault(t.date(), {})
        if key not in day:
            day[key] = [b.open, b.close]
        else:
            day[key][1] = b.close
    return {d: [(k, v[0], v[1]) for k, v in sorted(day.items())] for d, day in sessions.items()}


def to_min(hhmm: str) -> int:
    return int(hhmm[:2]) * 60 + int(hhmm[3:])


def replay(day: date, bars):
    close_min = 13 * 60 if day in EARLY_CLOSES else 16 * 60
    if not bars or bars[0][0] != "09:30" or to_min(bars[-1][0]) < close_min - 5:
        return []
    last_open_allowed = close_min - 30          # an entry/add fill must open before this
    trades = []
    pos = None                                  # dict(side, fills=[(hhmm, px)])
    order = None                                # (bar_position, what)
    for j in range(len(bars)):
        hhmm, o, c = bars[j]
        if order and order[0] == j:
            what = order[1]
            if what == "flat":
                trades.append((pos["side"], pos["fills"], hhmm, o, "middle"))
                pos = None
            elif what == "more":
                pos["fills"].append((hhmm, o))
            else:
                pos = {"side": what, "fills": [(hhmm, o)]}
            order = None
        if j == len(bars) - 1:
            if pos:
                trades.append((pos["side"], pos["fills"], hhmm, c, "session"))
            break
        if order or j < LOOKBACK - 1:
            continue
        window = [bars[x][2] for x in range(j - LOOKBACK + 1, j + 1)]
        mean = 0.0
        for v in window:
            mean += v
        mean /= LOOKBACK
        var = 0.0
        for v in window:
            var += (v - mean) ** 2
        sd = (var / LOOKBACK) ** 0.5
        nxt = bars[j + 1]
        if to_min(nxt[0]) != to_min(hhmm) + 5:
            continue
        can_open = to_min(nxt[0]) < last_open_allowed
        want = None
        if pos is None:
            if c < mean - WIDTH * sd:
                want = "long"
            elif c > mean + WIDTH * sd:
                want = "short"
            if want and not can_open:
                want = None
        elif pos["side"] == "long":
            worst = min(px for _, px in pos["fills"])
            if c >= mean:
                want = "flat"
            elif can_open and len(pos["fills"]) < CAP and c < mean - WIDTH * sd and c <= worst - ADD_GAP * sd:
                want = "more"
        else:
            worst = max(px for _, px in pos["fills"])
            if c <= mean:
                want = "flat"
            elif can_open and len(pos["fills"]) < CAP and c > mean + WIDTH * sd and c >= worst + ADD_GAP * sd:
                want = "more"
        if want:
            order = (j + 1, want)
    return trades


def main() -> None:
    with open(HERE / "trades.csv", newline="", encoding="utf-8") as f:
        expected = list(csv.DictReader(f))
    with MarketData() as md:
        minutes = md.bars("QQQ", "1m", start="2021-09-27", end="2026-09-25")
    sessions = five_minute_sessions(minutes)
    got = []
    for day in sorted(sessions):
        for side, fills, xt, xp, why in replay(day, sessions[day]):
            got.append({
                "session": day.isoformat(), "side": side, "units": str(len(fills)),
                "legs": "|".join(f"{t}@{p:.4f}" for t, p in fills),
                "exit_time": f"{day.isoformat()} {xt}", "exit_price": f"{xp:.6f}", "reason": why})
    keys = ("session", "side", "units", "legs", "exit_time", "exit_price", "reason")
    mismatches = 0
    for i in range(max(len(got), len(expected))):
        a = got[i] if i < len(got) else None
        b = {k: expected[i][k] for k in keys} if i < len(expected) else None
        if a != b:
            mismatches += 1
            if mismatches <= 10:
                print("MISMATCH", i + 1, "\n  verify  ", a, "\n  backtest", b)
    print(f"sessions replayed: {len(sessions)}; campaigns verify {len(got)}, backtest {len(expected)}; "
          f"mismatches {mismatches}")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
