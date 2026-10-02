# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock counts for the QQQ ATR scale-in study.

Counts sessions, bars, and how often a 5-minute close sits at least k grid
steps beyond the session open. It does not read any price after the signal
bar, and it computes no return, P&L, or hit rate.

    python research/qqq-atr-scale-in/research/counts.py
"""
from __future__ import annotations

import json
import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import (  # noqa: E402
    EARLY_CLOSES,
    MarketData,
    ny_datetime,
    resample,
)

BAR_S = 300
ATR_N = 14
SPACING = 0.5
N_LEVELS = 3
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
CUTOFF_BEFORE_END_MIN = 30
SYMBOLS = ("QQQ", "SPY", "IGV")


def atr_at_close(daily) -> list[tuple[date, float]]:
    """Wilder ATR(14) fixed at each daily close, once 14 true ranges exist.

    The pair (session, atr) may be used only on a later session. A session
    with intraday bars but no daily bar takes the latest pair whose session
    is strictly earlier.
    """
    if len(daily) < ATR_N + 1:
        return []
    trs: list[float] = []
    days: list[date] = []
    for i in range(1, len(daily)):
        prev_c = daily[i - 1].close
        h, l = daily[i].high, daily[i].low
        tr = max(h - l, abs(h - prev_c), abs(l - prev_c))
        trs.append(tr)
        days.append(daily[i].session)
    atr = sum(trs[:ATR_N]) / ATR_N
    out = [(days[ATR_N - 1], atr)]
    for j in range(ATR_N, len(trs)):
        atr = ((ATR_N - 1) * atr + trs[j]) / ATR_N
        if atr > 0:
            out.append((days[j], atr))
    return out


def atr_for_sessions(closes: list[tuple[date, float]], sessions: list[date]) -> dict[date, float]:
    """ATR usable on each session: the latest close strictly before that session."""
    out: dict[date, float] = {}
    j = 0
    for day in sessions:
        while j + 1 < len(closes) and closes[j + 1][0] < day:
            j += 1
        if closes and closes[j][0] < day and closes[j][1] > 0:
            out[day] = closes[j][1]
    return out


def minute_of(ts: int) -> int:
    t = ny_datetime(ts)
    return t.hour * 60 + t.minute


def session_end_minute(day: date) -> int:
    return 13 * 60 if day in EARLY_CLOSES else 16 * 60


def summarize(symbol: str, bars_5m, atr_by_day: dict[date, float]) -> dict:
    by_day: dict[date, list] = {}
    for b in bars_5m:
        by_day.setdefault(b.session, []).append(b)

    bar_count = Counter()
    level_closes = {k: {"full": 0, "is": 0, "oos": 0} for k in range(1, N_LEVELS + 1)}
    sessions_hit = {k: {"full": 0, "is": 0, "oos": 0} for k in range(1, N_LEVELS + 1)}
    side_sessions = {"long": {"full": 0, "is": 0, "oos": 0}, "short": {"full": 0, "is": 0, "oos": 0}}
    n_eval = {"full": 0, "is": 0, "oos": 0}
    skipped_no_open = 0
    skipped_short = 0
    noncontiguous = 0
    first_eval = None
    last_eval = None

    for day in sorted(by_day):
        if day not in atr_by_day:
            continue
        bars = by_day[day]
        bar_count[len(bars)] += 1
        if bars[0].time.hour != 9 or bars[0].time.minute != 30:
            skipped_no_open += 1
            continue
        end_m = session_end_minute(day)
        last_open_m = minute_of(bars[-1].ts)
        need = 12 * 60 + 55 if day in EARLY_CLOSES else 15 * 60 + 55
        if last_open_m < need:
            skipped_short += 1
            continue
        window = "is" if day <= IS_END else "oos" if day >= OOS_START else "other"
        if window == "other":
            continue
        n_eval["full"] += 1
        n_eval[window] += 1
        first_eval = day if first_eval is None else first_eval
        last_eval = day
        opens = {b.ts for b in bars}
        session_open = bars[0].open
        atr = atr_by_day[day]
        hit_level = {k: False for k in range(1, N_LEVELS + 1)}
        hit_side = {"long": False, "short": False}
        for b in bars[:-1]:
            fill_ts = b.ts + BAR_S
            if fill_ts not in opens:
                noncontiguous += 1
                continue
            if minute_of(fill_ts) >= end_m - CUTOFF_BEFORE_END_MIN:
                continue
            for k in range(1, N_LEVELS + 1):
                band = k * SPACING * atr
                if b.close <= session_open - band or b.close >= session_open + band:
                    level_closes[k]["full"] += 1
                    level_closes[k][window] += 1
                    hit_level[k] = True
                if k == 1 and b.close <= session_open - band:
                    hit_side["long"] = True
                if k == 1 and b.close >= session_open + band:
                    hit_side["short"] = True
        for k in range(1, N_LEVELS + 1):
            if hit_level[k]:
                sessions_hit[k]["full"] += 1
                sessions_hit[k][window] += 1
        for side in ("long", "short"):
            if hit_side[side]:
                side_sessions[side]["full"] += 1
                side_sessions[side][window] += 1

    return {
        "symbol": symbol,
        "first_session_with_prior_atr": first_eval.isoformat() if first_eval else None,
        "last_evaluated_session": last_eval.isoformat() if last_eval else None,
        "evaluated_sessions": n_eval,
        "five_minute_bar_count_histogram": {str(k): bar_count[k] for k in sorted(bar_count)},
        "skipped_no_0930_bar": skipped_no_open,
        "skipped_last_bar_too_early": skipped_short,
        "decision_bars_with_no_contiguous_fill_bar": noncontiguous,
        "closes_at_or_beyond_level": level_closes,
        "sessions_with_a_legal_close_at_or_beyond_level": sessions_hit,
        "sessions_with_a_legal_level1_close_by_side": side_sessions,
        "note": (
            "A level-k count is a same-bar comparison of that bar's close with "
            "the session open and the prior-close ATR. No later price is read. "
            "Sessions with a legal level-1 close are a lower bound on campaigns, "
            "because each session starts flat and the first such close schedules "
            "an entry. This script does not apply the one-position rule, so the "
            "close counts overstate entries."
        ),
    }


def main() -> None:
    out = {"spacing": SPACING, "atr_n": ATR_N, "bar_seconds": BAR_S, "symbols": {}}
    with MarketData() as md:
        for symbol in SYMBOLS:
            daily = md.bars(symbol, "1d")
            bars_1m = md.bars(symbol, "1m")
            bars_5m = resample(bars_1m, BAR_S)
            intraday_days = sorted({b.session for b in bars_5m})
            closes = atr_at_close(daily)
            atr = atr_for_sessions(closes, intraday_days)
            cov = md.coverage_summary(symbol, "1m")
            actions = md.corporate_actions(symbol)
            notes = Counter((p.get("note") or "")[:80] for p in cov["needs_attention"])
            out["symbols"][symbol] = {
                "coverage_1m": {
                    "first": cov["first_session"],
                    "last": cov["last_session"],
                    "sessions_recorded": cov["sessions_recorded"],
                    "sessions_with_bars": cov["sessions_with_bars"],
                    "by_status": cov["by_status"],
                    "needs_attention_count": len(cov["needs_attention"]),
                    "needs_attention_notes": dict(notes),
                    "sessions_not_recorded": cov["sessions_not_recorded"],
                },
                "corporate_actions": actions,
                "daily_first": daily[0].session.isoformat() if daily else None,
                "daily_last": daily[-1].session.isoformat() if daily else None,
                "daily_bars": len(daily),
                "intraday_first": intraday_days[0].isoformat() if intraday_days else None,
                "intraday_last": intraday_days[-1].isoformat() if intraday_days else None,
                "one_minute_bars": len(bars_1m),
                "five_minute_bars": len(bars_5m),
                "atr_seed_session": closes[0][0].isoformat() if closes else None,
                "sessions_with_usable_prior_atr": len(atr),
                "signals": summarize(symbol, bars_5m, atr),
            }
    text = json.dumps(out, indent=2, default=str)
    path = Path(__file__).resolve().parent / "counts.json"
    path.write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
