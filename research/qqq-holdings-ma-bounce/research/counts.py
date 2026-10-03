# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Coverage counts only. No prices, returns, signals, or P&L.

Run from the repo root:

    python research/qqq-holdings-ma-bounce/research/counts.py
"""

from __future__ import annotations

import json
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent
FIRST = date(2021, 10, 4)
LAST = date(2026, 10, 2)
EVAL_START = date(2022, 10, 3)

NAMES = (
    "NVDA AAPL MSFT MU AMD AMZN META GOOGL TSLA SPCX GOOG INTC AVGO WMT CSCO LRCX "
    "PLTR AMAT COST PANW NFLX CRWD KLAC TXN SNDK MRVL LIN AMGN ADI QCOM STX SHOP GILD "
    "ASML TMUS PEP WDC ISRG ARM FTNT VRTX BKNG SBUX ADP LITE CDNS ADBE SNPS MAR DDOG "
    "CEG CSX MELI MNST APP WBD DASH CTAS INTU CMCSA MDLZ REGN ROST MPWR TER ORLY ABNB "
    "HON AEP NXPI ALAB MSTR FAST NBIS PCAR BKR FANG PDD HONA PYPL XEL ADSK RKLB MCHP "
    "CCEP EXC KDP CRWV IDXX FER TTWO ODFL TRI WDAY PAYX ROP AXON DXCM ALNY GEHC CPRT"
).split()


def main() -> None:
    if len(NAMES) != 101 or len(set(NAMES)) != 101:
        raise SystemExit(f"universe is {len(NAMES)} names, {len(set(NAMES))} unique")
    calendar = nyse_sessions(FIRST, LAST)
    eval_days = [day for day in calendar if day >= EVAL_START]
    warmup = [day for day in calendar if day < EVAL_START]
    rows = []
    with MarketData() as md:
        for symbol in NAMES + ["QQQ", "SPY"]:
            bars = md.bars(symbol, "1d", start=FIRST, end=LAST, adjust=True)
            sessions = [bar.session for bar in bars]
            if len(sessions) != len(set(sessions)):
                raise SystemExit(f"{symbol} has duplicate daily sessions")
            have = set(sessions)
            rows.append({
                "symbol": symbol,
                "bars": len(sessions),
                "first": sessions[0].isoformat() if sessions else None,
                "last": sessions[-1].isoformat() if sessions else None,
                "eval_bars": sum(day in have for day in eval_days),
                "warmup_bars": sum(day in have for day in warmup),
                "missing_eval": sum(day not in have for day in eval_days),
            })
    payload = {
        "universe": len(NAMES),
        "calendar_sessions": len(calendar),
        "warmup_sessions": len(warmup),
        "eval_sessions": len(eval_days),
        "eval_first": eval_days[0].isoformat(),
        "eval_last": eval_days[-1].isoformat(),
        "warmup_first": warmup[0].isoformat() if warmup else None,
        "warmup_last": warmup[-1].isoformat() if warmup else None,
        "names": rows,
        "note": "Bar counts and session spans only. No return, forward return, hit rate, or P&L.",
    }
    (HERE / "counts.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    thin = [row for row in rows if row["symbol"] not in ("QQQ", "SPY") and row["warmup_bars"] < 249]
    print(f"universe {len(NAMES)} eval_sessions {len(eval_days)} warmup_sessions {len(warmup)}")
    print(f"names with fewer than 249 warmup bars: {len(thin)}")
    for row in thin:
        print(f"  {row['symbol']} first {row['first']} bars {row['bars']} warmup {row['warmup_bars']} missing_eval {row['missing_eval']}")
    holes = [row for row in rows if row["missing_eval"] > 0]
    print(f"names missing at least one evaluated session: {len(holes)}")
    for row in holes:
        print(f"  {row['symbol']} missing_eval {row['missing_eval']} first {row['first']} last {row['last']} bars {row['bars']}")


if __name__ == "__main__":
    main()
