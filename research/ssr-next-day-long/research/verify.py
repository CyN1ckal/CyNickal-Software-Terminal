# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent hold-1 check of trades.csv and daily.csv. Does not import backtest.py."""

from __future__ import annotations

import csv
import sys
from collections import defaultdict
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import EARLY_CLOSES, MarketData, nyse_sessions, ymd_from_int  # noqa: E402

HERE = Path(__file__).resolve().parent
SAMPLE_START = date(2015, 3, 25)
SAMPLE_END = date(2026, 10, 1)
COST = 0.001
EXCLUDE = frozenset({"VIXY", "UVXY"})


def cover_symbol(rows: list[dict], sessions: list[date]) -> dict[date, str]:
    index = {day: i for i, day in enumerate(sessions)}
    covered: dict[date, str] = {}
    last = len(sessions)
    for row in rows:
        if row["source"] == "nasdaq":
            day = ymd_from_int(row["file_session"])
            if day in index:
                covered[day] = "nasdaq"
            continue
        start = ymd_from_int(row["trigger_session"])
        stop = ymd_from_int(row["deactivate_session"])
        cursor = 0 if start not in index else index[start]
        if start not in index:
            while cursor < last and sessions[cursor] < start:
                cursor += 1
        while cursor < last and sessions[cursor] < stop:
            day = sessions[cursor]
            if covered.get(day) != "nasdaq":
                covered[day] = "nyse_group"
            cursor += 1
    return covered


def clock(day: date, kind: str) -> str:
    if kind == "open":
        stamp = "09:30:00"
    else:
        stamp = "13:00:00" if day in EARLY_CLOSES else "16:00:00"
    return f"{day.isoformat()}T{stamp}"


def rebuild():
    calendar = nyse_sessions(date(2015, 3, 20), SAMPLE_END)
    book = nyse_sessions(SAMPLE_START, SAMPLE_END)
    previous = {calendar[i]: calendar[i - 1] for i in range(1, len(calendar))}
    nxt = {calendar[i]: calendar[i + 1] for i in range(len(calendar) - 1)}
    with MarketData() as md:
        instruments = [row for row in md.instruments() if row["symbol"] not in EXCLUDE and row["1d_sessions"]]
        wanted = {row["symbol"].strip().upper() for row in instruments}
        rows_by_symbol: dict[str, list[dict]] = defaultdict(list)
        for row in md.sql(
            "SELECT source, symbol, file_session, trigger_session, deactivate_session "
            "FROM short_sale_restriction"
        ):
            symbol = str(row["symbol"]).strip().upper()
            if symbol in wanted:
                rows_by_symbol[symbol].append(row)
        bars: dict[str, dict[date, object]] = {}
        episodes = []
        for inst in instruments:
            symbol = inst["symbol"].strip().upper()
            series = {}
            for bar in md.bars(inst["id"], "1d", start=date(2015, 3, 1), end=SAMPLE_END, adjust=True):
                series[bar.session] = bar
            if not series:
                continue
            bars[symbol] = series
            covered = cover_symbol(rows_by_symbol.get(symbol, []), calendar)
            for day, source in covered.items():
                prior = previous.get(day)
                if prior is not None and prior in covered:
                    continue
                entry = nxt.get(day)
                if entry is None:
                    continue
                episodes.append((entry, symbol, day, source))
        spy = {}
        for bar in md.bars("SPY", "1d", start=SAMPLE_START, end=SAMPLE_END, adjust=True):
            spy[bar.session] = bar
    sessions = [day for day in book if day in spy and spy[day].open > 0 and spy[day].close > 0]
    by_entry: dict[date, list[tuple]] = defaultdict(list)
    for entry, symbol, signal, source in episodes:
        if entry not in set(sessions):
            continue
        bar = bars.get(symbol, {}).get(entry)
        if bar is None or bar.open <= 0 or bar.close <= 0:
            continue
        by_entry[entry].append((symbol, signal, source, bar.open, bar.close))

    trades = []
    day_net = {day: 0.0 for day in sessions}
    for day in sessions:
        group = by_entry.get(day)
        if not group:
            continue
        weight = 1.0 / len(group)
        total = 0.0
        for symbol, signal, source, open_, close in sorted(group):
            ratio = close / open_
            gross = weight * (ratio - 1.0)
            net = gross - weight * COST - weight * ratio * COST
            total += net
            trades.append({
                "symbol": symbol,
                "session": day.isoformat(),
                "side": "long",
                "entry_time": clock(day, "open"),
                "exit_time": clock(day, "close"),
                "entry_price": open_,
                "exit_price": close,
                "net": net,
                "source": source,
                "signal": signal.isoformat(),
            })
        day_net[day] = total
    benchmark = [spy[day].close / spy[day].open - 1.0 for day in sessions]
    return sessions, trades, [day_net[day] for day in sessions], benchmark


def _load_csv(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def main() -> None:
    sessions, trades, nets, benchmark = rebuild()
    stored_trades = _load_csv(HERE / "trades.csv")
    stored_daily = _load_csv(HERE / "daily.csv")
    problems = []
    if len(stored_trades) != len(trades):
        problems.append(f"trade count {len(stored_trades)} != {len(trades)}")
    left = sorted(stored_trades, key=lambda row: (row["session"], row["symbol"]))
    right = sorted(trades, key=lambda row: (row["session"], row["symbol"]))
    for index, (got, want) in enumerate(zip(left, right)):
        for key in ("symbol", "session", "side", "entry_time", "exit_time", "source", "signal"):
            if got.get(key) != want[key]:
                problems.append(f"row {index} {key}: {got.get(key)!r} != {want[key]!r}")
                break
        else:
            if abs(float(got["entry_price"]) - want["entry_price"]) > 1e-6:
                problems.append(f"row {index} entry_price")
            elif abs(float(got["exit_price"]) - want["exit_price"]) > 1e-6:
                problems.append(f"row {index} exit_price")
            elif abs(float(got["net"]) - want["net"]) > 1e-8:
                problems.append(f"row {index} net {got['net']} != {want['net']}")
        if len(problems) > 12:
            break
    if len(stored_daily) != len(sessions):
        problems.append(f"daily rows {len(stored_daily)} != {len(sessions)}")
    for index, (row, day, net, bench) in enumerate(zip(stored_daily, sessions, nets, benchmark)):
        if row["session"] != day.isoformat():
            problems.append(f"daily {index} session {row['session']} != {day.isoformat()}")
            break
        if abs(float(row["strategy_net"]) - net) > 1e-8:
            problems.append(f"daily {day} strategy {row['strategy_net']} != {net}")
            break
        if abs(float(row["benchmark"]) - bench) > 1e-8:
            problems.append(f"daily {day} benchmark")
            break
    if problems:
        print("\n".join(problems))
        sys.exit(1)
    print(f"verify ok: {len(trades)} trades, {len(sessions)} sessions")


if __name__ == "__main__":
    main()
