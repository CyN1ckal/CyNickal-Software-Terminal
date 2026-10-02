# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock counts for the QQQ Bollinger reversion-with-adding study.

Counts sessions, bars, and how often a 5-minute close sits outside a
session-local Bollinger band. Everything is a same-bar comparison: the close
of bar i against the mean and population SD of the last N closes of the same
session, ending at bar i. No price after the signal bar is read, and no
return, P&L, forward return, or hit rate is computed.

    python research/qqq-bollinger-adding/research/counts.py
"""
from __future__ import annotations

import json
import math
import statistics
import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, ny_datetime, resample  # noqa: E402

BAR_S = 300
N = 20
K = 2.0
DEPTHS = (2.0, 3.0, 4.0)       # same-bar |z| thresholds, descriptive only
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
CUTOFF_MIN = 30
SYMBOLS = ("QQQ", "SPY", "IGV")


def minute_of(ts: int) -> int:
    t = ny_datetime(ts)
    return t.hour * 60 + t.minute


def summarize(bars_5m) -> dict:
    by_day: dict[date, list] = {}
    for b in bars_5m:
        by_day.setdefault(b.session, []).append(b)

    hist = Counter()
    n_eval = {"is": 0, "oos": 0}
    not_tradable = []
    missing_next = 0
    closes_beyond = {str(d): {"is": 0, "oos": 0} for d in DEPTHS}
    sessions_beyond = {str(d): {"is": 0, "oos": 0} for d in DEPTHS}
    side_sessions = {"long": {"is": 0, "oos": 0}, "short": {"is": 0, "oos": 0}}
    halfwidth_bp = {"is": [], "oos": []}   # 2 sigma / middle, at K-band breaks
    first_decision_minute = Counter()

    for day in sorted(by_day):
        bars = by_day[day]
        hist[len(bars)] += 1
        end_m = 13 * 60 if day in EARLY_CLOSES else 16 * 60
        if minute_of(bars[0].ts) != 9 * 60 + 30 or minute_of(bars[-1].ts) < end_m - 5:
            not_tradable.append(day.isoformat())
            continue
        w = "is" if day <= IS_END else "oos"
        n_eval[w] += 1
        ts_set = {b.ts for b in bars}
        hit = {str(d): False for d in DEPTHS}
        side_hit = {"long": False, "short": False}
        for i in range(N - 1, len(bars) - 1):
            window = [b.close for b in bars[i - N + 1: i + 1]]
            if i == N - 1:
                first_decision_minute[minute_of(bars[i].ts) + 5] += 1
            nxt = bars[i].ts + BAR_S
            if nxt not in ts_set:
                missing_next += 1
                continue
            if minute_of(nxt) >= end_m - CUTOFF_MIN:
                continue
            mu = sum(window) / N
            sd = math.sqrt(sum((x - mu) ** 2 for x in window) / N)
            if sd <= 0:
                continue
            c = bars[i].close
            z = (c - mu) / sd
            for d in DEPTHS:
                if abs(z) > d:
                    closes_beyond[str(d)][w] += 1
                    hit[str(d)] = True
            if z < -K:
                side_hit["long"] = True
            if z > K:
                side_hit["short"] = True
            if abs(z) > K:
                halfwidth_bp[w].append(K * sd / mu * 1e4)
        for d in DEPTHS:
            if hit[str(d)]:
                sessions_beyond[str(d)][w] += 1
        for s in side_hit:
            if side_hit[s]:
                side_sessions[s][w] += 1

    def med(xs):
        return round(statistics.median(xs), 2) if xs else None

    return {
        "tradable_sessions": n_eval,
        "not_tradable": not_tradable,
        "five_minute_bar_histogram": {str(k): hist[k] for k in sorted(hist)},
        "first_decision_close_ny_minute": {f"{m // 60:02d}:{m % 60:02d}": c for m, c in sorted(first_decision_minute.items())},
        "decision_bars_without_next_bucket": missing_next,
        "legal_closes_with_abs_z_above": closes_beyond,
        "sessions_with_a_legal_close_abs_z_above": sessions_beyond,
        "sessions_with_a_legal_2sigma_break_by_side": side_sessions,
        "median_band_halfwidth_bp_at_2sigma_breaks": {k: med(v) for k, v in halfwidth_bp.items()},
        "max_abs_z_possible": round((N - 1) / math.sqrt(N), 4),
    }


def main() -> None:
    out = {"n": N, "k": K, "bar_seconds": BAR_S, "symbols": {}}
    with MarketData() as md:
        for sym in SYMBOLS:
            b1 = md.bars(sym, "1m")
            b5 = resample(b1, BAR_S)
            cov = md.coverage_summary(sym, "1m")
            notes = Counter((p.get("note") or "")[:60] for p in cov["needs_attention"])
            out["symbols"][sym] = {
                "coverage_1m": {
                    "first": cov["first_session"], "last": cov["last_session"],
                    "by_status": cov["by_status"],
                    "needs_attention_notes": dict(notes),
                },
                "corporate_actions": md.corporate_actions(sym),
                "one_minute_bars": len(b1),
                "five_minute_bars": len(b5),
                "sessions_with_bars": len({b.session for b in b5}),
                "signals": summarize(b5),
            }
    text = json.dumps(out, indent=2, default=str)
    (Path(__file__).resolve().parent / "counts.json").write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
