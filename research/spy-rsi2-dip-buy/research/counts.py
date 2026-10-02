# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock data checks and signal counts for the SPY RSI(2) dip-buy study.

Checks that daily and 1-minute sessions line up, how far the stored daily
close sits from the last regular-hours minute, that the 15:49 decision minute
exists, and counts how often the 15:50 proxy RSI(2) is below the entry
thresholds. Closes are built from regular-hours 1-minute bars. It reads no price after a decision minute, simulates no exit, and
computes no return, P&L, or hit rate.

    python research/spy-rsi2-dip-buy/research/counts.py
"""
from __future__ import annotations

import json
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, ny_datetime, nyse_sessions  # noqa: E402

SYMBOLS = ("SPY", "QQQ", "IGV")
START, END = date(2021, 9, 16), date(2026, 9, 25)
IS_END, OOS_START = date(2024, 6, 28), date(2024, 7, 1)
RSI_N = 2
WARMUP_DAILY = 20
SMA_TREND = 200


def decision_minute(day: date) -> int:
    """Minute of day of the decision bar's open: 15:49, or 12:49 on an early close."""
    return (12 if day in EARLY_CLOSES else 15) * 60 + 49


def main() -> None:
    out: dict = {}
    with MarketData() as md:
        for sym in SYMBOLS:
            daily = md.bars(sym, "1d", START, END)
            minutes = md.bars(sym, "1m", START, END)
            d_close = {b.session: b.close for b in daily}
            by_day: dict[date, list] = {}
            for b in minutes:
                by_day.setdefault(b.session, []).append(b)
            cal = nyse_sessions(START, END)
            only_1m = [str(d) for d in cal if d in by_day and d not in d_close]
            only_1d = [str(d) for d in cal if d in d_close and d not in by_day]
            no_decision_bar, proxy_fallback = [], []
            proxy: dict[date, float] = {}
            for d, bars in by_day.items():
                dm = decision_minute(d)
                exact = [b for b in bars if (lambda t: t.hour * 60 + t.minute)(ny_datetime(b.ts)) == dm]
                upto = [b for b in bars if (lambda t: t.hour * 60 + t.minute)(ny_datetime(b.ts)) <= dm]
                if exact:
                    proxy[d] = exact[0].close
                elif upto:
                    proxy[d] = upto[-1].close
                    proxy_fallback.append(str(d))
                else:
                    no_decision_bar.append(str(d))
            # Daily close vs last 1-minute close (data quality, not an outcome).
            gaps = []
            for d, bars in by_day.items():
                if d in d_close:
                    rel = abs(d_close[d] / bars[-1].close - 1)
                    if rel > 5e-4:
                        gaps.append(str(d))
            # Signal counts on the proxy RSI(2), using prior regular-hours closes
            # only. The close of a session is its last 1-minute bar's close.
            days = sorted(by_day)
            closes = [by_day[d][-1].close for d in days]
            ag = al = None
            fires = {"lt10": {"IS": 0, "OOS": 0}, "lt5": {"IS": 0, "OOS": 0},
                     "lt10_above_sma200": {"IS": 0, "OOS": 0}}
            first_eval = None
            sma200_first = None
            for i, d in enumerate(days):
                if ag is not None and i >= WARMUP_DAILY:
                    if first_eval is None:
                        first_eval = d
                    ch = proxy[d] - closes[i - 1]
                    g, l = max(ch, 0.0), max(-ch, 0.0)
                    ag2 = (ag * (RSI_N - 1) + g) / RSI_N
                    al2 = (al * (RSI_N - 1) + l) / RSI_N
                    rsi = 100.0 if al2 == 0 else 100 - 100 / (1 + ag2 / al2)
                    win = "IS" if d <= IS_END else "OOS"
                    if rsi < 10:
                        fires["lt10"][win] += 1
                    if rsi < 5:
                        fires["lt5"][win] += 1
                    if i >= SMA_TREND - 1:
                        if sma200_first is None:
                            sma200_first = d
                        sma = (sum(closes[i - SMA_TREND + 1:i]) + proxy[d]) / SMA_TREND
                        if rsi < 10 and proxy[d] > sma:
                            fires["lt10_above_sma200"][win] += 1
                # Update Wilder averages with today's regular-hours close.
                if i == RSI_N:
                    chs = [closes[k] - closes[k - 1] for k in range(1, RSI_N + 1)]
                    ag = sum(max(c, 0) for c in chs) / RSI_N
                    al = sum(max(-c, 0) for c in chs) / RSI_N
                elif i > RSI_N:
                    ch = closes[i] - closes[i - 1]
                    ag = (ag * (RSI_N - 1) + max(ch, 0)) / RSI_N
                    al = (al * (RSI_N - 1) + max(-ch, 0)) / RSI_N
            gap_by_year: dict[int, int] = {}
            for g in gaps:
                gap_by_year[int(g[:4])] = gap_by_year.get(int(g[:4]), 0) + 1
            gaps_20 = []
            for d, bars in by_day.items():
                if d in d_close and abs(d_close[d] / bars[-1].close - 1) > 2e-3:
                    gaps_20.append(str(d))
            out[sym] = {
                "daily_first": str(min(d_close)), "daily_last": str(max(d_close)), "daily_n": len(d_close),
                "minute_sessions": len(by_day),
                "minute_first": str(min(by_day)), "minute_last": str(max(by_day)),
                "sessions_1m_without_1d": only_1m,
                "sessions_1d_without_1m": only_1d,
                "decision_bar_fallback": proxy_fallback,
                "decision_bar_missing": no_decision_bar,
                "daily_vs_last_minute_close_gt_5bp": len(gaps),
                "daily_vs_last_minute_close_gt_5bp_by_year": gap_by_year,
                "daily_vs_last_minute_close_gt_20bp": len(gaps_20),
                "daily_vs_last_minute_close_gt_20bp_first": min(gaps_20) if gaps_20 else None,
                "first_evaluable_session": str(first_eval),
                "sma200_first_session": str(sma200_first),
                "proxy_rsi_fires": fires,
                "splits": md.corporate_actions(sym),
            }
    path = Path(__file__).resolve().parent / "counts.json"
    path.write_text(json.dumps(out, indent=2, default=str))
    print(json.dumps(out, indent=2, default=str))


if __name__ == "__main__":
    main()
