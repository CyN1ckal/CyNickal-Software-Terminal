# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock data checks for spdr-sector-momentum. Counts and adjustment continuity only.

No formation return, forward return, hit rate, or P&L is computed.
"""

import json
import sys
from collections import defaultdict
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

UNIVERSE = ["XLK", "XLF", "XLE", "XLV", "XLI", "XLY", "XLP", "XLU", "XLB", "XLRE", "XLC"]
SPLIT_NAMES = ["XLK", "XLE", "XLY", "XLU", "XLB"]
SPLIT_EX = date(2025, 12, 5)
FAR = 273  # primary eligibility lag; used only as a bar-count threshold
SKIP_NO_BAR = [date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)]
XLRE_GAPS = [date(2015, 10, 14), date(2015, 11, 27)]
WATCH = SKIP_NO_BAR + XLRE_GAPS + [date(2021, 12, 31), date(2025, 1, 9), date(2025, 12, 5), date(2026, 10, 1)]


def month_ends(spy_dates: list[date]) -> list[date]:
    """Last NYSE session of each calendar month that is complete in the sample.

    A month is complete only when its true last NYSE session (mdq calendar, not
    the truncated store) is on or before the last SPY bar. The signal inside
    that month is the last session that has a SPY bar. 2026-10-01 is not a
    month-end: October 2026 continues after the store.
    """
    from datetime import timedelta

    spy_set = set(spy_dates)
    first, last = spy_dates[0], spy_dates[-1]
    y, m = first.year, first.month
    out: list[date] = []
    while (y, m) <= (last.year, last.month):
        nxt = date(y + 1, 1, 1) if m == 12 else date(y, m + 1, 1)
        days = nyse_sessions(date(y, m, 1), nxt - timedelta(days=1))
        if days and days[-1] <= last:
            for d in reversed(days):
                if d in spy_set:
                    out.append(d)
                    break
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    return out


def main() -> None:
    out: dict = {"universe": UNIVERSE, "symbols": {}}
    with MarketData() as md:
        spy = md.bars("SPY", "1d")
        spy_dates = [b.session for b in spy]
        spy_set = set(spy_dates)
        cal = nyse_sessions(spy_dates[0], spy_dates[-1]) if spy_dates else []
        cal_set = set(cal)
        out["spy"] = {
            "n_bars": len(spy),
            "first": spy_dates[0].isoformat() if spy_dates else None,
            "last": spy_dates[-1].isoformat() if spy_dates else None,
            "calendar_sessions": len(cal),
            "calendar_without_spy_bar": [d.isoformat() for d in cal if d not in spy_set],
            "spy_bar_not_on_calendar": [d.isoformat() for d in spy_dates if d not in cal_set],
        }
        ends = month_ends(spy_dates)
        oos_ends = [d for d in ends if d >= date(2024, 7, 1) and d <= date(2026, 10, 1)]
        out["signal_dates"] = {
            "n_month_ends_with_spy_bar": len(ends),
            "first": ends[0].isoformat() if ends else None,
            "last": ends[-1].isoformat() if ends else None,
            "n_month_ends_in_oos_window": len(oos_ends),
            "oos_month_ends": [d.isoformat() for d in oos_ends],
        }

        series: dict[str, list[date]] = {}
        for sym in UNIVERSE + ["SPY"]:
            bars = md.bars(sym, "1d")
            raw = md.bars(sym, "1d", adjust=False)
            dates = [b.session for b in bars]
            series[sym] = dates
            cov = md.coverage_summary(sym, "1d")
            actions = md.corporate_actions(sym)
            cov_1m = md.coverage_summary(sym, "1m")
            present = {d: (a, r) for d, a, r in zip(dates, bars, raw)}
            watch = {}
            for d in WATCH:
                if d in present:
                    a, r = present[d]
                    watch[d.isoformat()] = {
                        "has_bar": True,
                        "adj_open": a.open,
                        "adj_close": a.close,
                        "raw_open": r.open,
                        "raw_close": r.close,
                    }
                else:
                    watch[d.isoformat()] = {"has_bar": False}
            # Split-adjustment continuity on 2025-12-05. A ratio, not a strategy return.
            split_check = None
            if sym in SPLIT_NAMES or True:
                if SPLIT_EX in present:
                    prevs = [d for d in dates if d < SPLIT_EX]
                    if prevs:
                        prev = prevs[-1]
                        a_ex, r_ex = present[SPLIT_EX]
                        a_prev, r_prev = present[prev]
                        split_check = {
                            "prev_session": prev.isoformat(),
                            "prev_adj_close": a_prev.close,
                            "adj_ohlc": [a_ex.open, a_ex.high, a_ex.low, a_ex.close],
                            "raw_ohlc": [r_ex.open, r_ex.high, r_ex.low, r_ex.close],
                            "adj_close_ratio": a_ex.close / a_prev.close if a_prev.close else None,
                            "raw_close_ratio": r_ex.close / r_prev.close if r_prev.close else None,
                            "adj_open_over_prev_adj_close": a_ex.open / a_prev.close if a_prev.close else None,
                            "raw_open_over_prev_raw_close": r_ex.open / r_prev.close if r_prev.close else None,
                        }
            # Eligibility counts: a name can be ranked on a signal date only with a bar there
            # and a bar FAR own-sessions earlier. No price ratio is stored.
            date_to_i = {d: i for i, d in enumerate(dates)}
            n_eligible_signals = 0
            first_eligible = None
            for sig in ends:
                i = date_to_i.get(sig)
                if i is not None and i >= FAR:
                    n_eligible_signals += 1
                    if first_eligible is None:
                        first_eligible = sig.isoformat()
            missing_after_list = []
            if dates:
                missing_after_list = [d.isoformat() for d in spy_dates if d >= dates[0] and d not in date_to_i]
            out["symbols"][sym] = {
                "n_bars": len(bars),
                "first": dates[0].isoformat() if dates else None,
                "last": dates[-1].isoformat() if dates else None,
                "coverage_1d": {
                    "sessions_recorded": cov["sessions_recorded"],
                    "sessions_with_bars": cov["sessions_with_bars"],
                    "first_session": cov["first_session"],
                    "last_session": cov["last_session"],
                    "by_status": cov["by_status"],
                    "needs_attention": cov["needs_attention"],
                    "sessions_not_recorded": cov["sessions_not_recorded"],
                },
                "coverage_1m_sessions_with_bars": cov_1m["sessions_with_bars"],
                "coverage_1m_range": [cov_1m["first_session"], cov_1m["last_session"]],
                "corporate_actions": actions,
                "n_spy_sessions_missing": sum(1 for d in spy_dates if d not in date_to_i),
                "missing_on_or_after_first_bar": missing_after_list,
                "bars_not_on_spy": [d.isoformat() for d in dates if d not in spy_set],
                "watch": watch,
                "split_continuity_2025_12_05": split_check,
                "n_signal_dates_with_far_bar": n_eligible_signals,
                "first_signal_date_with_far_bar": first_eligible,
            }

        # How many names clear the far-bar count on each signal date. Not a return.
        elig_hist = defaultdict(int)
        n_ge6 = 0
        first_ge6 = None
        for sig in ends:
            n = 0
            for sym in UNIVERSE:
                dates = series[sym]
                # binary search
                lo, hi = 0, len(dates)
                while lo < hi:
                    mid = (lo + hi) // 2
                    if dates[mid] < sig:
                        lo = mid + 1
                    else:
                        hi = mid
                if lo < len(dates) and dates[lo] == sig and lo >= FAR:
                    n += 1
            elig_hist[n] += 1
            if n >= 6:
                n_ge6 += 1
                if first_ge6 is None:
                    first_ge6 = sig.isoformat()
        out["eligibility_counts"] = {
            "names_eligible_histogram": {str(k): elig_hist[k] for k in sorted(elig_hist)},
            "signal_dates_with_at_least_6": n_ge6,
            "first_signal_date_with_at_least_6": first_ge6,
        }

        # SPY daily close vs last regular-hours minute, where 1m exists.
        # Data-quality only: are stored daily closes the 16:00 print?
        spy_1m = md.bars("SPY", "1m")
        if spy_1m:
            last_min = {}
            for b in spy_1m:
                last_min[b.session] = b.close
            spy_1d = {b.session: b for b in spy}
            diffs = []
            for d, close in last_min.items():
                if d in spy_1d and close:
                    ratio = spy_1d[d].close / close - 1.0
                    diffs.append((abs(ratio), d, ratio, spy_1d[d].close, close))
            diffs.sort(reverse=True)
            over_20bp = [x for x in diffs if x[0] > 0.002]
            over_20bp_from_2024 = [x for x in over_20bp if x[1] >= date(2024, 11, 1)]
            out["spy_1d_vs_last_1m"] = {
                "n_overlap": len(diffs),
                "n_abs_log_over_20bp": len(over_20bp),
                "n_abs_over_20bp_from_2024_11": len(over_20bp_from_2024),
                "worst_10": [
                    {
                        "session": d.isoformat(),
                        "rel_diff": ratio,
                        "daily_close": dc,
                        "last_1m": lm,
                    }
                    for _, d, ratio, dc, lm in diffs[:10]
                ],
            }
        else:
            out["spy_1d_vs_last_1m"] = None

    dest = Path(__file__).resolve().parent / "counts.json"
    dest.write_text(json.dumps(out, indent=2, default=str) + "\n", encoding="utf-8")
    print(f"wrote {dest}")
    print("spy", out["spy"]["n_bars"], out["spy"]["first"], out["spy"]["last"])
    print("calendar without spy", out["spy"]["calendar_without_spy_bar"])
    print("oos month ends", out["signal_dates"]["n_month_ends_in_oos_window"])
    print("eligibility", out["eligibility_counts"])
    for sym in UNIVERSE:
        s = out["symbols"][sym]
        print(
            sym,
            s["n_bars"],
            s["first"],
            s["last"],
            "missing_vs_spy",
            s["n_spy_sessions_missing"],
            "missing_after_list",
            s["missing_on_or_after_first_bar"],
            "actions",
            [(a["type"], a["ex_date"], a.get("split_ratio"), a.get("amount")) for a in s["corporate_actions"]],
            "split_check",
            s["split_continuity_2025_12_05"],
            "1m",
            s["coverage_1m_sessions_with_bars"],
        )
    if out["spy_1d_vs_last_1m"]:
        q = out["spy_1d_vs_last_1m"]
        print("spy 1d vs 1m", q["n_overlap"], "over20bp", q["n_abs_log_over_20bp"], "from_2024_11", q["n_abs_over_20bp_from_2024_11"])
        print("worst", q["worst_10"][:5])


if __name__ == "__main__":
    main()
