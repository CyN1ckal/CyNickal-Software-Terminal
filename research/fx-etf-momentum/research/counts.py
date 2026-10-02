# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock data checks for fx-etf-momentum. Counts, coverage, and corporate
actions only. No return, P&L, forward return, or signal sign is computed."""

import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions, session_date_of  # noqa: E402

UNIVERSE = ("FXE", "FXB", "FXA", "FXC", "FXF", "FXY")
SKIP = {date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)}
START = date(2011, 1, 4)
END = date(2026, 10, 1)


def decimals(x: float) -> int:
    s = f"{x:.10f}".rstrip("0")
    if "." not in s:
        return 0
    return len(s.split(".")[1])


def main() -> None:
    with MarketData() as md:
        spy = md.bars("SPY", "1d", start=START, end=END)
        spy_days = [session_date_of(b.ts) for b in spy]
        spy_set = set(spy_days)
        print(f"SPY bars {len(spy_days)} first {spy_days[0]} last {spy_days[-1]}")
        print(f"SPY duplicate dates {len(spy_days) - len(spy_set)}")
        cal = nyse_sessions(spy_days[0], spy_days[-1])
        missing_vs_cal = [d for d in cal if d not in spy_set]
        extra_vs_cal = [d for d in spy_days if d not in set(cal)]
        print(f"nyse_sessions in SPY span {len(cal)}")
        print(f"calendar days with no SPY bar ({len(missing_vs_cal)}): {missing_vs_cal}")
        print(f"SPY bars not on nyse_sessions ({len(extra_vs_cal)}): {extra_vs_cal}")
        for d in sorted(SKIP):
            print(f"skip-date {d} weekday={d.strftime('%A')} in_nyse_sessions={d in set(cal)} spy_bar={d in spy_set}")

        months = []
        by_month: dict[tuple[int, int], date] = {}
        for d in spy_days:
            by_month[(d.year, d.month)] = d
        months = [by_month[k] for k in sorted(by_month)]
        print(f"SPY month-end signal dates {len(months)} first {months[0]} last {months[-1]}")

        for sym in UNIVERSE:
            info = md.resolve(sym)
            bars = md.bars(sym, "1d", start=START, end=END)
            raw = md.bars(sym, "1d", start=START, end=END, adjust=False)
            days = [session_date_of(b.ts) for b in bars]
            cov = md.coverage_summary(sym, "1d")
            actions = md.corporate_actions(sym)
            types = Counter(a["type"] for a in actions)
            split_n = sum(1 for a in actions if a["type"] == "split")
            bad = 0
            dec = Counter()
            for b in bars:
                if not (b.open > 0 and b.high > 0 and b.low > 0 and b.close > 0):
                    bad += 1
                if b.high < b.low or b.high < max(b.open, b.close) or b.low > min(b.open, b.close):
                    bad += 1
                dec[decimals(b.close)] += 1
            differ = sum(1 for a, r in zip(bars, raw) if a.close != r.close or a.open != r.open)
            dset = set(days)
            print(
                f"{sym} id={info['id']} name={info.get('name')} class={info.get('class')} "
                f"bars={len(days)} first={days[0] if days else None} last={days[-1] if days else None} "
                f"dup={len(days) - len(dset)} nonpositive_or_ohlc={bad} "
                f"adjust_differs={differ} actions={dict(types)} splits={split_n}"
            )
            print(
                f"  coverage recorded={cov['sessions_recorded']} with_bars={cov['sessions_with_bars']} "
                f"first={cov['first_session']} last={cov['last_session']} status={cov['by_status']} "
                f"needs_attention={len(cov['needs_attention'])} not_recorded={len(cov['sessions_not_recorded'])}"
            )
            if cov["needs_attention"]:
                print(f"  needs_attention sample {cov['needs_attention'][:8]}")
            if cov["sessions_not_recorded"]:
                print(f"  not_recorded {cov['sessions_not_recorded']}")
            only_spy = [d for d in spy_days if d not in dset]
            only_sym = [d for d in days if d not in spy_set]
            print(f"  missing_vs_SPY {len(only_spy)} {only_spy[:12]} extra_vs_SPY {len(only_sym)} {only_sym[:12]}")
            print(f"  close_decimal_places {dict(sorted(dec.items()))}")
            if actions:
                ex = [a["ex_date"] for a in actions]
                print(f"  action_ex_dates {len(ex)} first {ex[0]} last {ex[-1]}")


if __name__ == "__main__":
    main()
