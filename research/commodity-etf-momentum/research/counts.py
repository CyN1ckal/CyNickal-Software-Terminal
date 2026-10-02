# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock data-quality look. Counts, coverage, splits, and the ex-date
price ratio that shows whether mdq removed a reverse-split jump.

No strategy return, forward return, hit rate, or P&L.
"""

import json
import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent
NAMES = ["GLD", "SLV", "USO", "UNG", "DBA", "DBB"]
SKIP = [date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)]
EXPECTED_SPLITS = {
    "USO": [("2020-04-29", 0.125)],
    "UNG": [("2018-01-05", 0.25), ("2024-01-24", 0.25)],
}


def main() -> None:
    out: dict = {"names": {}, "spy": {}, "calendar": {}}
    with MarketData() as md:
        cal_from = date(2011, 1, 1)
        cal_to = date(2026, 10, 1)
        sessions = nyse_sessions(cal_from, cal_to)
        out["calendar"] = {
            "nyse_sessions_2011-01-01_to_2026-10-01": len(sessions),
            "skip_dates_in_nyse_calendar": [d.isoformat() for d in SKIP if d in set(sessions)],
            "skip_dates_absent_from_nyse_calendar": [d.isoformat() for d in SKIP if d not in set(sessions)],
        }
        spy = md.bars("SPY", "1d", start="2011-01-01", end="2026-10-01")
        spy_days = [b.session for b in spy]
        out["spy"] = {
            "bars_in_window": len(spy),
            "first": spy_days[0].isoformat() if spy_days else None,
            "last": spy_days[-1].isoformat() if spy_days else None,
            "duplicate_sessions": sorted(
                d.isoformat() for d, n in Counter(spy_days).items() if n > 1
            ),
            "nonpositive_ohlc": sum(
                1 for b in spy if b.open <= 0 or b.high <= 0 or b.low <= 0 or b.close <= 0
            ),
        }
        spy_set = set(spy_days)
        for name in NAMES:
            cov = md.coverage_summary(name, "1d")
            actions = md.corporate_actions(name)
            bars = md.bars(name, "1d")
            raw = md.bars(name, "1d", adjust=False)
            days = [b.session for b in bars]
            raw_by = {b.session: b for b in raw}
            adj_by = {b.session: b for b in bars}
            day_set = set(days)
            missing_vs_spy = sorted(d.isoformat() for d in spy_set - day_set)
            extra_vs_spy = sorted(d.isoformat() for d in day_set - spy_set)
            split_checks = []
            for ex, ratio in EXPECTED_SPLITS.get(name, []):
                ex_d = date.fromisoformat(ex)
                prior = [d for d in days if d < ex_d]
                if not prior or ex_d not in adj_by or ex_d not in raw_by:
                    split_checks.append({"ex_date": ex, "stored_ratio": ratio, "present": False})
                    continue
                prev = prior[-1]
                split_checks.append({
                    "ex_date": ex,
                    "stored_ratio": ratio,
                    "present": True,
                    "prev_session": prev.isoformat(),
                    "raw_close_ratio_ex_over_prev": raw_by[ex_d].close / raw_by[prev].close,
                    "adjusted_close_ratio_ex_over_prev": adj_by[ex_d].close / adj_by[prev].close,
                    "raw_jump_if_unadjusted_reverse_split": 1.0 / ratio,
                })
            out["names"][name] = {
                "coverage": {
                    "sessions_recorded": cov["sessions_recorded"],
                    "sessions_with_bars": cov["sessions_with_bars"],
                    "first_session": cov["first_session"],
                    "last_session": cov["last_session"],
                    "by_status": cov["by_status"],
                    "needs_attention_n": len(cov["needs_attention"]),
                    "needs_attention_head": cov["needs_attention"][:12],
                    "sessions_not_recorded_n": len(cov["sessions_not_recorded"]),
                    "sessions_not_recorded_head": cov["sessions_not_recorded"][:20],
                },
                "bar_count": len(bars),
                "first": days[0].isoformat() if days else None,
                "last": days[-1].isoformat() if days else None,
                "duplicate_sessions": sorted(
                    d.isoformat() for d, n in Counter(days).items() if n > 1
                ),
                "nonpositive_ohlc": sum(
                    1 for b in bars if min(b.open, b.high, b.low, b.close) <= 0
                ),
                "zero_volume": sum(1 for b in bars if b.volume == 0),
                "missing_vs_spy_in_window_n": len(missing_vs_spy),
                "missing_vs_spy_in_window_head": missing_vs_spy[:30],
                "extra_vs_spy_in_window_n": len(extra_vs_spy),
                "extra_vs_spy_in_window": extra_vs_spy[:30],
                "actions": [
                    {
                        "ex_date": a["ex_date"],
                        "type": a["type"],
                        "split_ratio": a["split_ratio"],
                        "amount": a["amount"],
                    }
                    for a in actions
                ],
                "split_continuity": split_checks,
                "bars_through_index": {
                    "253rd_session": days[252].isoformat() if len(days) >= 253 else None,
                },
            }
        # Month-ends: last SPY session of each month, a count only.
        month_ends = []
        by_month: dict[tuple[int, int], date] = {}
        for d in spy_days:
            by_month[(d.year, d.month)] = d
        for key in sorted(by_month):
            month_ends.append(by_month[key])
        ready = []
        for name in NAMES:
            ready.append(date.fromisoformat(out["names"][name]["bars_through_index"]["253rd_session"]))
        first_all_ready = max(ready)
        first_signal_candidates = [d for d in month_ends if d >= first_all_ready]
        out["signal_calendar_count_only"] = {
            "spy_month_ends_in_window": len(month_ends),
            "first_spy_month_end": month_ends[0].isoformat() if month_ends else None,
            "last_spy_month_end": month_ends[-1].isoformat() if month_ends else None,
            "session_when_each_name_has_253_bars": {
                name: out["names"][name]["bars_through_index"]["253rd_session"] for name in NAMES
            },
            "first_month_end_on_or_after_all_names_have_253_bars": (
                first_signal_candidates[0].isoformat() if first_signal_candidates else None
            ),
            "month_ends_from_that_date_through_2026-10-01": len(first_signal_candidates),
            "oos_start_is_spy_session": date(2024, 7, 1).isoformat() in {d.isoformat() for d in spy_days}
            or date(2024, 7, 1) in spy_set,
            "last_spy_session_before_2024-07-01": (
                max(d for d in spy_days if d < date(2024, 7, 1)).isoformat()
            ),
        }
    (HERE / "counts.json").write_text(json.dumps(out, indent=2) + "\n")
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
