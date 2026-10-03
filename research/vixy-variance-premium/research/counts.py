# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock coverage, splits, and month-end counts. No strategy return or P&L."""

import json
import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions, session_date_of  # noqa: E402

HERE = Path(__file__).resolve().parent
LAST = date(2026, 10, 1)
SKIP = {date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)}
SYMBOLS = ("VIXY", "UVXY", "SPY")


def _month_ends(days):
    last = {}
    for d in days:
        last[(d.year, d.month)] = d
    return [last[k] for k in sorted(last)]


def main() -> None:
    out = {"schema_user_version": None, "symbols": {}}
    with MarketData() as md:
        out["schema_user_version"] = md.schema_version
        known = [r["symbol"] for r in md.instruments()]
        out["instrument_count"] = len(known)
        for sym in SYMBOLS:
            out["symbols"][sym] = {"present": sym in known}
        if any(not out["symbols"][s]["present"] for s in SYMBOLS):
            (HERE / "counts.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
            raise SystemExit("missing symbol")

        loaded = {}
        for sym in SYMBOLS:
            bars = md.bars(sym, "1d", adjust=True)
            raw = md.bars(sym, "1d", adjust=False)
            actions = md.corporate_actions(sym)
            cov = md.coverage_summary(sym, "1d")
            days = [session_date_of(b.ts) for b in bars]
            loaded[sym] = (bars, raw, days)
            ohlc_bad = 0
            hl_bad = 0
            for b in bars:
                if min(b.open, b.high, b.low, b.close) <= 0.0:
                    ohlc_bad += 1
                if b.high + 1e-6 < max(b.open, b.close) or b.low - 1e-6 > min(b.open, b.close):
                    hl_bad += 1
            splits = [
                {"ex_date": a["ex_date"], "split_ratio": a["split_ratio"], "type": a["type"]}
                for a in actions
                if a["type"] == "split"
            ]
            out["symbols"][sym] = {
                "present": True,
                "n_bars": len(bars),
                "first": days[0].isoformat() if days else None,
                "last": days[-1].isoformat() if days else None,
                "n_on_or_before_2026_10_01": sum(1 for d in days if d <= LAST),
                "n_after_2026_10_01": sum(1 for d in days if d > LAST),
                "dates_after_2026_10_01": [d.isoformat() for d in days if d > LAST],
                "duplicate_sessions": len(days) - len(set(days)),
                "nonpositive_ohlc": ohlc_bad,
                "high_low_violations": hl_bad,
                "zero_volume": sum(1 for b in bars if b.volume == 0),
                "action_counts": dict(Counter(a["type"] for a in actions)),
                "splits": splits,
                "coverage": {
                    "sessions_recorded": cov["sessions_recorded"],
                    "sessions_with_bars": cov["sessions_with_bars"],
                    "first_session": cov["first_session"],
                    "last_session": cov["last_session"],
                    "by_status": cov["by_status"],
                    "needs_attention": cov["needs_attention"],
                    "sessions_not_recorded_n": len(cov["sessions_not_recorded"]),
                    "sessions_not_recorded_head": cov["sessions_not_recorded"][:12],
                },
                "minute_bars": len(md.bars(sym, "1m", start="2011-01-04", end="2026-10-02")),
                "skip_dates_with_bars": [d.isoformat() for d in days if d in SKIP],
            }
            # Jump check only: ex-date close / previous close, raw vs adjusted.
            # Not a strategy return. The adjusted ratio must not be the split multiple.
            raw_by = {session_date_of(b.ts): b for b in raw}
            adj_by = {session_date_of(b.ts): b for b in bars}
            jumps = []
            for sp in splits:
                ex = date.fromisoformat(sp["ex_date"])
                prevs = [d for d in days if d < ex]
                if not prevs or ex not in adj_by:
                    jumps.append({"ex_date": sp["ex_date"], "status": "no adjacent bar"})
                    continue
                prev = prevs[-1]
                raw_ratio = raw_by[ex].close / raw_by[prev].close
                adj_ratio = adj_by[ex].close / adj_by[prev].close
                multiple = 1.0 / sp["split_ratio"] if sp["split_ratio"] else None
                jumps.append({
                    "ex_date": sp["ex_date"],
                    "prev": prev.isoformat(),
                    "split_ratio": sp["split_ratio"],
                    "reverse_multiple": multiple,
                    "raw_close_ratio": raw_ratio,
                    "adjusted_close_ratio": adj_ratio,
                    "adjusted_contains_split_jump": (
                        multiple is not None and abs(adj_ratio - multiple) < abs(adj_ratio - 1.0)
                    ),
                })
            out["symbols"][sym]["split_jump_check"] = jumps

        cal_end = date(2026, 10, 31)
        cal = [d for d in nyse_sessions(date(2011, 1, 1), cal_end) if d not in SKIP]
        out["nyse_sessions_2011_01_01_through_2026_10_31_ex_skip"] = len(cal)
        out["skip_in_nyse_calendar"] = [
            d.isoformat() for d in nyse_sessions(date(2011, 1, 1), LAST) if d in SKIP
        ]
        month_ends = [d for d in _month_ends(cal) if d <= LAST]
        out["month_ends_on_or_before_2026_10_01"] = len(month_ends)
        out["october_2026_month_end"] = _month_ends([d for d in cal if d.year == 2026 and d.month == 10])[-1].isoformat()

        for sym in ("VIXY", "UVXY"):
            days = [d for d in loaded[sym][2] if d <= LAST and d not in SKIP]
            have = set(days)
            signals = [d for d in month_ends if d in have and d >= days[0]]
            fills = []
            for sd in signals:
                later = [d for d in days if d > sd]
                if later:
                    fills.append(later[0])
            oos_fills = [d for d in fills if d >= date(2024, 7, 1)]
            info = out["symbols"][sym]
            info["bars_used"] = len(days)
            info["first_used"] = days[0].isoformat()
            info["last_used"] = days[-1].isoformat()
            info["signal_dates"] = len(signals)
            info["first_signal"] = signals[0].isoformat() if signals else None
            info["last_signal"] = signals[-1].isoformat() if signals else None
            info["fills"] = len(fills)
            info["first_fill"] = fills[0].isoformat() if fills else None
            info["last_fill"] = fills[-1].isoformat() if fills else None
            info["fills_on_or_after_2024_07_01"] = len(oos_fills)
            info["month_ends_in_span_missing_bar"] = [
                d.isoformat() for d in month_ends if days[0] <= d <= days[-1] and d not in have
            ]
            # Sessions in the NYSE calendar inside the symbol's span with no bar.
            span = [d for d in cal if days[0] <= d <= LAST]
            info["nyse_sessions_in_span_to_last"] = len(span)
            info["span_sessions_missing_bar"] = sum(1 for d in span if d not in have)

    (HERE / "counts.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
