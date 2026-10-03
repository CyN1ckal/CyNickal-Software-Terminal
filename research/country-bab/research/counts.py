# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock data checks for country-bab. Coverage, splits, and date counts only.

No beta, forward return, hit rate, or P&L is computed.
"""

import json
import sys
from collections import defaultdict
from datetime import date, timedelta
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

UNIVERSE = [
    "EWA", "EWC", "EWG", "EWH", "EWJ", "EWS", "EWU", "EWW", "EWZ", "EWL", "EWT", "EWY",
]
# Expected splits from the parent's look. Confirmed here; not assumed if the store disagrees.
EXPECT_SPLIT = {
    "EWJ": (date(2016, 11, 7), 0.25),
    "EWS": (date(2016, 11, 7), 0.5),
    "EWU": (date(2016, 11, 7), 0.5),
    "EWT": (date(2016, 11, 7), 0.5),
}
SAMPLE_END = date(2026, 10, 1)
OOS_START = date(2024, 7, 1)
# 252 paired returns need 253 paired closes. Count only; prices are not differenced.
PAIRED_CLOSES_FOR_PRIMARY = 253
SKIP_NO_BAR = [date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)]


def month_ends(spy_dates: list[date]) -> list[date]:
    """Last NYSE session of each complete month that has a SPY bar on or before SAMPLE_END.

    A month is complete only when its true last NYSE session is on or before the last
    SPY bar used by the study. 2026-10-01 is not a month-end if October continues.
    """
    spy_set = set(spy_dates)
    first, last = spy_dates[0], spy_dates[-1]
    y, m = first.year, first.month
    out: list[date] = []
    while (y, m) <= (last.year, last.month):
        nxt = date(y + 1, 1, 1) if m == 12 else date(y, m + 1, 1)
        days = nyse_sessions(date(y, m, 1), nxt - timedelta(days=1))
        if days and days[-1] <= last:
            for d in reversed(days):
                if d in spy_set and d <= SAMPLE_END:
                    out.append(d)
                    break
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    return out


def main() -> None:
    out: dict = {"universe": UNIVERSE}
    with MarketData() as md:
        spy_bars = md.bars("SPY", "1d", end=SAMPLE_END)
        spy_dates = [b.session for b in spy_bars]
        spy_set = set(spy_dates)
        cal = nyse_sessions(spy_dates[0], SAMPLE_END)
        out["spy"] = {
            "n_bars_through_sample_end": len(spy_bars),
            "first": spy_dates[0].isoformat(),
            "last": spy_dates[-1].isoformat(),
            "n_positive_ohlc": sum(
                1 for b in spy_bars if b.open > 0 and b.high > 0 and b.low > 0 and b.close > 0
            ),
            "n_high_low_ok": sum(
                1
                for b in spy_bars
                if b.high + 1e-9 >= max(b.open, b.close) and b.low - 1e-9 <= min(b.open, b.close)
            ),
            "duplicate_dates": len(spy_dates) - len(spy_set),
            "calendar_sessions": len(cal),
            "calendar_without_spy_bar": [d.isoformat() for d in cal if d not in spy_set],
            "spy_bar_not_on_calendar": [d.isoformat() for d in spy_dates if d not in set(cal)],
            "bars_after_sample_end": [
                b.session.isoformat() for b in md.bars("SPY", "1d") if b.session > SAMPLE_END
            ],
            "corporate_actions": md.corporate_actions("SPY"),
            "coverage_1d_needs_attention": md.coverage_summary("SPY", "1d")["needs_attention"],
            "coverage_1d_by_status": md.coverage_summary("SPY", "1d")["by_status"],
            "coverage_1d_sessions_with_bars": md.coverage_summary("SPY", "1d")["sessions_with_bars"],
            "coverage_1d_first_last": [
                md.coverage_summary("SPY", "1d")["first_session"],
                md.coverage_summary("SPY", "1d")["last_session"],
            ],
        }
        ends = month_ends(spy_dates)
        out["signal_dates"] = {
            "n": len(ends),
            "first": ends[0].isoformat() if ends else None,
            "last": ends[-1].isoformat() if ends else None,
            "includes_2026_10_01": date(2026, 10, 1) in ends,
            "n_oos_signal_on_or_after_oos_start": sum(1 for d in ends if d >= OOS_START),
            "oos_signals": [d.isoformat() for d in ends if d >= OOS_START],
            "last_spy_session_before_oos": max(d for d in spy_dates if d < OOS_START).isoformat(),
            "oos_is_a_spy_session": OOS_START in spy_set,
        }

        series: dict[str, list[date]] = {}
        out["symbols"] = {}
        for sym in UNIVERSE:
            bars = md.bars(sym, "1d")
            raw = md.bars(sym, "1d", adjust=False)
            dates = [b.session for b in bars]
            series[sym] = dates
            date_set = set(dates)
            cov = md.coverage_summary(sym, "1d")
            actions = md.corporate_actions(sym)
            present = {d: (a, r) for d, a, r in zip(dates, bars, raw)}
            split_checks = []
            for act in actions:
                ex = date.fromisoformat(act["ex_date"])
                prevs = [d for d in dates if d < ex]
                if ex not in present or not prevs:
                    split_checks.append({"ex_date": act["ex_date"], "missing_bar": True, "action": act})
                    continue
                prev = prevs[-1]
                a_ex, r_ex = present[ex]
                a_prev, r_prev = present[prev]
                split_checks.append({
                    "action": act,
                    "prev_session": prev.isoformat(),
                    "prev_adj_close": a_prev.close,
                    "prev_raw_close": r_prev.close,
                    "adj_ohlc": [a_ex.open, a_ex.high, a_ex.low, a_ex.close],
                    "raw_ohlc": [r_ex.open, r_ex.high, r_ex.low, r_ex.close],
                    "adj_close_over_prev_adj_close": a_ex.close / a_prev.close,
                    "raw_close_over_prev_raw_close": r_ex.close / r_prev.close,
                    "adj_open_over_prev_adj_close": a_ex.open / a_prev.close,
                    "raw_open_over_prev_raw_close": r_ex.open / r_prev.close,
                })
            paired = [d for d in spy_dates if d in date_set]
            n_eligible = 0
            first_eligible = None
            for sig in ends:
                # Index of sig in paired closes. Eligibility is a count of closes, not a return.
                if sig not in date_set:
                    continue
                k = paired.index(sig)
                if k + 1 >= PAIRED_CLOSES_FOR_PRIMARY:
                    n_eligible += 1
                    if first_eligible is None:
                        first_eligible = sig.isoformat()
            after = [d.isoformat() for d in dates if d > SAMPLE_END]
            missing_on_spy = [d.isoformat() for d in spy_dates if d not in date_set]
            extra = [d.isoformat() for d in dates if d not in spy_set and d <= SAMPLE_END]
            out["symbols"][sym] = {
                "n_bars": len(bars),
                "first": dates[0].isoformat() if dates else None,
                "last": dates[-1].isoformat() if dates else None,
                "duplicate_dates": len(dates) - len(date_set),
                "n_nonpositive_ohlc": sum(
                    1 for b in bars if not (b.open > 0 and b.high > 0 and b.low > 0 and b.close > 0)
                ),
                "n_high_low_violation": sum(
                    1
                    for b in bars
                    if b.high + 1e-9 < max(b.open, b.close) or b.low - 1e-9 > min(b.open, b.close)
                ),
                "n_zero_volume": sum(1 for b in bars if b.volume == 0),
                "bars_after_sample_end": after,
                "missing_on_spy_through_sample_end": missing_on_spy,
                "bars_not_on_spy_through_sample_end": extra,
                "n_paired_closes_through_sample_end": len(paired),
                "coverage_1d": {
                    "sessions_recorded": cov["sessions_recorded"],
                    "sessions_with_bars": cov["sessions_with_bars"],
                    "first_session": cov["first_session"],
                    "last_session": cov["last_session"],
                    "by_status": cov["by_status"],
                    "needs_attention": cov["needs_attention"],
                    "sessions_not_recorded": cov["sessions_not_recorded"],
                },
                "corporate_actions": actions,
                "expected_split": (
                    {"ex_date": EXPECT_SPLIT[sym][0].isoformat(), "split_ratio": EXPECT_SPLIT[sym][1]}
                    if sym in EXPECT_SPLIT
                    else None
                ),
                "split_continuity": split_checks,
                "n_signal_dates_with_253_paired_closes": n_eligible,
                "first_such_signal": first_eligible,
            }

        elig_hist: dict[int, int] = defaultdict(int)
        n_ge6 = 0
        first_ge6 = None
        for sig in ends:
            n = 0
            for sym in UNIVERSE:
                dset = set(series[sym])
                paired = [d for d in spy_dates if d in dset]
                if sig not in dset:
                    continue
                k = paired.index(sig)
                if k + 1 >= PAIRED_CLOSES_FOR_PRIMARY:
                    n += 1
            elig_hist[n] += 1
            if n >= 6:
                n_ge6 += 1
                if first_ge6 is None:
                    first_ge6 = sig.isoformat()
        out["eligibility_counts"] = {
            "paired_closes_required": PAIRED_CLOSES_FOR_PRIMARY,
            "note": "Count of paired closes on or before the signal. Not a return and not a beta.",
            "names_eligible_histogram": {str(k): elig_hist[k] for k in sorted(elig_hist)},
            "signal_dates_with_at_least_6": n_ge6,
            "first_signal_date_with_at_least_6": first_ge6,
        }
        out["named_closures_absent_from_spy"] = [d.isoformat() for d in SKIP_NO_BAR if d not in spy_set]

    path = Path(__file__).resolve().parent / "counts.json"
    path.write_text(json.dumps(out, indent=2, default=str) + "\n", encoding="utf-8")
    print(f"wrote {path}")


if __name__ == "__main__":
    main()
