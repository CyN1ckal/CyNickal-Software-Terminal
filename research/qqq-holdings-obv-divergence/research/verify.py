# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the primary OBV trades. Does not import backtest.py.

Run from the repo root:
    python research/qqq-holdings-obv-divergence/research/verify.py
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

NAMES = (
    "NVDA AAPL MSFT MU AMD AMZN META GOOGL TSLA SPCX GOOG INTC AVGO WMT CSCO LRCX "
    "PLTR AMAT COST PANW NFLX CRWD KLAC TXN SNDK MRVL LIN AMGN ADI QCOM STX SHOP "
    "GILD ASML TMUS PEP WDC ISRG ARM FTNT VRTX BKNG SBUX ADP LITE CDNS ADBE SNPS "
    "MAR DDOG CEG CSX MELI MNST APP WBD DASH CTAS INTU CMCSA MDLZ REGN ROST MPWR "
    "TER ORLY ABNB HON AEP NXPI ALAB MSTR FAST NBIS PCAR BKR FANG PDD HONA PYPL "
    "XEL ADSK RKLB MCHP CCEP EXC KDP CRWV IDXX FER TTWO ODFL TRI WDAY PAYX ROP "
    "AXON DXCM ALNY GEHC CPRT"
).split()

W = 5
MIN_SEP = 10
MAX_SEP = 60
HOLD = 20
EVAL_START = date(2022, 10, 3)
EVAL_END = date(2026, 10, 2)
STORE_START = date(2021, 10, 4)


def obv(closes, volumes):
    total = 0.0
    prev = None
    out = []
    for close, volume in zip(closes, volumes):
        if prev is not None and close != prev:
            total += volume if close > prev else -volume
        out.append(total)
        prev = close
    return out


def signals(bars, full_index):
    # bars are (session, open, close, volume). Swings and OBV use the close and the volume.
    closes = [row[2] for row in bars]
    line = obv(closes, [row[3] for row in bars])
    prior_low = None
    prior_high = None
    found = []
    for end in range(len(bars)):
        pivot = end - W
        bull = bear = False
        level = gap = None
        if pivot >= W:
            left = closes[pivot - W:pivot]
            right = closes[pivot + 1:end + 1]
            price = closes[pivot]
            low = all(price < value for value in left + right)
            high = all(price > value for value in left + right)
            day = bars[pivot][0]
            if low and prior_low is not None:
                span = full_index[day] - full_index[prior_low[0]]
                if MIN_SEP <= span <= MAX_SEP and price < prior_low[1] and line[pivot] > prior_low[2]:
                    bull = True
                    level = price
                    gap = abs(price - prior_low[1])
            if high and prior_high is not None:
                span = full_index[day] - full_index[prior_high[0]]
                if MIN_SEP <= span <= MAX_SEP and price > prior_high[1] and line[pivot] < prior_high[2]:
                    bear = True
                    level = price
                    gap = abs(price - prior_high[1])
            if low:
                prior_low = (day, price, line[pivot])
            if high:
                prior_high = (day, price, line[pivot])
        day = bars[end][0]
        if EVAL_START <= day <= EVAL_END and bull != bear and (bull or bear):
            found.append((end, day, "long" if bull else "short", level, gap))
    return found


def trades_for(symbol, bars, eval_index, last_index, full_index):
    events = {}
    for item in signals(bars, full_index):
        events.setdefault(item[0], []).append(item)
    out = []
    state = "flat"
    pending = None
    waited = 0
    entry_at = None
    side = level = gap = confirm = None
    entry_index = None
    fail_next = False
    for pos, row in enumerate(bars):
        day, open_, close, _volume = row
        k = eval_index.get(day)
        if state == "pending" and day > pending[1] and k is not None:
            waited += 1
            if waited >= 1:
                state = "open"
                entry_at = pos
                entry_index = k
                side = pending[2]
                level = pending[3]
                gap = pending[4]
                confirm = pending[1]
                fail_next = False
                pending = None
        if state == "open" and pos > entry_at and k is not None:
            if fail_next or k >= entry_index + HOLD:
                reason = "failure" if fail_next else "time"
                out.append(_row(symbol, side, confirm, bars[entry_at], row, reason, entry_index, k, level, gap))
                state = "flat"
                fail_next = False
        if state == "open" and k is not None:
            if (side == "long" and close < level) or (side == "short" and close > level):
                fail_next = True
        if state == "flat":
            group = events.get(pos, [])
            sides = {item[2] for item in group}
            if len(group) == 1 and not ({"long", "short"} <= sides):
                state = "pending"
                pending = group[0]
                waited = 0
    if state == "open":
        last = bars[entry_at]
        for row in bars:
            k = eval_index.get(row[0])
            if k is not None and entry_index <= k <= last_index:
                last = row
        out.append(_row(symbol, side, confirm, bars[entry_at], last, "sample_end", entry_index, last_index + 1, level, gap))
    return out


def _row(symbol, side, confirm, entry, exit_row, reason, entry_index, exclusive_end, level, gap):
    return {
        "symbol": symbol,
        "side": side,
        "entry_time": f"{entry[0].isoformat()}T09:30:00",
        "entry_price": entry[1],
        "exit_time": f"{exit_row[0].isoformat()}T09:30:00",
        "exit_price": exit_row[1],
        "exit_reason": reason,
    }


def main() -> None:
    calendar = nyse_sessions(STORE_START, EVAL_END)
    evaluated = [day for day in calendar if EVAL_START <= day <= EVAL_END]
    full_index = {day: i for i, day in enumerate(calendar)}
    eval_index = {day: i for i, day in enumerate(evaluated)}
    last_index = len(evaluated) - 1
    fresh = []
    with MarketData() as md:
        for symbol in NAMES:
            rows = md.bars(symbol, "1d", start=STORE_START, end=EVAL_END)
            bars = [(bar.session, float(bar.open), float(bar.close), float(bar.volume)) for bar in rows]
            fresh.extend(trades_for(symbol, bars, eval_index, last_index, full_index))
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        saved = list(csv.DictReader(handle))
    def key(row):
        return (
            row["symbol"],
            row["side"],
            row["entry_time"],
            round(float(row["entry_price"]), 8),
            row["exit_time"],
            round(float(row["exit_price"]), 8),
            row.get("exit_reason", ""),
        )
    got = sorted(key(row) for row in fresh)
    want = sorted(key(row) for row in saved)
    if got == want:
        print(f"matched {len(got)} trades")
        return
    print(f"mismatch got {len(got)} saved {len(want)}", file=sys.stderr)
    pairs = min(len(got), len(want))
    shown = 0
    for left, right in zip(got, want):
        if left != right:
            print("got  ", left, file=sys.stderr)
            print("saved", right, file=sys.stderr)
            shown += 1
            if shown >= 8:
                break
    if len(got) != len(want):
        extra = got[pairs:pairs + 4] if len(got) > len(want) else want[pairs:pairs + 4]
        print("tail", extra, file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
    main()
