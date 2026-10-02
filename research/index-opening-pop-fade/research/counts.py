# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock data checks and signal counts for the index opening-pop fade study.

For SPY, QQQ, and IGV: checks that each session has a 09:30 bar, a bar before
each decision time, a bar at or after it, and a last bar at 15:59 (12:59 on an
early close). Counts how often the opening pop (09:30 open -> close of the last
minute before the decision time) is at least z trailing RMS units, by window.

It reads no price after the latest decision time (10:30) except to confirm the
last bar's timestamp. It computes no return after a signal, no P&L, and no hit
rate.

    python research/index-opening-pop-fade/research/counts.py
"""
from __future__ import annotations

import json
import math
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, ny_datetime, nyse_sessions  # noqa: E402

SYMBOLS = ("SPY", "QQQ", "IGV")
START, END = date(2021, 9, 27), date(2026, 9, 25)
IS_END = date(2024, 6, 28)
LOOKBACK = 60
DECISIONS = {"0945": 9 * 60 + 45, "1000": 10 * 60, "1030": 10 * 60 + 30}
ZS = (0.5, 0.75, 1.0, 1.5, 2.0)


def mod(ts: int) -> int:
    t = ny_datetime(ts)
    return t.hour * 60 + t.minute


def main() -> None:
    out: dict = {}
    cal = nyse_sessions(START, END)
    with MarketData() as md:
        for sym in SYMBOLS:
            by_day: dict[date, list] = {}
            for b in md.bars(sym, "1m", START, END):
                by_day.setdefault(b.session, []).append(b)
            no_bars = [str(d) for d in cal if d not in by_day]
            no_0930, bad_last, missing_decision_bar = [], [], {k: [] for k in DECISIONS}
            pops: dict[str, dict[date, float]] = {k: {} for k in DECISIONS}
            gap_pop: dict[date, float] = {}
            prev_close = None
            for d in cal:
                bars = sorted(by_day.get(d, []), key=lambda b: b.ts)
                if not bars:
                    continue
                last_expected = (12 if d in EARLY_CLOSES else 15) * 60 + 59
                if mod(bars[-1].ts) != last_expected:
                    bad_last.append(str(d))
                if mod(bars[0].ts) != 9 * 60 + 30:
                    no_0930.append(str(d))
                else:
                    o = bars[0].open
                    for k, dm in DECISIONS.items():
                        before = [b for b in bars if mod(b.ts) < dm]
                        after = [b for b in bars if mod(b.ts) >= dm]
                        if not before or not after:
                            missing_decision_bar[k].append(str(d))
                            continue
                        if mod(before[-1].ts) != dm - 1 or mod(after[0].ts) != dm:
                            missing_decision_bar[k].append(str(d) + " (fallback)")
                        pops[k][d] = before[-1].close / o - 1
                        if k == "1000" and prev_close is not None:
                            gap_pop[d] = before[-1].close / prev_close - 1
                prev_close = bars[-1].close

            def count(series: dict[date, float]) -> dict:
                days = sorted(series)
                res = {f"z{z}": {"IS": 0, "OOS": 0} for z in ZS}
                first_eval = None
                for i in range(LOOKBACK, len(days)):
                    d = days[i]
                    rms = math.sqrt(sum(series[x] ** 2 for x in days[i - LOOKBACK:i]) / LOOKBACK)
                    if first_eval is None:
                        first_eval = d
                    w = "IS" if d <= IS_END else "OOS"
                    for z in ZS:
                        if series[d] >= z * rms:
                            res[f"z{z}"][w] += 1
                n_is = sum(1 for d in days[LOOKBACK:] if d <= IS_END)
                return {"first_evaluable": str(first_eval), "sessions_IS": n_is,
                        "sessions_OOS": len(days) - LOOKBACK - n_is, "fires": res}

            out[sym] = {
                "sessions_calendar": len(cal),
                "sessions_without_bars": no_bars,
                "sessions_without_0930_bar": no_0930,
                "sessions_last_bar_not_1559_or_1259": bad_last,
                "decision_bar_missing_or_fallback": missing_decision_bar,
                "splits": md.corporate_actions(sym),
                "open_pop": {k: count(v) for k, v in pops.items()},
                "close_to_1000_pop": count(gap_pop),
            }
    path = Path(__file__).resolve().parent / "counts.json"
    path.write_text(json.dumps(out, indent=2, default=str))
    print(json.dumps(out, indent=2, default=str))


if __name__ == "__main__":
    main()
