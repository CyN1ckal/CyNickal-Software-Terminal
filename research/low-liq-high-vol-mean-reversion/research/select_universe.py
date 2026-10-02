# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Turn the Koyfin screen into the primary and cross-market books.

Reads screener.csv and writes universe.json. Uses the screen filters only.
Does not read the price-change columns and does not touch the market-data store.

    python research/low-liq-high-vol-mean-reversion/research/select_universe.py
"""

from __future__ import annotations

import csv
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCREEN = HERE.parent / "screener.csv"
OUT = HERE / "universe.json"

# Whole words only. "Community" contains the letters "unit" and must not match.
EXCLUDE_NAME = re.compile(
    r"\b(warrant|warrants|preferred|unit|units|etf|etn|acquisition|spac|rights)\b",
    re.IGNORECASE,
)


def main() -> None:
    rows = list(csv.DictReader(SCREEN.open(encoding="utf-8-sig", newline="")))
    qualifying = []
    excluded = []
    for row in rows:
        ticker = row["Ticker"].strip().upper()
        name = row["Name"].strip()
        market_cap_m = float(row["Market Cap"])
        dollar_volume = float(row["Volume Notional"])
        vol_pct = float(row["Volatility (3M)"])
        cagr = float(row["Total Revenues/CAGR (1Y FQ)"])
        reasons = []
        if not ticker.isalpha() or not 1 <= len(ticker) <= 5:
            reasons.append("ticker symbol is not 1-5 letters")
        if EXCLUDE_NAME.search(name):
            reasons.append("name is a warrant, unit, preferred, ETF, or acquisition shell")
        if not 50.0 <= market_cap_m <= 400.0:
            reasons.append("market cap outside 50-400 million")
        if not 1_000_000.0 <= dollar_volume <= 10_000_000.0:
            reasons.append("dollar volume outside 1-10 million")
        if not 20.0 <= vol_pct <= 60.0:
            reasons.append("3-month volatility outside 20-60 percent")
        if not -0.10 <= cagr <= 0.10:
            reasons.append("1-year revenue CAGR outside -10 to +10 percent")
        if reasons:
            excluded.append({"ticker": ticker, "name": name, "reasons": reasons})
            continue
        qualifying.append(ticker)

    qualifying.sort()
    primary = [ticker for i, ticker in enumerate(qualifying) if i % 2 == 0]
    cross = [ticker for i, ticker in enumerate(qualifying) if i % 2 == 1]
    payload = {
        "screen_file": "screener.csv",
        "screen_rows": len(rows),
        "price_change_columns_used": False,
        "qualifying_sorted": qualifying,
        "primary": primary,
        "cross": cross,
        "excluded": excluded,
        "assignment": (
            "Sort qualifying tickers A to Z. Even indexes are the primary book. "
            "Odd indexes are the cross-market book. No price-change column is an input."
        ),
    }
    OUT.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"rows {len(rows)} qualifying {len(qualifying)} excluded {len(excluded)}")
    print(f"primary {len(primary)}: {' '.join(primary)}")
    print(f"cross {len(cross)}: {' '.join(cross)}")
    for item in excluded:
        print(f"excluded {item['ticker']}: {'; '.join(item['reasons'])}")


if __name__ == "__main__":
    main()
