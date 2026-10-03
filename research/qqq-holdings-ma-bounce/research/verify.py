# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the locked 150/250 bounce. Does not import the study engine.

Run from the repo root:

    python research/qqq-holdings-ma-bounce/research/verify.py
"""

from __future__ import annotations

import csv
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

FIRST = date(2021, 10, 4)
LAST = date(2026, 10, 2)
EVAL_START = date(2022, 10, 3)
FAST = 150
SLOW = 250
HOLD = 20
LAG = 1

NAMES = (
    "NVDA AAPL MSFT MU AMD AMZN META GOOGL TSLA SPCX GOOG INTC AVGO WMT CSCO LRCX "
    "PLTR AMAT COST PANW NFLX CRWD KLAC TXN SNDK MRVL LIN AMGN ADI QCOM STX SHOP GILD "
    "ASML TMUS PEP WDC ISRG ARM FTNT VRTX BKNG SBUX ADP LITE CDNS ADBE SNPS MAR DDOG "
    "CEG CSX MELI MNST APP WBD DASH CTAS INTU CMCSA MDLZ REGN ROST MPWR TER ORLY ABNB "
    "HON AEP NXPI ALAB MSTR FAST NBIS PCAR BKR FANG PDD HONA PYPL XEL ADSK RKLB MCHP "
    "CCEP EXC KDP CRWV IDXX FER TTWO ODFL TRI WDAY PAYX ROP AXON DXCM ALNY GEHC CPRT"
).split()


def averages(has_close: list[float | None]) -> tuple[list[float | None], list[float | None]]:
    """Prefix sum of the closes that exist. Same oldest-to-newest order as the study."""
    fast: list[float | None] = [None] * len(has_close)
    slow: list[float | None] = [None] * len(has_close)
    prefix = [0.0]
    seen: list[int] = []
    for i, close in enumerate(has_close):
        if close is None:
            continue
        prefix.append(prefix[-1] + close)
        seen.append(i)
    for k, i in enumerate(seen):
        if k + 1 >= FAST:
            fast[i] = (prefix[k + 1] - prefix[k + 1 - FAST]) / FAST
        if k + 1 >= SLOW:
            slow[i] = (prefix[k + 1] - prefix[k + 1 - SLOW]) / SLOW
    return fast, slow


def replay(bars: dict[date, tuple[float, float, float, float]], dates: list[date]) -> list[tuple]:
    """Naive long-only state machine. Returns (side, entry_time, entry_price, exit_time, exit_price)."""
    closes: list[float | None] = []
    for day in dates:
        row = bars.get(day)
        closes.append(None if row is None else row[3])
    fast, slow = averages(closes)
    first = next(i for i, day in enumerate(dates) if day >= EVAL_START)
    held = False
    entry_i = -1
    entry_px = 0.0
    wait_entry: int | None = None
    wait_exit: tuple[int, str] | None = None
    trades = []

    for i, day in enumerate(dates):
        if i < first:
            continue
        row = bars.get(day)
        if row is not None:
            price = row[0]
            if held and wait_exit is not None and wait_exit[0] <= i:
                trades.append((
                    "long",
                    f"{dates[entry_i].isoformat()}T09:30:00",
                    entry_px,
                    f"{day.isoformat()}T09:30:00",
                    price,
                ))
                held = False
                wait_exit = None
            elif held and i >= entry_i + HOLD:
                trades.append((
                    "long",
                    f"{dates[entry_i].isoformat()}T09:30:00",
                    entry_px,
                    f"{day.isoformat()}T09:30:00",
                    price,
                ))
                held = False
                wait_exit = None
            if (not held) and wait_entry is not None and wait_entry <= i:
                held = True
                entry_i = i
                entry_px = price
                wait_entry = None
                wait_exit = None
        if row is None or slow[i] is None:
            continue
        _o, _h, low, close = row
        if held and close < slow[i]:
            nxt = i + LAG
            if wait_exit is None or nxt < wait_exit[0]:
                wait_exit = (nxt, "trend")
        if held or wait_entry is not None or fast[i] is None:
            continue
        lower = fast[i] if fast[i] < slow[i] else slow[i]
        upper = slow[i] if fast[i] < slow[i] else fast[i]
        trend = close > slow[i] and fast[i] > slow[i]
        touch = lower <= low <= upper
        bounce = close > fast[i]
        if trend and touch and bounce:
            wait_entry = i + LAG
    return trades


def main() -> None:
    dates = nyse_sessions(FIRST, LAST)
    stored = []
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            stored.append((
                row["symbol"],
                row["side"],
                row["entry_time"],
                float(row["entry_price"]),
                row["exit_time"],
                float(row["exit_price"]),
            ))
    fresh: list[tuple] = []
    with MarketData() as md:
        for symbol in NAMES:
            bars = {}
            for bar in md.bars(symbol, "1d", start=FIRST, end=LAST, adjust=True):
                bars[bar.session] = (float(bar.open), float(bar.high), float(bar.low), float(bar.close))
            for side, entry_time, entry_px, exit_time, exit_px in replay(bars, dates):
                fresh.append((symbol, side, entry_time, entry_px, exit_time, exit_px))
    fresh.sort()
    stored.sort()
    if len(fresh) != len(stored):
        raise SystemExit(f"trade count {len(fresh)} != {len(stored)}")
    mismatches = 0
    for got, want in zip(fresh, stored):
        same = (
            got[0] == want[0]
            and got[1] == want[1]
            and got[2] == want[2]
            and got[3] == want[3]
            and got[4] == want[4]
            and got[5] == want[5]
        )
        if not same:
            mismatches += 1
            if mismatches <= 8:
                print(f"mismatch stored {want} replay {got}")
    if mismatches:
        raise SystemExit(f"{mismatches} trades did not match")
    print(f"matched {len(stored)} trades")


if __name__ == "__main__":
    main()
