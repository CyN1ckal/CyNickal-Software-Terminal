# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the locked primary. Does not import the study signal."""

from __future__ import annotations

import csv
import math
import sys
from datetime import date
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import MarketData, nyse_sessions  # noqa: E402
from research.kit import assert_lock  # noqa: E402

HERE = Path(__file__).resolve().parent
QQQ = "QQQ"
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
LOOKBACK = 126
SKIP = 21
STEP = 21
MIN_NAMES = 5
COST = 0.0005
SEED = 20261003
EVAL_START = date(2022, 10, 3)
EVAL_END = date(2026, 10, 2)


def positive(value) -> bool:
    return isinstance(value, float) and math.isfinite(value) and value > 0.0


def ranked_at(closes, t: int) -> list[tuple[str, float]]:
    end = t - SKIP
    start = end - LOOKBACK
    if start < 0:
        return []
    q0, q1 = closes[QQQ][start], closes[QQQ][end]
    if not (positive(q0) and positive(q1)):
        return []
    market = q1 / q0 - 1.0
    rows = []
    for symbol in TICKERS:
        a, b, c = closes[symbol][start], closes[symbol][end], closes[symbol][t]
        if not (positive(a) and positive(b) and positive(c)):
            continue
        rows.append((symbol, (b / a - 1.0) - market))
    rows.sort(key=lambda row: (-row[1], row[0]))
    return rows


def weights_of(rows: list[tuple[str, float]]) -> dict[str, float]:
    if len(rows) < MIN_NAMES:
        return {}
    k = max(1, len(rows) // 5)
    weight = 1.0 / k
    return {symbol: weight for symbol, _excess in rows[:k]}


def replay(calendar, opens, times, closes) -> list[dict]:
    evaluated = [i for i, day in enumerate(calendar) if EVAL_START <= day <= EVAL_END]
    first = None
    for pos, index in enumerate(evaluated):
        if index - SKIP - LOOKBACK < 0:
            continue
        if len(ranked_at(closes, index)) >= MIN_NAMES:
            first = pos
            break
    signals = [] if first is None else evaluated[first::STEP]
    targets = {}
    for signal in signals:
        fill = signal + 1
        if fill < len(calendar) and EVAL_START <= calendar[fill] <= EVAL_END:
            targets[fill] = weights_of(ranked_at(closes, signal))

    lots: dict[str, dict] = {}
    live: dict[str, float] = {}
    trades: list[dict] = []

    def shut(symbol: str, index: int, exit_cost: float, reason: str) -> None:
        lot = lots.pop(symbol)
        trades.append({
            "session": calendar[lot["entry"]].isoformat(),
            "symbol": symbol,
            "side": "long",
            "entry_time": times[symbol][lot["entry"]],
            "entry_price": lot["price"],
            "exit_time": times[symbol][index],
            "exit_price": opens[symbol][index],
            "exit_reason": reason,
        })

    def buy(symbol: str, index: int, weight: float) -> None:
        lots[symbol] = {"entry": index, "price": opens[symbol][index], "weight": weight}

    for index in evaluated:
        if index in targets:
            intended = targets[index]
            names = sorted(set(lots) | set(intended))
            tradable = {symbol: positive(opens[symbol][index]) for symbol in names}
            for symbol in names:
                old = lots[symbol]["weight"] if symbol in lots else 0.0
                want = intended.get(symbol, 0.0)
                if not tradable[symbol]:
                    continue
                if symbol in lots and (abs(want - old) > 1e-15 or old > 0.0):
                    shut(symbol, index, COST * max(old - want, 0.0), "rebalance")
                    if want > 1e-15:
                        buy(symbol, index, want)
                elif symbol not in lots and want > 1e-15:
                    buy(symbol, index, want)
            updated = {}
            for symbol, lot in lots.items():
                want = intended.get(symbol, 0.0)
                if not tradable.get(symbol, False) and want < lot["weight"] - 1e-15 and want > 1e-15:
                    updated[symbol] = want
                else:
                    updated[symbol] = lot["weight"]
            live = updated
            continue
        for symbol in sorted(list(lots)):
            want = live.get(symbol, 0.0)
            old = lots[symbol]["weight"]
            if old <= want + 1e-15 or not positive(opens[symbol][index]):
                continue
            shut(symbol, index, COST * (old - want), "rebalance")
            if want > 1e-15:
                buy(symbol, index, want)
                live[symbol] = want
            else:
                live.pop(symbol, None)

    if evaluated:
        last = evaluated[-1]
        for symbol in sorted(list(lots)):
            exit_i = last
            if not positive(opens[symbol][exit_i]):
                found = None
                for prior in range(len(evaluated) - 2, -1, -1):
                    candidate = evaluated[prior]
                    if positive(opens[symbol][candidate]):
                        found = candidate
                        break
                exit_i = lots[symbol]["entry"] if found is None else found
            shut(symbol, exit_i, 0.0, "sample_end")
    trades.sort(key=lambda row: (row["session"], row["symbol"]))
    return trades


def load_saved() -> list[dict]:
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    for row in rows:
        row["entry_price"] = float(row["entry_price"])
        row["exit_price"] = float(row["exit_price"])
    return rows


def same(got: dict, saved: dict) -> bool:
    if got["side"] != saved["side"]:
        return False
    if got["entry_time"] != saved["entry_time"] or got["exit_time"] != saved["exit_time"]:
        return False
    if abs(got["entry_price"] - saved["entry_price"]) > 1e-8:
        return False
    if abs(got["exit_price"] - saved["exit_price"]) > 1e-8:
        return False
    return True


def main() -> None:
    assert_lock(HERE)
    calendar = nyse_sessions(date(2021, 10, 4), EVAL_END)
    index = {day: i for i, day in enumerate(calendar)}
    symbols = list(TICKERS) + [QQQ]
    opens = {symbol: [math.nan] * len(calendar) for symbol in symbols}
    closes = {symbol: [math.nan] * len(calendar) for symbol in symbols}
    times = {symbol: [None] * len(calendar) for symbol in symbols}
    with MarketData() as md:
        for symbol in symbols:
            for bar in md.bars(symbol, "1d", start=calendar[0], end=calendar[-1]):
                slot = index.get(bar.session)
                if slot is None:
                    continue
                if isinstance(bar.open, float) and math.isfinite(bar.open) and bar.open > 0.0:
                    opens[symbol][slot] = float(bar.open)
                if isinstance(bar.close, float) and math.isfinite(bar.close) and bar.close > 0.0:
                    closes[symbol][slot] = float(bar.close)
                times[symbol][slot] = bar.time.isoformat(timespec="seconds")
    got = replay(calendar, opens, times, closes)
    saved = load_saved()
    if len(got) != len(saved):
        raise SystemExit(f"trade count {len(got)} != {len(saved)}")
    mismatches = []
    for pos, (left, right) in enumerate(zip(got, saved)):
        if left["symbol"] != right["symbol"] or left["session"] != right["session"] or not same(left, right):
            mismatches.append((pos, left, right))
            if len(mismatches) >= 8:
                break
    entry_sessions = sorted({row["session"] for row in got})
    take = min(40, len(entry_sessions))
    draw = np.random.default_rng(SEED).choice(len(entry_sessions), size=take, replace=False)
    picked = {entry_sessions[int(slot)] for slot in draw}
    sample_bad = [
        row["session"] for row, stored in zip(got, saved)
        if row["session"] in picked and (row["symbol"] != stored["symbol"] or not same(row, stored))
    ]
    if mismatches or sample_bad:
        for pos, left, right in mismatches:
            print("mismatch", pos)
            print(" got  ", left["session"], left["symbol"], left["side"], left["entry_time"], left["entry_price"], left["exit_time"], left["exit_price"])
            print(" saved", right["session"], right["symbol"], right["side"], right["entry_time"], right["entry_price"], right["exit_time"], right["exit_price"])
        raise SystemExit(f"verify failed: {len(mismatches)} shown, sample misses {len(sample_bad)}")
    print(f"verify matched {len(got)} trades, including {take} seeded entry sessions")


if __name__ == "__main__":
    main()
