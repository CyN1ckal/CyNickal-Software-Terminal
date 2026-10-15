# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Parse Nasdaq and NYSE Group short-sale restriction files.

The Nasdaq file for a session lists names restricted that session. The NYSE
Group workbook lists that session's triggers and the date the restriction is
expected to lift. See docs/short-sale-restriction.md.
"""

from __future__ import annotations

import csv
import io
from dataclasses import dataclass
from datetime import datetime
from zoneinfo import ZoneInfo

NY = ZoneInfo("America/New_York")

NASDAQ_HEADER = ["Symbol", "Security Name", "Market Category", "Trigger Time"]
NYSE_REQUIRED = (
    "Trigger Date",
    "Issue Symbol",
    "Issue Name",
    "Primary Exchange",
    "Trigger Time",
    "Anticipated Deactivation Date",
)
NYSE_STATE_HEADERS = ("SSR State", "SSR Status")
_SYMBOL_CHARS = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ./-")


class ParseError(ValueError):
    """The file is not a short-sale restriction file we can store."""


@dataclass(frozen=True)
class Restriction:
    source: str
    file_session: int
    symbol: str
    security_name: str | None
    listing_market: str
    trigger_session: int
    trigger_ts: int
    deactivate_session: int | None
    ssr_state: str | None


@dataclass(frozen=True)
class ParsedFile:
    source: str
    file_session: int
    file_name: str
    file_created_at: int | None
    rows: tuple[Restriction, ...]


def ymd(day: datetime) -> int:
    return day.year * 10000 + day.month * 100 + day.day


def _published_symbol(raw: str) -> str:
    text = raw.strip().upper()
    if not text or any(ch not in _SYMBOL_CHARS for ch in text):
        raise ParseError(f"symbol {raw!r} is empty or has an unexpected character")
    return text


def _market(raw: str) -> str:
    text = raw.strip().upper()
    if not text or any(not ch.isalnum() for ch in text):
        raise ParseError(f"listing market {raw!r} is empty or has an unexpected character")
    return text


def _name(raw: str) -> str | None:
    text = raw.strip()
    return text or None


def _ny_timestamp(local: datetime) -> int:
    if local.tzinfo is None:
        raise ParseError("trigger time is missing a time zone")
    return int(local.timestamp())


def _collapse(rows: list[Restriction]) -> tuple[Restriction, ...]:
    """Drop a repeated line. Two lines with the same key and different fields fail."""
    kept: dict[tuple[str, int], Restriction] = {}
    order: list[tuple[str, int]] = []
    for row in rows:
        key = (row.symbol, row.trigger_ts)
        prior = kept.get(key)
        if prior is None:
            kept[key] = row
            order.append(key)
            continue
        if prior != row:
            raise ParseError(
                f"{row.symbol} is published twice at {row.trigger_ts} with different fields"
            )
    return tuple(kept[key] for key in order)


def parse_nasdaq(text: str, file_session: int, file_name: str) -> ParsedFile:
    if text.lstrip().startswith("<") or "Incapsula" in text[:500]:
        raise ParseError(f"{file_name} is an HTML page, not a Nasdaq circuit-breaker file")
    # The first archive day is this sentence and nothing else. It is a
    # published session with no restricted names, not a missing file.
    if text.strip().lower() == "no data available":
        return ParsedFile("nasdaq", file_session, file_name, None, ())
    grid = [row for row in csv.reader(io.StringIO(text)) if any(cell.strip() for cell in row)]
    if len(grid) < 2:
        raise ParseError(f"{file_name} has no header and trailer")
    header = [cell.strip() for cell in grid[0]]
    if header != NASDAQ_HEADER:
        raise ParseError(f"{file_name} header is {header}, not {NASDAQ_HEADER}")
    trailer = grid[-1]
    if len(trailer) != 1 or len(trailer[0].strip()) != 14 or not trailer[0].strip().isdigit():
        raise ParseError(f"{file_name} trailer is {trailer!r}")
    stamp = datetime.strptime(trailer[0].strip(), "%Y%m%d%H%M%S").replace(tzinfo=NY)
    if ymd(stamp) != file_session:
        raise ParseError(f"{file_name} trailer date is {ymd(stamp)}, not {file_session}")

    rows: list[Restriction] = []
    for line in grid[1:-1]:
        if len(line) != 4:
            raise ParseError(f"{file_name} has a row with {len(line)} fields: {line!r}")
        trigger = datetime.strptime(line[3].strip(), "%m/%d/%Y %I:%M:%S %p").replace(tzinfo=NY)
        rows.append(
            Restriction(
                source="nasdaq",
                file_session=file_session,
                symbol=_published_symbol(line[0]),
                security_name=_name(line[1]),
                listing_market=_market(line[2]),
                trigger_session=ymd(trigger),
                trigger_ts=_ny_timestamp(trigger),
                deactivate_session=None,
                ssr_state=None,
            )
        )
    return ParsedFile("nasdaq", file_session, file_name, _ny_timestamp(stamp), _collapse(rows))


def parse_nyse_grid(grid: list[list[str]], file_session: int, file_name: str) -> ParsedFile:
    if not grid or not any(cell.strip() for cell in grid[0]):
        raise ParseError(f"{file_name} has no header")
    header = [cell.strip() for cell in grid[0]]
    index = {name: pos for pos, name in enumerate(header) if name}
    missing = [name for name in NYSE_REQUIRED if name not in index]
    state_header = next((name for name in NYSE_STATE_HEADERS if name in index), None)
    if missing or state_header is None:
        raise ParseError(f"{file_name} header is missing {missing or list(NYSE_STATE_HEADERS)}")

    def cell(row: list[str], name: str) -> str:
        pos = index[name]
        if pos >= len(row):
            raise ParseError(f"{file_name} row is short of {name}: {row!r}")
        return row[pos].strip()

    rows: list[Restriction] = []
    for raw in grid[1:]:
        if not any(cell.strip() for cell in raw):
            continue
        trigger_day = datetime.strptime(cell(raw, "Trigger Date"), "%m/%d/%Y")
        clock = datetime.strptime(cell(raw, "Trigger Time"), "%H:%M:%S")
        trigger = datetime(
            trigger_day.year,
            trigger_day.month,
            trigger_day.day,
            clock.hour,
            clock.minute,
            clock.second,
            tzinfo=NY,
        )
        deactivation = datetime.strptime(cell(raw, "Anticipated Deactivation Date"), "%m/%d/%Y")
        rows.append(
            Restriction(
                source="nyse_group",
                file_session=file_session,
                symbol=_published_symbol(cell(raw, "Issue Symbol")),
                security_name=_name(cell(raw, "Issue Name")),
                listing_market=_market(cell(raw, "Primary Exchange")),
                trigger_session=ymd(trigger),
                trigger_ts=_ny_timestamp(trigger),
                deactivate_session=ymd(deactivation),
                ssr_state=_market(cell(raw, state_header)),
            )
        )
    return ParsedFile("nyse_group", file_session, file_name, None, _collapse(rows))


def _xls_cell_text(cell: object, datemode: int) -> str:
    import xlrd

    ctype = getattr(cell, "ctype")
    value = getattr(cell, "value")
    if ctype == xlrd.XL_CELL_EMPTY:
        return ""
    if ctype == xlrd.XL_CELL_DATE:
        stamp = xlrd.xldate_as_datetime(value, datemode)
        if stamp.year < 1901:
            return stamp.strftime("%H:%M:%S")
        if (stamp.hour, stamp.minute, stamp.second) == (0, 0, 0):
            return stamp.strftime("%m/%d/%Y")
        return stamp.strftime("%m/%d/%Y %H:%M:%S")
    if ctype == xlrd.XL_CELL_NUMBER and value == int(value):
        return str(int(value))
    return str(value).strip()


def parse_nyse_xls(data: bytes, file_session: int, file_name: str) -> ParsedFile:
    if data[:4] != b"\xd0\xcf\x11\xe0":
        raise ParseError(f"{file_name} is not a BIFF workbook")
    try:
        import xlrd
    except ImportError as exc:
        raise ParseError("reading an NYSE workbook requires xlrd") from exc
    book = xlrd.open_workbook(file_contents=data)
    if book.nsheets < 1:
        raise ParseError(f"{file_name} has no sheet")
    sheet = book.sheet_by_index(0)
    grid = [
        [_xls_cell_text(sheet.cell(row, col), book.datemode) for col in range(sheet.ncols)]
        for row in range(sheet.nrows)
    ]
    return parse_nyse_grid(grid, file_session, file_name)
