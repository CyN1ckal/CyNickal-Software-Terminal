# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Coverage, calendar, and corporate-action counts. No prices, returns, or P&L."""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent
NAMED = ("2012-10-29", "2012-10-30", "2018-12-05", "2021-12-31", "2025-01-09",
         "2024-06-28", "2024-07-01", "2026-10-01", "2026-10-02")


def main() -> None:
    out = {"symbols": {}}
    with MarketData() as md:
        out["schema_user_version"] = md.sql("PRAGMA user_version")[0]["user_version"]
        for sym in ("EEM", "EFA", "SPY"):
            cov_d = md.coverage_summary(sym, "1d")
            cov_m = md.coverage_summary(sym, "1m")
            actions = md.corporate_actions(sym)
            days = md.coverage(sym, "1d")
            with_bars = [r["session"] for r in days if r["bar_count"] > 0]
            dup = len(with_bars) - len(set(with_bars))
            partial = [r["session"] for r in days if r["status"] == "partial"]
            missing = [r["session"] for r in days if r["status"] == "missing"]
            # Date identity only. The price fields are counted, not printed.
            bars = md.bars(sym, "1d")
            sessions = [b.session.isoformat() for b in bars]
            nonpositive = sum(
                1 for b in bars
                if b.open <= 0 or b.high <= 0 or b.low <= 0 or b.close <= 0
            )
            bad_ohlc = sum(
                1 for b in bars
                if b.high + 1e-6 < max(b.open, b.close) or b.low - 1e-6 > min(b.open, b.close)
            )
            out["symbols"][sym] = {
                "coverage_1d": cov_d,
                "coverage_1m_sessions_with_bars": cov_m["sessions_with_bars"],
                "coverage_1m_first": cov_m["first_session"],
                "coverage_1m_last": cov_m["last_session"],
                "coverage_1m_by_status": cov_m["by_status"],
                "action_count": len(actions),
                "action_types": sorted({a["type"] for a in actions}),
                "coverage_dates_with_bars": len(with_bars),
                "coverage_duplicate_dates": dup,
                "partial_sessions": partial,
                "missing_sessions": missing,
                "bars_call_count": len(bars),
                "bars_first": sessions[0] if sessions else None,
                "bars_last": sessions[-1] if sessions else None,
                "bars_duplicate_dates": len(sessions) - len(set(sessions)),
                "nonpositive_bars": nonpositive,
                "bad_ohlc_bars": bad_ohlc,
                "named_dates_present": {d: d in set(sessions) for d in NAMED},
            }
        sets = {sym: None for sym in ("EEM", "EFA", "SPY")}
        for sym in sets:
            days = md.coverage(sym, "1d")
            sets[sym] = {r["session"] for r in days if r["bar_count"] > 0}
        eem, efa, spy = sets["EEM"], sets["EFA"], sets["SPY"]
        out["date_diff"] = {
            "eem_minus_spy": sorted(eem - spy),
            "spy_minus_eem": sorted(spy - eem),
            "eem_minus_efa": sorted(eem - efa),
            "efa_minus_eem": sorted(efa - eem),
            "efa_minus_spy": sorted(efa - spy),
            "spy_minus_efa": sorted(spy - efa),
        }
        if spy:
            cal = [d.isoformat() for d in nyse_sessions(min(spy), max(spy))]
            out["nyse_vs_spy"] = {
                "nyse_sessions": len(cal),
                "spy_bars": len(spy),
                "in_calendar_not_in_spy": sorted(set(cal) - spy),
                "in_spy_not_in_calendar": sorted(spy - set(cal)),
            }
    (HERE / "counts.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "schema": out["schema_user_version"],
        "counts": {
            s: {
                "bars": out["symbols"][s]["bars_call_count"],
                "first": out["symbols"][s]["bars_first"],
                "last": out["symbols"][s]["bars_last"],
                "actions": out["symbols"][s]["action_count"],
                "nonpositive": out["symbols"][s]["nonpositive_bars"],
                "bad_ohlc": out["symbols"][s]["bad_ohlc_bars"],
                "m1": out["symbols"][s]["coverage_1m_sessions_with_bars"],
                "attention": len(out["symbols"][s]["coverage_1d"]["needs_attention"]),
                "partial": out["symbols"][s]["partial_sessions"],
                "missing": out["symbols"][s]["missing_sessions"],
            }
            for s in ("EEM", "EFA", "SPY")
        },
        "eem_minus_spy": out["date_diff"]["eem_minus_spy"],
        "spy_minus_eem": out["date_diff"]["spy_minus_eem"],
        "eem_minus_efa": out["date_diff"]["eem_minus_efa"],
        "nyse_not_spy": out["nyse_vs_spy"]["in_calendar_not_in_spy"],
        "spy_not_nyse": out["nyse_vs_spy"]["in_spy_not_in_calendar"],
    }, indent=2))


if __name__ == "__main__":
    main()
