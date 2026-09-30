# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock feasibility looks only. Coverage, session and bar counts, corporate
actions, and how many sessions a band touch fires on. No price after the signal
bar is read anywhere, and no return, forward return, or outcome statistic is
computed. Writes counts.json."""

import json
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, EARLY_CLOSES, nyse_sessions, session_date_of  # noqa: E402

FIRST = date(2021, 10, 7)      # first usable session (ATR seed 2021-10-06 close)
LAST = date(2026, 9, 25)
SPLIT_END = date(2024, 6, 28)  # IS/OOS boundary used by every study on this store
KS = [0.5, 0.75, 1.0, 1.25, 1.5]
ATR_N = 14


def wilder_atr(daily):
    """List of (session_date, atr_fixed_at_that_close). Wilder's smoothing."""
    trs, out = [], []
    prev_close = None
    atr = None
    for b in daily:
        d = session_date_of(b.ts)
        if prev_close is None:
            tr = b.high - b.low
        else:
            tr = max(b.high - b.low, abs(b.high - prev_close), abs(b.low - prev_close))
        prev_close = b.close
        if trs is not None and len(out) == 0:
            trs.append(tr)
            if len(trs) == ATR_N:
                atr = sum(trs) / ATR_N
                out.append((d, atr))
        else:
            atr = (ATR_N - 1) / ATR_N * atr + tr / ATR_N
            out.append((d, atr))
    return out


def main():
    result = {}
    with MarketData() as md:
        for sym in ("QQQ", "SPY", "IGV"):
            result.setdefault("coverage", {})[sym] = md.coverage_summary(sym, "1m")
            result.setdefault("coverage_1d", {})[sym] = md.coverage_summary(sym, "1d")
            result.setdefault("actions", {})[sym] = md.corporate_actions(sym)

        # ---- QQQ feasibility structure -------------------------------------
        sessions = [d for d in nyse_sessions(FIRST, LAST)]
        daily = md.bars("QQQ", "1d", end="2026-09-23")
        atr_by_day = dict(wilder_atr(daily))
        last_atr_day = max(atr_by_day)

        touch_counts = {f"{k:g}": {"IS": 0, "OOS": 0} for k in KS}
        tapeless, no_atr = [], []
        for d in sessions:
            bars = md.bars("QQQ", "1m", start=d, end=d)
            if not bars:
                tapeless.append(str(d))
                continue
            prior = [dt for dt in atr_by_day if dt < d]
            if not prior:
                no_atr.append(str(d))
                continue
            a = atr_by_day[max(prior)]
            s = bars[0].open
            lo = min(b.low for b in bars)  # a touch can fire in the first minute too
            for k in KS:
                if lo <= s - k * a:
                    touch_counts[f"{k:g}"]["IS" if d <= SPLIT_END else "OOS"] += 1

        result["qqq"] = {
            "eval_sessions": len(sessions),
            "is_sessions": sum(1 for d in sessions if d <= SPLIT_END),
            "oos_sessions": sum(1 for d in sessions if d > SPLIT_END),
            "tapeless_sessions": tapeless,
            "sessions_without_prior_atr": no_atr,
            "early_closes_in_window": [str(d) for d in sessions if d in EARLY_CLOSES],
            "atr_last_fixed": str(last_atr_day),
            "touch_sessions_by_k": touch_counts,
        }

        # ---- data checks on 1m structure -----------------------------------
        full_390 = short = 0
        short_days = []
        for d in sessions:
            bars = md.bars("QQQ", "1m", start=d, end=d)
            if not bars:
                continue
            if len(bars) == 390:
                full_390 += 1
            else:
                short += 1
                short_days.append((str(d), len(bars)))
        result["qqq"]["sessions_390_bars"] = full_390
        result["qqq"]["sessions_other_bar_counts"] = short_days

    out = Path(__file__).resolve().parent / "counts.json"
    out.write_text(json.dumps(result, indent=1))
    print(json.dumps({k: v for k, v in result["qqq"].items() if k != "sessions_other_bar_counts"}, indent=1))


if __name__ == "__main__":
    main()
