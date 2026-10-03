# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock coverage counts. No prices, returns, or signals.

Run from the repo root: python research/qqq-holdings-obv-divergence/research/counts.py
"""

from __future__ import annotations

import json
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import MarketData, nyse_sessions  # noqa: E402

NAMES = (
    "NVDA AAPL MSFT MU AMD AMZN META GOOGL TSLA SPCX GOOG INTC AVGO WMT CSCO LRCX "
    "PLTR AMAT COST PANW NFLX CRWD KLAC TXN SNDK MRVL LIN AMGN ADI QCOM STX SHOP "
    "GILD ASML TMUS PEP WDC ISRG ARM FTNT VRTX BKNG SBUX ADP LITE CDNS ADBE SNPS "
    "MAR DDOG CEG CSX MELI MNST APP WBD DASH CTAS INTU CMCSA MDLZ REGN ROST MPWR "
    "TER ORLY ABNB HON AEP NXPI ALAB MSTR FAST NBIS PCAR BKR FANG PDD HONA PYPL "
    "XEL ADSK RKLB MCHP CCEP EXC KDP CRWV IDXX FER TTWO ODFL TRI WDAY PAYX ROP "
    "AXON DXCM ALNY GEHC CPRT"
).split()

EVAL_START = date(2022, 10, 3)
EVAL_END = date(2026, 10, 2)
STORE_START = date(2021, 10, 4)


def main() -> None:
    calendar = nyse_sessions(STORE_START, EVAL_END)
    evaluated = [day for day in calendar if EVAL_START <= day <= EVAL_END]
    out = {
        "n_names": len(NAMES),
        "unique_names": len(set(NAMES)),
        "store_sessions": len(calendar),
        "evaluated_sessions": len(evaluated),
        "eval_first": evaluated[0].isoformat(),
        "eval_last": evaluated[-1].isoformat(),
        "names": [],
    }
    missing_names = []
    with MarketData() as md:
        for symbol in NAMES + ["QQQ", "SPY"]:
            bars = md.bars(symbol, "1d", start=STORE_START, end=EVAL_END)
            sessions = [bar.session for bar in bars]
            have = set(sessions)
            first = sessions[0].isoformat() if sessions else None
            last = sessions[-1].isoformat() if sessions else None
            span = [day for day in calendar if sessions and sessions[0] <= day <= sessions[-1]]
            missing = [day.isoformat() for day in span if day not in have]
            eval_missing = [day for day in missing if day >= EVAL_START.isoformat()]
            if not sessions:
                missing_names.append(symbol)
            out["names"].append({
                "symbol": symbol,
                "bars": len(sessions),
                "first": first,
                "last": last,
                "missing_in_span": len(missing),
                "missing_in_eval_span": len(eval_missing),
                "missing_dates": missing,
            })
    out["unresolved_or_empty"] = missing_names
    path = Path(__file__).resolve().parent / "counts.json"
    path.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"names {out['n_names']} unique {out['unique_names']}")
    print(f"calendar {out['store_sessions']} evaluated {out['evaluated_sessions']}")
    print(f"empty {missing_names}")
    for row in out["names"]:
        if row["missing_in_span"] or row["bars"] == 0 or row["first"] != STORE_START.isoformat():
            print(
                f"{row['symbol']} bars {row['bars']} {row['first']} {row['last']} "
                f"missing {row['missing_in_span']} eval_missing {row['missing_in_eval_span']}"
            )


if __name__ == "__main__":
    main()
