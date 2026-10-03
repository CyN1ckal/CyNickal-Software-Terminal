# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Bar and session counts only. No prices, returns, or ranks are printed."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import MarketData, nyse_sessions  # noqa: E402

TICKERS = (
    "NVDA", "AAPL", "MSFT", "MU", "AMD", "AMZN", "META", "GOOGL", "TSLA", "SPCX",
    "GOOG", "INTC", "AVGO", "WMT", "CSCO", "LRCX", "PLTR", "AMAT", "COST", "PANW",
    "NFLX", "CRWD", "KLAC", "TXN", "SNDK", "MRVL", "LIN", "AMGN", "ADI", "QCOM",
    "STX", "SHOP", "GILD", "ASML", "TMUS", "PEP", "WDC", "ISRG", "ARM", "FTNT",
    "VRTX", "BKNG", "SBUX", "ADP", "LITE", "CDNS", "ADBE", "SNPS", "MAR", "DDOG",
    "CEG", "CSX", "MELI", "MNST", "APP", "WBD", "DASH", "CTAS", "INTU", "CMCSA",
    "MDLZ", "REGN", "ROST", "MPWR", "TER", "ORLY", "ABNB", "HON", "AEP", "NXPI",
    "ALAB", "MSTR", "FAST", "NBIS", "PCAR", "BKR", "FANG", "PDD", "HONA", "PYPL",
    "XEL", "ADSK", "RKLB", "MCHP", "CCEP", "EXC", "KDP", "CRWV", "IDXX", "FER",
    "TTWO", "ODFL", "TRI", "WDAY", "PAYX", "ROP", "AXON", "DXCM", "ALNY", "GEHC",
    "CPRT",
)
START = "2021-10-04"
EVAL_START = "2022-10-03"
END = "2026-10-02"


def main() -> None:
    if len(TICKERS) != 101 or len(set(TICKERS)) != 101:
        raise SystemExit(f"expected 101 unique tickers, got {len(TICKERS)}")
    ordered = tuple(sorted(TICKERS))
    calendar = nyse_sessions(START, END)
    evaluated = [day for day in calendar if day.isoformat() >= EVAL_START]
    warmup = [day for day in calendar if day.isoformat() < EVAL_START]
    print(f"tickers {len(TICKERS)}")
    print(f"nyse_sessions {START} {END} {len(calendar)}")
    print(f"warmup {len(warmup)} {warmup[0].isoformat()} {warmup[-1].isoformat()}")
    print(f"evaluated {len(evaluated)} {evaluated[0].isoformat()} {evaluated[-1].isoformat()}")
    print(f"contains_2021-12-31 {any(day.isoformat() == '2021-12-31' for day in calendar)}")
    print(f"contains_2025-01-09 {any(day.isoformat() == '2025-01-09' for day in calendar)}")
    print("HALF1 " + " ".join(ordered[:51]))
    print("HALF2 " + " ".join(ordered[51:]))
    cal_set = set(calendar)
    with MarketData() as md:
        for symbol in ("QQQ",) + ordered:
            bars = md.bars(symbol, "1d", start=START, end=END)
            days = [bar.session for bar in bars]
            if len(days) != len(set(days)):
                raise SystemExit(f"{symbol} has duplicate sessions")
            present = set(days)
            missing = sorted(cal_set - present)
            first = days[0].isoformat() if days else "none"
            last = days[-1].isoformat() if days else "none"
            print(f"{symbol} bars {len(days)} first {first} last {last} missing_nyse {len(missing)}")


if __name__ == "__main__":
    main()
