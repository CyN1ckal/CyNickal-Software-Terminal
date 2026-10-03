# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock coverage and calendar counts. No prices, no returns, no P&L."""

import json
import sys
from collections import Counter
from datetime import date, timedelta
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, is_nyse_holiday, nyse_sessions

FIRST = date(2011, 1, 4)
LAST = date(2026, 10, 1)
OOS_START = date(2024, 7, 1)
HERE = Path(__file__).resolve().parent

CASES = (
    "2024-03-28",
    "2024-01-12",
    "2024-01-05",
    "2024-11-29",
    "2024-07-03",
    "2022-06-17",
    "2021-12-31",
    "2025-01-08",
    "2025-01-09",
    "2024-12-24",
    "2011-01-14",
    "2011-01-04",
)


def is_pre_holiday(session: date) -> bool:
    nxt = session + timedelta(days=1)
    while nxt.weekday() >= 5:
        nxt += timedelta(days=1)
    return is_nyse_holiday(nxt)


def next_weekday(session: date) -> date:
    nxt = session + timedelta(days=1)
    while nxt.weekday() >= 5:
        nxt += timedelta(days=1)
    return nxt


def main():
    calendar = nyse_sessions(FIRST, LAST)
    cal_set = set(calendar)
    out = {
        "window": [FIRST.isoformat(), LAST.isoformat()],
        "nyse_sessions": len(calendar),
        "cases": [],
        "symbols": {},
    }
    for iso in CASES:
        day = date.fromisoformat(iso)
        nxt = next_weekday(day)
        out["cases"].append({
            "session": iso,
            "weekday": day.strftime("%A"),
            "is_nyse_session": day.weekday() < 5 and not is_nyse_holiday(day),
            "is_nyse_holiday": is_nyse_holiday(day),
            "is_pre_holiday": is_pre_holiday(day),
            "early_close": day in EARLY_CLOSES,
            "next_weekday": nxt.isoformat(),
            "next_is_holiday": is_nyse_holiday(nxt),
        })

    with MarketData() as md:
        for sym in ("SPY", "QQQ", "IWM"):
            rows = md.coverage(sym, "1d", start=FIRST, end=LAST)
            actions = md.corporate_actions(sym)
            with_bars = [r for r in rows if r["bar_count"] > 0]
            bar_dates = [date.fromisoformat(r["session"]) for r in with_bars]
            dupes = [d.isoformat() for d, n in Counter(bar_dates).items() if n > 1]
            bar_set = set(bar_dates)
            pre = [d for d in bar_dates if d in cal_set and is_pre_holiday(d)]
            pre_no_bar = [
                d.isoformat() for d in calendar
                if is_pre_holiday(d) and d not in bar_set
            ]
            missing_sessions = [d.isoformat() for d in calendar if d not in bar_set]
            extra = [d.isoformat() for d in bar_dates if d not in cal_set]
            by_status = Counter(r["status"] for r in rows)
            zero = [r for r in rows if r["bar_count"] == 0]
            partial = [r["session"] for r in rows if r["status"] == "partial"]
            out["symbols"][sym] = {
                "coverage_rows": len(rows),
                "sessions_with_bars": len(with_bars),
                "first": bar_dates[0].isoformat() if bar_dates else None,
                "last": bar_dates[-1].isoformat() if bar_dates else None,
                "duplicate_dates": dupes,
                "by_status": dict(by_status),
                "zero_bar_rows": len(zero),
                "partial_sessions": partial,
                "nyse_sessions_without_a_bar": missing_sessions,
                "bar_dates_that_are_not_nyse_sessions": extra,
                "corporate_actions": len(actions),
                "corporate_action_types": dict(Counter(a["type"] for a in actions)),
                "pre_holiday_with_bar": len(pre),
                "pre_holiday_is": sum(1 for d in pre if d < OOS_START),
                "pre_holiday_oos": sum(1 for d in pre if d >= OOS_START),
                "first_pre_holiday": pre[0].isoformat() if pre else None,
                "last_pre_holiday": pre[-1].isoformat() if pre else None,
                "pre_holiday_without_bar": pre_no_bar,
                "pre_holiday_early_closes": [d.isoformat() for d in pre if d in EARLY_CLOSES],
                "evaluation_sessions_if_window_starts_at_first_pre": (
                    sum(1 for d in bar_dates if pre and d >= pre[0])
                ),
                "evaluation_is": sum(1 for d in bar_dates if pre and pre[0] <= d < OOS_START),
                "evaluation_oos": sum(1 for d in bar_dates if pre and d >= pre[0] and d >= OOS_START and d <= LAST),
            }

    (HERE / "counts.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8", newline="\n")
    spy = out["symbols"]["SPY"]
    print(
        f"SPY bars {spy['sessions_with_bars']} {spy['first']} {spy['last']} "
        f"pre {spy['pre_holiday_with_bar']} IS {spy['pre_holiday_is']} "
        f"OOS {spy['pre_holiday_oos']} first {spy['first_pre_holiday']}"
    )
    print("actions", {s: out["symbols"][s]["corporate_actions"] for s in out["symbols"]})
    print("dates_equal", len({
        json.dumps(out["symbols"][s]["nyse_sessions_without_a_bar"]) for s in out["symbols"]
    }) == 1)


if __name__ == "__main__":
    main()
