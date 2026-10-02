# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock coverage counts. No prices, returns, signs, or P&L."""

import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions, session_date_of  # noqa: E402

SKIP = {__import__("datetime").date(2012, 10, 29),
        __import__("datetime").date(2012, 10, 30),
        __import__("datetime").date(2018, 12, 5)}
LOOKBACK = 252
FUNDS = ("TLT", "IEF")
TREASURY_LIKE = (
    "TLT", "IEF", "SHY", "SHV", "BIL", "TIP", "LQD", "HYG", "AGG", "BND",
    "GOVT", "TLH", "IEI", "VGIT", "VGLT", "SGOV", "TBT", "TMV", "TMF",
    "ZN", "ZB", "ZF", "ZT",
)


def main() -> None:
    with MarketData() as md:
        rows = md.instruments()
        symbols = [r["symbol"] for r in rows]
        print(f"instrument_count {len(symbols)}")
        print("symbols " + " ".join(symbols))
        present = [s for s in TREASURY_LIKE if s in symbols]
        absent = [s for s in TREASURY_LIKE if s not in symbols]
        print("treasury_like_present " + (" ".join(present) if present else "(none)"))
        print("treasury_like_absent " + " ".join(absent))
        for sym in ("TLT", "IEF", "SPY"):
            bars = md.bars(sym, "1d")
            days = [session_date_of(b.ts) for b in bars]
            actions = md.corporate_actions(sym)
            kinds = Counter(a["type"] for a in actions)
            cov = md.coverage_summary(sym, "1d")
            print(
                f"{sym} n={len(bars)} first={days[0].isoformat()} last={days[-1].isoformat()} "
                f"dup={len(days) - len(set(days))} actions={dict(kinds) or '{}'} "
                f"coverage_n={cov.get('sessions', cov.get('with_bars', 'na'))}"
            )
            print(f"{sym} skip_dates_with_bars " +
                  " ".join(d.isoformat() for d in days if d in SKIP) or f"{sym} skip_dates_with_bars none")
            # 1m presence only: count, not a price.
            m1 = md.bars(sym, "1m", start="2024-01-02", end="2024-01-05")
            print(f"{sym} minute_bars_2024-01-02_to_2024-01-05 {len(m1)}")

        spy_days = [session_date_of(b.ts) for b in md.bars("SPY", "1d")]
        spy_set = set(spy_days)
        cal = [d for d in nyse_sessions(spy_days[0], spy_days[-1]) if d in spy_set and d not in SKIP]
        by_month: dict[tuple[int, int], list] = {}
        for d in cal:
            by_month.setdefault((d.year, d.month), []).append(d)
        signal_dates = [days[-1] for days in by_month.values()]
        print(f"spy_book_sessions {len(cal)} signal_dates {len(signal_dates)}")
        print(f"skip_in_nyse_calendar " +
              " ".join(d.isoformat() for d in nyse_sessions("2011-01-01", "2026-10-01") if d in SKIP))
        for sym in FUNDS:
            own = [session_date_of(b.ts) for b in md.bars(sym, "1d")]
            index = {d: i for i, d in enumerate(own)}
            eligible = 0
            for d in signal_dates:
                i = index.get(d)
                if i is not None and i >= LOOKBACK:
                    eligible += 1
            missing_vs_spy = sum(1 for d in cal if d not in index)
            print(f"{sym} eligible_signals_lookback_{LOOKBACK} {eligible} "
                  f"book_sessions_missing_bar {missing_vs_spy}")


if __name__ == "__main__":
    main()
