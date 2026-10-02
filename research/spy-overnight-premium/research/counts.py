# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock coverage and corporate-action counts. No prices and no returns.

Reads coverage rows and corporate_action rows only. Does not call bars().
"""

import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

FIRST = "2011-01-04"
LAST = "2026-10-01"
SYMBOLS = ("SPY", "QQQ", "IWM")
NAMED_GAPS = ("2012-10-29", "2012-10-30", "2018-12-05", "2021-12-31", "2025-01-09")


def main():
    out = {"window": [FIRST, LAST], "symbols": {}}
    with MarketData() as md:
        cal = [d.isoformat() for d in nyse_sessions(FIRST, LAST)]
        out["nyse_sessions_in_window"] = len(cal)
        cal_set = set(cal)
        for sym in SYMBOLS:
            summary = md.coverage_summary(sym, "1d")
            rows = md.coverage(sym, "1d", start=FIRST, end=LAST)
            with_bars = [r["session"] for r in rows if r["bar_count"] > 0]
            zero = [r["session"] for r in rows if r["bar_count"] == 0]
            partial = [r["session"] for r in rows if r["status"] == "partial"]
            missing = [r["session"] for r in rows if r["status"] == "missing"]
            outside = [
                r["session"] for r in md.coverage(sym, "1d")
                if r["bar_count"] > 0 and (r["session"] < FIRST or r["session"] > LAST)
            ]
            actions = md.corporate_actions(sym)
            types = Counter(a["type"] for a in actions)
            splits = [
                {"ex_date": a["ex_date"], "split_ratio": a["split_ratio"]}
                for a in actions if a["type"] == "split"
            ]
            div_dates = sorted(a["ex_date"] for a in actions if a["type"] == "dividend")
            bar_set = set(with_bars)
            missing_vs_cal = sorted(cal_set - bar_set)
            extra_vs_cal = sorted(bar_set - cal_set)
            out["symbols"][sym] = {
                "coverage_summary_1d": {
                    "sessions_recorded": summary["sessions_recorded"],
                    "sessions_with_bars": summary["sessions_with_bars"],
                    "first_session": summary["first_session"],
                    "last_session": summary["last_session"],
                    "by_status": summary["by_status"],
                    "needs_attention_n": len(summary["needs_attention"]),
                    "needs_attention_head": summary["needs_attention"][:12],
                    "sessions_not_recorded_n": len(summary["sessions_not_recorded"]),
                    "sessions_not_recorded_head": summary["sessions_not_recorded"][:20],
                },
                "in_window_with_bars": len(with_bars),
                "in_window_first": with_bars[0] if with_bars else None,
                "in_window_last": with_bars[-1] if with_bars else None,
                "in_window_zero_bar_rows": len(zero),
                "partial_sessions": partial,
                "missing_sessions": missing,
                "bars_outside_window": outside,
                "duplicate_dates": len(with_bars) - len(bar_set),
                "missing_vs_nyse_calendar": missing_vs_cal,
                "extra_vs_nyse_calendar": extra_vs_cal,
                "named_dates_have_bars": {d: d in bar_set for d in NAMED_GAPS},
                "action_types": dict(types),
                "splits": splits,
                "dividend_count": len(div_dates),
                "dividend_first": div_dates[0] if div_dates else None,
                "dividend_last": div_dates[-1] if div_dates else None,
                "dividend_ex_dates_in_window": sum(1 for d in div_dates if FIRST <= d <= LAST),
            }
    text = json.dumps(out, indent=2)
    Path(__file__).with_name("counts.json").write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
