# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Count SSR episodes the store can price. No prices and no returns.

An episode starts on an NYSE session when the instrument's current symbol is
restricted and the previous NYSE session was not. A tradeable episode is one
whose next NYSE session has a daily bar. Listing opened_at is a store-binding
time, so it is not a market listing window. This script does not read bars.

When both publishers restrict the same session, the source is nasdaq.
"""

from __future__ import annotations

import json
import sys
from collections import defaultdict
from datetime import date, datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import (  # noqa: E402
    MarketData,
    is_nyse_holiday,
    nyse_sessions,
    session_date_of,
    ymd_from_int,
    ymd_int,
)

HERE = Path(__file__).resolve().parent
OOS_START = date(2024, 7, 1)
FIRST_FILE = date(2011, 2, 25)
LAST_DAY = date(2026, 10, 14)


def listing_date(stamp: str | int | None) -> date | None:
    """New York calendar date of a listing timestamp. None stays open-ended."""
    if stamp is None or stamp == "":
        return None
    from mdq import ny_datetime

    if isinstance(stamp, str):
        instant = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
        ts = int(instant.timestamp())
    else:
        ts = int(stamp)
    return ny_datetime(ts).date()


def _open_on(opened: date | None, closed: date | None, day: date) -> bool:
    if opened is not None and day < opened:
        return False
    if closed is not None and day >= closed:
        return False
    return True


def _cover_symbol(rows: list[dict], sessions: list[date], index: dict[date, int]) -> dict[date, str]:
    """Restricted sessions for one published symbol. Nasdaq overwrites NYSE Group."""
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
        cursor = index.get(start)
        if cursor is None:
            # First stored session on or after the trigger.
            cursor = 0
            while cursor < last and sessions[cursor] < start:
                cursor += 1
        while cursor < last and sessions[cursor] < stop:
            day = sessions[cursor]
            if covered.get(day) != "nasdaq":
                covered[day] = "nyse_group"
            cursor += 1
    return covered


def main() -> None:
    sessions = nyse_sessions(FIRST_FILE, LAST_DAY)
    index = {day: i for i, day in enumerate(sessions)}
    previous = {sessions[i]: sessions[i - 1] for i in range(1, len(sessions))}
    nxt = {sessions[i]: sessions[i + 1] for i in range(len(sessions) - 1)}
    is_end = max(day for day in sessions if day < OOS_START)

    with MarketData() as md:
        instruments = md.instruments()
        bar_days: dict[int, set[int]] = defaultdict(set)
        for row in md.sql(
            "SELECT instrument_id, session_date FROM coverage_day "
            "WHERE timeframe_s = 86400 AND bar_count > 0"
        ):
            bar_days[row["instrument_id"]].add(row["session_date"])

        dividend_days: set[tuple[int, int]] = set()
        for row in md.sql("SELECT instrument_id, ex_ts FROM corporate_action WHERE type = 'dividend'"):
            dividend_days.add((row["instrument_id"], ymd_int(session_date_of(row["ex_ts"]))))

        priced = [inst for inst in instruments if bar_days.get(inst["id"])]
        priced_ids = {inst["id"] for inst in priced}
        by_id = {inst["id"]: inst for inst in priced}

        windows: dict[int, dict[str, list[tuple[date | None, date | None]]]] = defaultdict(
            lambda: defaultdict(list)
        )
        listing_rows = 0
        for row in md.sql(
            "SELECT instrument_id, symbol, opened_at, closed_at FROM instrument_listing"
        ):
            if row["instrument_id"] not in priced_ids:
                continue
            listing_rows += 1
            symbol = row["symbol"].strip().upper()
            windows[row["instrument_id"]][symbol].append(
                (listing_date(row["opened_at"]), listing_date(row["closed_at"]))
            )

        wanted = {symbol for spans in windows.values() for symbol in spans}
        rows_by_symbol: dict[str, list[dict]] = defaultdict(list)
        ssr_rows_read = 0
        for row in md.sql(
            "SELECT source, symbol, file_session, trigger_session, deactivate_session "
            "FROM short_sale_restriction"
        ):
            ssr_rows_read += 1
            symbol = str(row["symbol"]).strip().upper()
            if symbol in wanted:
                rows_by_symbol[symbol].append(row)

        spy = next((inst for inst in instruments if inst["symbol"] == "SPY"), None)
        spy_days = sorted(bar_days.get(spy["id"], set())) if spy else []
        spy_today = md.sql(
            "SELECT session_date, bar_count, status FROM coverage_day "
            "WHERE instrument_id = ? AND timeframe_s = 86400 AND session_date = ?",
            (spy["id"] if spy else -1, ymd_int(LAST_DAY)),
        )

        # instrument_listing.opened_at is when this store bound the ticker.
        binding_window_restricted = 0
        for inst_id, spans_by_symbol in windows.items():
            for symbol, spans in spans_by_symbol.items():
                raw = _cover_symbol(rows_by_symbol.get(symbol, []), sessions, index)
                for day in raw:
                    if any(_open_on(opened, closed, day) for opened, closed in spans):
                        binding_window_restricted += 1
        opened_values = [
            row["opened_at"]
            for row in md.sql("SELECT opened_at FROM instrument_listing")
            if row["opened_at"] is not None
        ]

        # Study count: current symbol, no binding-date filter. Nasdaq wins per session.
        episodes = []
        for inst in priced:
            symbol = inst["symbol"].strip().upper()
            raw = _cover_symbol(rows_by_symbol.get(symbol, []), sessions, index)
            days = bar_days[inst["id"]]
            for day, source in raw.items():
                prior = previous.get(day)
                if prior is not None and prior in raw:
                    continue
                entry = nxt.get(day)
                if entry is None:
                    continue
                entry_int = ymd_int(entry)
                entry_i = index[entry]
                hold5_bars = entry_i + 4 < len(sessions) and all(
                    ymd_int(sessions[entry_i + offset]) in days for offset in range(5)
                )
                episodes.append(
                    {
                        "id": inst["id"],
                        "symbol": inst["symbol"],
                        "asset_class": inst["asset_class"],
                        "source": source,
                        "signal": day.isoformat(),
                        "entry": entry.isoformat(),
                        "has_bar": entry_int in days,
                        "signal_bar": ymd_int(day) in days,
                        "hold5_bars": hold5_bars,
                        "dividend": (inst["id"], entry_int) in dividend_days,
                    }
                )

    tradeable = [row for row in episodes if row["has_bar"]]
    oos = [row for row in tradeable if date.fromisoformat(row["entry"]) >= OOS_START]
    by_entry: dict[str, int] = defaultdict(int)
    by_symbol: dict[str, int] = defaultdict(int)
    for row in tradeable:
        by_entry[row["entry"]] += 1
        by_symbol[row["symbol"]] += 1
    classes: dict[str, int] = defaultdict(int)
    for row in tradeable:
        classes[row["asset_class"] or "unknown"] += 1

    summary = {
        "instruments": len(instruments),
        "instruments_with_daily_bars": len(priced),
        "listing_rows": listing_rows,
        "listing_symbols": len(wanted),
        "nyse_sessions": len(sessions),
        "is_end": is_end.isoformat(),
        "oos_start": OOS_START.isoformat(),
        "episode_starts": len(episodes),
        "tradeable_episodes": len(tradeable),
        "tradeable_missing_next_bar": len(episodes) - len(tradeable),
        "tradeable_oos_entry_on_or_after_2024_07_01": len(oos),
        "tradeable_before_oos": len(tradeable) - len(oos),
        "entry_sessions": len(by_entry),
        "entry_sessions_oos": len({row["entry"] for row in oos}),
        "entry_sessions_with_two_or_more_names": sum(1 for n in by_entry.values() if n >= 2),
        "max_names_on_one_entry_session": max(by_entry.values()) if by_entry else 0,
        "tradeable_nasdaq_source": sum(1 for row in tradeable if row["source"] == "nasdaq"),
        "tradeable_nyse_source": sum(1 for row in tradeable if row["source"] == "nyse_group"),
        "tradeable_nasdaq_oos": sum(1 for row in oos if row["source"] == "nasdaq"),
        "tradeable_nyse_oos": sum(1 for row in oos if row["source"] == "nyse_group"),
        "tradeable_symbols": len(by_symbol),
        "tradeable_symbols_list": sorted(by_symbol),
        "max_tradeable_per_symbol": max(by_symbol.values()) if by_symbol else 0,
        "tradeable_by_asset_class": dict(sorted(classes.items())),
        "entry_dividends": sum(1 for row in tradeable if row["dividend"]),
        "entry_dividends_oos": sum(1 for row in oos if row["dividend"]),
        "first_entry": min((row["entry"] for row in tradeable), default=None),
        "last_entry": max((row["entry"] for row in tradeable), default=None),
        "first_signal": min((row["signal"] for row in tradeable), default=None),
        "last_signal": max((row["signal"] for row in tradeable), default=None),
        "spy_daily_first": ymd_from_int(spy_days[0]).isoformat() if spy_days else None,
        "spy_daily_last": ymd_from_int(spy_days[-1]).isoformat() if spy_days else None,
        "spy_daily_sessions": len(spy_days),
        "spy_on_2026_10_14": [dict(row) for row in spy_today],
        "last_day_is_nyse_session": LAST_DAY in index,
        "last_day_is_holiday_flag": is_nyse_holiday(LAST_DAY),
        "tradeable_with_signal_bar": sum(1 for row in tradeable if row["signal_bar"]),
        "tradeable_with_hold5_bars": sum(1 for row in tradeable if row["hold5_bars"]),
        "tradeable_oos_with_hold5_bars": sum(1 for row in oos if row["hold5_bars"]),
        "ssr_rows_read": ssr_rows_read,
        "ssr_rows_on_listing_symbols": sum(len(v) for v in rows_by_symbol.values()),
        "listing_symbols_with_any_ssr_row": sum(1 for v in rows_by_symbol.values() if v),
        "binding_window_restricted_sessions": binding_window_restricted,
        "listing_opened_min_unix": min(opened_values) if opened_values else None,
        "listing_opened_max_unix": max(opened_values) if opened_values else None,
    }
    (HERE / "counts.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    shown = dict(summary)
    shown["tradeable_symbols_list"] = f"{len(by_symbol)} symbols"
    for key, value in shown.items():
        print(f"{key}: {value}")


if __name__ == "__main__":
    main()
