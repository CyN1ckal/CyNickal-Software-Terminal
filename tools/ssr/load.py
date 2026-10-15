# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Load published short-sale restriction files into the market-data store.

Nasdaq files come from ftp://ftp.nasdaqtrader.com/SymbolDirectory/shorthalts/.
NYSE Group workbooks come from https://ftp.nyse.com/NYSEGroupSSRCircuitBreakers/.
Reading an NYSE workbook requires xlrd. The store must already be schema 6 or 7.
This does not call MBoum. See docs/short-sale-restriction.md.
"""

from __future__ import annotations

import argparse
import sqlite3
import sys
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from datetime import date, datetime, timedelta
from pathlib import Path

from parse import ParseError, ParsedFile, parse_nasdaq, parse_nyse_xls

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DB = ROOT / "data" / "market-data.sqlite"
SCHEMA_V7 = ROOT / "libs" / "market-data" / "schema" / "v7.sql"
USER_AGENT = "CyNickal-Software-Terminal/ssr-load"
WORKERS = 4
NYSE_WORKERS = 2
ATTEMPTS = 4
# ftp.nyse.com answers 503 when a real workbook is busy. A short retry
# gives up and the file is left unstored. These pauses are only for that.
BUSY_DELAYS = (2, 4, 8, 15, 30, 30, 30)

NASDAQ_FLOOR = date(2011, 2, 25)
NYSE_FLOOR = date(2015, 1, 1)


class SchemaError(RuntimeError):
    pass


class FetchError(RuntimeError):
    pass


class MissingFile(Exception):
    pass


def parse_cli_date(text: str) -> date:
    cleaned = text.strip()
    if len(cleaned) == 8 and cleaned.isdigit():
        return datetime.strptime(cleaned, "%Y%m%d").date()
    return datetime.strptime(cleaned, "%Y-%m-%d").date()


def weekdays(start: date, end: date) -> list[date]:
    days: list[date] = []
    cursor = start
    while cursor <= end:
        if cursor.weekday() < 5:
            days.append(cursor)
        cursor += timedelta(days=1)
    return days


def session_of(day: date) -> int:
    return day.year * 10000 + day.month * 100 + day.day


def nasdaq_url(session: int) -> str:
    return f"ftp://ftp.nasdaqtrader.com/SymbolDirectory/shorthalts/shorthalts{session}.txt"


def nyse_url(session: int) -> str:
    year, month = divmod(session, 10000)
    month //= 100
    return (
        "https://ftp.nyse.com/NYSEGroupSSRCircuitBreakers/"
        f"NYSEGroupSSRCircuitBreakers_{year}/"
        f"NYSEGroupSSRCircuitBreakers_{year}{month:02d}/"
        f"NYSEGroupSSRCircuitBreakers{session}.xls"
    )


def ensure_schema(con: sqlite3.Connection, schema_path: Path = SCHEMA_V7) -> None:
    """Apply v7.sql when the store is at user_version 6. Refuse anything else."""
    version = con.execute("PRAGMA user_version").fetchone()[0]
    if version not in (6, 7):
        raise SchemaError(
            f"schema user_version is {version}; this loader writes a version-6 or version-7 store"
        )
    if version == 6:
        sql = schema_path.read_text(encoding="utf-8")
        con.executescript(sql)
        con.execute("PRAGMA user_version = 7")
        con.commit()
    present = {
        row[0]
        for row in con.execute(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name IN "
            "('short_sale_restriction', 'short_sale_restriction_file')"
        )
    }
    if present != {"short_sale_restriction", "short_sale_restriction_file"}:
        raise SchemaError("schema user_version 7 is missing the short-sale restriction tables")


def fetch_bytes(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read()
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            raise MissingFile(url) from exc
        raise FetchError(f"{url} HTTP {exc.code}") from exc
    except urllib.error.URLError as exc:
        if "550" in str(exc):
            raise MissingFile(url) from exc
        raise FetchError(f"{url} {exc.reason}") from exc
    head = data.lstrip()[:200]
    if head.startswith(b"<") or b"Incapsula" in head:
        raise FetchError(f"{url} returned a web page, not a data file")
    return data


def _busy(message: str) -> bool:
    return "HTTP 429" in message or "HTTP 503" in message


def fetch_retry(url: str) -> bytes | None:
    delay = 1.0
    last = "unknown error"
    attempt = 0
    busy_attempt = 0
    while attempt < ATTEMPTS:
        try:
            return fetch_bytes(url)
        except MissingFile:
            return None
        except FetchError as exc:
            last = str(exc)
            if _busy(last) and busy_attempt < len(BUSY_DELAYS):
                time.sleep(BUSY_DELAYS[busy_attempt])
                busy_attempt += 1
                continue
            attempt += 1
            if attempt >= ATTEMPTS:
                break
            time.sleep(delay)
            delay *= 2
    raise FetchError(last)


def store_file(con: sqlite3.Connection, parsed: ParsedFile, fetched_at: int) -> None:
    try:
        con.execute("BEGIN")
        con.execute(
            "DELETE FROM short_sale_restriction WHERE source = ? AND file_session = ?",
            (parsed.source, parsed.file_session),
        )
        con.executemany(
            "INSERT INTO short_sale_restriction ("
            "source, file_session, symbol, security_name, listing_market, "
            "trigger_session, trigger_ts, deactivate_session, ssr_state"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            [
                (
                    row.source,
                    row.file_session,
                    row.symbol,
                    row.security_name,
                    row.listing_market,
                    row.trigger_session,
                    row.trigger_ts,
                    row.deactivate_session,
                    row.ssr_state,
                )
                for row in parsed.rows
            ],
        )
        con.execute(
            "INSERT INTO short_sale_restriction_file ("
            "source, file_session, file_name, row_count, file_created_at, fetched_at"
            ") VALUES (?, ?, ?, ?, ?, ?) "
            "ON CONFLICT (source, file_session) DO UPDATE SET "
            "file_name = excluded.file_name, "
            "row_count = excluded.row_count, "
            "file_created_at = excluded.file_created_at, "
            "fetched_at = excluded.fetched_at",
            (
                parsed.source,
                parsed.file_session,
                parsed.file_name,
                len(parsed.rows),
                parsed.file_created_at,
                fetched_at,
            ),
        )
        con.commit()
    except Exception:
        con.rollback()
        raise


def _parse(source: str, session: int, payload: bytes) -> ParsedFile:
    if source == "nasdaq":
        name = f"shorthalts{session}.txt"
        return parse_nasdaq(payload.decode("utf-8"), session, name)
    name = f"NYSEGroupSSRCircuitBreakers{session}.xls"
    return parse_nyse_xls(payload, session, name)


def _url(source: str, session: int) -> str:
    if source == "nasdaq":
        return nasdaq_url(session)
    return nyse_url(session)


def load_source(
    con: sqlite3.Connection,
    source: str,
    start: date,
    end: date,
    refresh: bool,
) -> tuple[int, int, int, list[str]]:
    have = {
        row[0]
        for row in con.execute(
            "SELECT file_session FROM short_sale_restriction_file WHERE source = ?",
            (source,),
        )
    }
    pending = [session_of(day) for day in weekdays(start, end) if refresh or session_of(day) not in have]
    skipped = len(weekdays(start, end)) - len(pending)
    stored = 0
    missing = 0
    failures: list[str] = []
    workers = NYSE_WORKERS if source == "nyse_group" else WORKERS
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for offset in range(0, len(pending), 32):
            batch = pending[offset : offset + 32]
            futures = [pool.submit(fetch_retry, _url(source, session)) for session in batch]
            for session, future in zip(batch, futures):
                label = f"{source} {session}"
                try:
                    payload = future.result()
                except FetchError as exc:
                    failures.append(f"{label} {exc}")
                    print(f"fail {label}: {exc}", file=sys.stderr)
                    continue
                if payload is None:
                    missing += 1
                    continue
                try:
                    parsed = _parse(source, session, payload)
                    store_file(con, parsed, int(time.time()))
                except (ParseError, sqlite3.Error, UnicodeError) as exc:
                    failures.append(f"{label} {exc}")
                    print(f"fail {label}: {exc}", file=sys.stderr)
                    continue
                stored += 1
                print(f"{label}  {len(parsed.rows)} rows", file=sys.stderr)
    print(
        f"{source}: stored {stored}, already stored {skipped}, no file {missing}, failed {len(failures)}",
        file=sys.stderr,
    )
    return stored, missing, skipped, failures


def default_db() -> Path:
    return DEFAULT_DB


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Load short-sale restriction files into the store.")
    parser.add_argument("--db", type=Path, default=None, help="store path (default: data/market-data.sqlite)")
    parser.add_argument("--from", dest="start", help="first weekday, inclusive (YYYY-MM-DD)")
    parser.add_argument("--to", dest="end", help="last weekday, inclusive (default: today)")
    parser.add_argument("--source", choices=("nasdaq", "nyse_group"), help="one publisher (default: both)")
    parser.add_argument("--refresh", action="store_true", help="replace files that are already stored")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    db_path = args.db if args.db is not None else default_db()
    if not db_path.is_file():
        print(f"ssr: store not found at {db_path}", file=sys.stderr)
        return 2
    end = parse_cli_date(args.end) if args.end else date.today()
    explicit_start = parse_cli_date(args.start) if args.start else None
    sources = [args.source] if args.source else ["nasdaq", "nyse_group"]
    con = sqlite3.connect(db_path)
    try:
        con.execute("PRAGMA busy_timeout = 30000")
        con.execute("PRAGMA foreign_keys = ON")
        ensure_schema(con)
        failures: list[str] = []
        for source in sources:
            floor = NASDAQ_FLOOR if source == "nasdaq" else NYSE_FLOOR
            start = explicit_start if explicit_start is not None else floor
            if start > end:
                print(f"{source}: nothing to do ({start} is after {end})", file=sys.stderr)
                continue
            _, _, _, source_failures = load_source(con, source, start, end, args.refresh)
            failures.extend(source_failures)
    except SchemaError as exc:
        print(f"ssr: {exc}", file=sys.stderr)
        return 2
    finally:
        con.close()
    if failures:
        print(f"ssr: {len(failures)} file(s) failed", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
