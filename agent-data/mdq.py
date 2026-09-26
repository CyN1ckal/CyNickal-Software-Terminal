# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""mdq: read-only access to the terminal's market-data store.

Use this instead of hand-written SQL against data/market-data.sqlite. It
applies the conventions the C++ terminal uses, so numbers match the charts:

- Symbols resolve through instrument_listing (open listing first, then the
  most recently closed one). instrument.id is the key every fact row uses.
- Timestamps are Unix seconds UTC. A bar's ts is its OPEN time. A daily bar's
  ts is 09:30 America/New_York of its session.
- 1m bars are regular trading hours only (09:30-16:00 ET, last bar 15:59).
- Bars are split-adjusted backward by default (Adjust.h). Dividends are not.
- 5m/15m/30m/1h/... bars are built from 1m bars, bucketed from the 09:30 open,
  never spanning sessions (CChartTransform.h). Only 1m and 1d are stored.
- Ledger round trips and positions use FIFO lot matching (trading/Ledger.h).

Library use (Python 3.10+, stdlib only; pandas optional):

    import sys; sys.path.insert(0, "agent-data")
    from mdq import MarketData
    with MarketData() as md:
        bars = md.bars("QQQ", "5m", start="2026-09-01", end="2026-09-25")

CLI use: `python agent-data/mdq.py --help`.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import os
import sqlite3
import sys
from dataclasses import asdict, dataclass, fields
from datetime import date, datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Iterable, Iterator, Sequence

__all__ = [
    "Bar",
    "MarketData",
    "default_db_path",
    "is_nyse_holiday",
    "nyse_sessions",
    "rth_minutes",
    "session_date_of",
    "to_frame",
]

TIMEFRAME_1M = 60
TIMEFRAME_1D = 86400
RTH_OPEN_S = 9 * 3600 + 30 * 60
RTH_DURATION_S = 23400
CONTRACT_MULTIPLIER = 100.0
LOT_EPSILON = 1e-9
CLI_ROW_CAP = 2000

# NYSE cash sessions that closed at 13:00 ET. Published calendars; the C++
# calendar has no early closes, so coverage marks these 1m sessions 'partial'.
EARLY_CLOSES = frozenset(
    date.fromisoformat(d)
    for d in (
        "2019-07-03", "2019-11-29", "2019-12-24",
        "2020-11-27", "2020-12-24",
        "2021-11-26",
        "2022-11-25",
        "2023-07-03", "2023-11-24",
        "2024-07-03", "2024-11-29", "2024-12-24",
        "2025-07-03", "2025-11-28", "2025-12-24",
        "2026-11-27", "2026-12-24",
    )
)

# Unscheduled full-day closures (national days of mourning).
SPECIAL_CLOSURES = frozenset({date(2025, 1, 9)})


# --------------------------------------------------------------------------
# Paths and time
# --------------------------------------------------------------------------


def default_db_path() -> Path:
    """$TERMINAL_DB if set, else <repo>/data/market-data.sqlite."""
    env = os.environ.get("TERMINAL_DB")
    if env:
        return Path(env)
    return Path(__file__).resolve().parents[1] / "data" / "market-data.sqlite"


def _load_ny():
    try:
        from zoneinfo import ZoneInfo

        return ZoneInfo("America/New_York")
    except Exception:  # Windows without the tzdata package.
        return None


_NY = _load_ny()


def _us_dst_bounds_utc(year: int) -> tuple[int, int]:
    # Since 2007: second Sunday of March 02:00 EST to first Sunday of November 02:00 EDT.
    march = date(year, 3, 1)
    start = march + timedelta(days=(6 - march.weekday()) % 7 + 7)
    nov = date(year, 11, 1)
    end = nov + timedelta(days=(6 - nov.weekday()) % 7)
    epoch = date(1970, 1, 1)
    return (start - epoch).days * 86400 + 7 * 3600, (end - epoch).days * 86400 + 6 * 3600


_offset_cache: dict[int, int] = {}


def _ny_offset(ts: int) -> int:
    """UTC offset of America/New_York at ts, in seconds. DST changes on the hour."""
    hour = ts // 3600
    cached = _offset_cache.get(hour)
    if cached is not None:
        return cached
    if _NY is not None:
        off = datetime.fromtimestamp(ts, _NY).utcoffset()
        value = int(off.total_seconds()) if off is not None else -5 * 3600
    else:
        begin, end = _us_dst_bounds_utc(datetime.fromtimestamp(ts, timezone.utc).year)
        value = -4 * 3600 if begin <= ts < end else -5 * 3600
    _offset_cache[hour] = value
    return value


def ny_datetime(ts: int) -> datetime:
    """Aware datetime of ts in New York (fixed-offset tzinfo if tzdata is missing)."""
    if _NY is not None:
        return datetime.fromtimestamp(ts, _NY)
    return datetime.fromtimestamp(ts, timezone(timedelta(seconds=_ny_offset(ts))))


def session_date_of(ts: int) -> date:
    """New York calendar date of ts."""
    return date(1970, 1, 1) + timedelta(days=(ts + _ny_offset(ts)) // 86400)


def _ny_local_to_utc(day: date, seconds_after_midnight: int) -> int:
    # Only called for 00:00 and 09:30, which never fall in a 02:00 DST transition.
    naive = (day - date(1970, 1, 1)).days * 86400 + seconds_after_midnight
    utc = naive + 5 * 3600
    if _ny_offset(utc) == -4 * 3600:
        utc = naive + 4 * 3600
    return utc


def rth_window(day: date) -> tuple[int, int]:
    """[09:30, 16:00) New York of day, as UTC seconds. Early closes are not shortened."""
    start = _ny_local_to_utc(day, RTH_OPEN_S)
    return start, start + RTH_DURATION_S


def _ny_midnight_utc(day: date) -> int:
    return _ny_local_to_utc(day, 0)


def parse_date(value: Any) -> date:
    """date, datetime, 'YYYY-MM-DD', 'YYYYMMDD', or int YYYYMMDD."""
    if isinstance(value, datetime):
        return value.date()
    if isinstance(value, date):
        return value
    text = str(value).strip()
    if len(text) == 8 and text.isdigit():
        return date(int(text[:4]), int(text[4:6]), int(text[6:]))
    return date.fromisoformat(text)


def ymd_int(day: date) -> int:
    return day.year * 10000 + day.month * 100 + day.day


def ymd_from_int(value: int | None) -> date | None:
    if value is None:
        return None
    return date(value // 10000, value // 100 % 100, value % 100)


def _iso_utc(ts: int | None) -> str | None:
    if ts is None:
        return None
    return datetime.fromtimestamp(ts, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# --------------------------------------------------------------------------
# NYSE calendar
# --------------------------------------------------------------------------


def _observed(day: date) -> date:
    if day.weekday() == 5:
        return day - timedelta(days=1)
    if day.weekday() == 6:
        return day + timedelta(days=1)
    return day


def _nth_weekday(year: int, month: int, weekday: int, n: int) -> date:
    first = date(year, month, 1)
    return first + timedelta(days=(weekday - first.weekday()) % 7 + 7 * (n - 1))


def _last_weekday(year: int, month: int, weekday: int) -> date:
    last = (date(year + (month == 12), month % 12 + 1, 1)) - timedelta(days=1)
    return last - timedelta(days=(last.weekday() - weekday) % 7)


def _easter(year: int) -> date:
    a, b, c = year % 19, year // 100, year % 100
    d, e = b // 4, b % 4
    f = (b + 8) // 25
    g = (b - f + 1) // 3
    h = (19 * a + b - d - g + 15) % 30
    i, k = c // 4, c % 4
    ell = (32 + 2 * e + 2 * i - h - k) % 7
    m = (a + 11 * h + 22 * ell) // 451
    return date(year, (h + ell - 7 * m + 114) // 31, (h + ell - 7 * m + 114) % 31 + 1)


def is_nyse_holiday(day: date) -> bool:
    """Full-day NYSE closure on a weekday.

    Unlike libs/market-data NyseCalendar.cpp, a Saturday New Year's Day is NOT
    observed on the prior Friday (NYSE rule 7.2), so 2021-12-31 is a session.
    """
    y = day.year
    if day in SPECIAL_CLOSURES:
        return True
    new_year = date(y, 1, 1)
    if new_year.weekday() != 5 and day == _observed(new_year):
        return True
    return day in (
        _nth_weekday(y, 1, 0, 3),
        _nth_weekday(y, 2, 0, 3),
        _easter(y) - timedelta(days=2),
        _last_weekday(y, 5, 0),
        _observed(date(y, 7, 4)),
        _nth_weekday(y, 9, 0, 1),
        _nth_weekday(y, 11, 3, 4),
        _observed(date(y, 12, 25)),
    ) or (y >= 2022 and day == _observed(date(y, 6, 19)))


def nyse_sessions(start: Any, end: Any) -> list[date]:
    """Inclusive NYSE session dates."""
    day, last = parse_date(start), parse_date(end)
    out: list[date] = []
    while day <= last:
        if day.weekday() < 5 and not is_nyse_holiday(day):
            out.append(day)
        day += timedelta(days=1)
    return out


def rth_minutes(day: Any) -> int:
    """Expected 1m RTH bars for a session: 390, 211 on an early close (09:30-13:00 inclusive,
    as the vendor sends it), 0 on a holiday or weekend."""
    d = parse_date(day)
    if d.weekday() >= 5 or is_nyse_holiday(d):
        return 0
    return 211 if d in EARLY_CLOSES else 390


# --------------------------------------------------------------------------
# Timeframes and bars
# --------------------------------------------------------------------------


def parse_timeframe(value: Any) -> int:
    """'1m', '5m', '15m', '1h', '1d', or seconds. Returns seconds."""
    if isinstance(value, int):
        seconds = value
    else:
        text = str(value).strip().lower()
        units = {"m": 60, "min": 60, "h": 3600, "d": 86400}
        for suffix in ("min", "m", "h", "d"):
            if text.endswith(suffix) and text[: -len(suffix)].isdigit():
                seconds = int(text[: -len(suffix)]) * units[suffix]
                break
        else:
            if not text.isdigit():
                raise ValueError(f"unknown timeframe {value!r}; use 1m, 5m, 15m, 30m, 1h, 1d")
            seconds = int(text)
    if seconds == TIMEFRAME_1D:
        return seconds
    if seconds <= 0 or seconds % 60 or seconds > RTH_DURATION_S:
        raise ValueError(f"timeframe {value!r} must be whole minutes up to 390m, or 1d")
    return seconds


def format_timeframe(seconds: int) -> str:
    if seconds == TIMEFRAME_1D:
        return "1d"
    if seconds % 3600 == 0:
        return f"{seconds // 3600}h"
    return f"{seconds // 60}m"


@dataclass(frozen=True, slots=True)
class Bar:
    ts: int  # bar open, Unix seconds UTC
    open: float
    high: float
    low: float
    close: float
    volume: float

    @property
    def time(self) -> datetime:
        """Bar open in New York."""
        return ny_datetime(self.ts)

    @property
    def session(self) -> date:
        return session_date_of(self.ts)

    def row(self) -> dict[str, Any]:
        t = ny_datetime(self.ts)
        return {
            "ts": self.ts,
            "time_ny": t.strftime("%Y-%m-%d %H:%M"),
            "session": t.date().isoformat(),
            # Split adjustment leaves float noise (2512.2 / 20 = 125.60999...).
            "open": round(self.open, 6),
            "high": round(self.high, 6),
            "low": round(self.low, 6),
            "close": round(self.close, 6),
            "volume": round(self.volume, 3),
        }


def adjust_for_splits(bars: Sequence[Bar], splits: Sequence[tuple[int, float]]) -> list[Bar]:
    """Backward split adjustment (Adjust.h). splits is (ex_ts, ratio new/old).

    Each split with ex_ts > bar.ts divides OHLC by ratio and multiplies volume
    by it. The bar at ex_ts is the first post-split bar and is unchanged.
    """
    splits = sorted((ex, r) for ex, r in splits if r and r > 0)
    if not splits:
        return list(bars)
    out: list[Bar] = []
    for b in bars:
        factor = 1.0
        for ex_ts, ratio in splits:
            if ex_ts > b.ts:
                factor *= ratio
        if factor == 1.0:
            out.append(b)
        else:
            out.append(Bar(b.ts, b.open / factor, b.high / factor, b.low / factor,
                           b.close / factor, b.volume * factor))
    return out


def resample(bars_1m: Iterable[Bar], timeframe_s: int) -> list[Bar]:
    """Composite RTH 1m bars (CChartTransform.cpp). Buckets start at the 09:30
    open, never span sessions, and a partial bucket still emits. Pass 86400
    for one bar per session built from minutes."""
    out: list[Bar] = []
    cur: list[float] | None = None  # ts, o, h, l, c, v
    win_start = win_end = -1
    for b in bars_1m:
        if not (win_start <= b.ts < win_end):
            win_start, win_end = rth_window(session_date_of(b.ts))
            if not (win_start <= b.ts < win_end):
                continue
        if timeframe_s == TIMEFRAME_1D:
            aligned = win_start
        else:
            aligned = win_start + (b.ts - win_start) // timeframe_s * timeframe_s
        if cur is None or aligned != cur[0]:
            if cur is not None:
                out.append(Bar(int(cur[0]), cur[1], cur[2], cur[3], cur[4], cur[5]))
            cur = [aligned, b.open, b.high, b.low, b.close, b.volume]
        else:
            cur[2] = max(cur[2], b.high)
            cur[3] = min(cur[3], b.low)
            cur[4] = b.close
            cur[5] += b.volume
    if cur is not None:
        out.append(Bar(int(cur[0]), cur[1], cur[2], cur[3], cur[4], cur[5]))
    return out


def to_frame(rows: Sequence[Any]):
    """pandas DataFrame from mdq rows (Bars get a New York DatetimeIndex)."""
    import pandas as pd

    if rows and isinstance(rows[0], Bar):
        frame = pd.DataFrame(
            {f.name: [getattr(b, f.name) for b in rows] for f in fields(Bar)}
        )
        frame.index = pd.to_datetime(frame["ts"], unit="s", utc=True).dt.tz_convert(
            "America/New_York"
        )
        frame.index.name = "time"
        return frame
    return pd.DataFrame([r if isinstance(r, dict) else asdict(r) for r in rows])


# --------------------------------------------------------------------------
# Store access
# --------------------------------------------------------------------------


class MarketData:
    """Read-only connection to the market-data store.

    The connection is opened with mode=ro and PRAGMA query_only, so nothing
    here can modify the database, and it is safe to use while the terminal or
    ingest is running (the store is in WAL mode).
    """

    def __init__(self, db_path: str | os.PathLike[str] | None = None) -> None:
        path = Path(db_path) if db_path is not None else default_db_path()
        if not path.is_file():
            raise FileNotFoundError(
                f"market-data store not found at {path}. Run the terminal or `ingest` "
                "first, or set TERMINAL_DB."
            )
        self.path = path
        self.con = sqlite3.connect(f"{path.resolve().as_uri()}?mode=ro", uri=True)
        self.con.row_factory = sqlite3.Row
        self.con.execute("PRAGMA query_only = ON")
        version = self.con.execute("PRAGMA user_version").fetchone()[0]
        if version < 6:
            raise RuntimeError(f"schema user_version {version}; mdq expects 6 or newer")
        self.schema_version = version
        self._instrument_cache: dict[int, dict[str, Any]] = {}

    def close(self) -> None:
        self.con.close()

    def __enter__(self) -> "MarketData":
        return self

    def __exit__(self, *exc: object) -> None:
        self.close()

    # -- raw SQL ----------------------------------------------------------

    def sql(self, query: str, params: Sequence[Any] | dict[str, Any] = ()) -> list[dict[str, Any]]:
        """Run one read-only statement and return rows as dicts."""
        cur = self.con.execute(query, params)
        return [dict(r) for r in cur.fetchall()]

    def schema(self) -> str:
        rows = self.con.execute(
            "SELECT sql FROM sqlite_master WHERE sql IS NOT NULL AND name NOT LIKE 'sqlite_%' "
            "ORDER BY type = 'index', name"
        ).fetchall()
        return ";\n\n".join(r[0] for r in rows) + ";\n"

    # -- instruments --------------------------------------------------------

    def instruments(self) -> list[dict[str, Any]]:
        """Every instrument with its current symbol and stored bar ranges."""
        rows = self.sql(
            "SELECT id, symbol, figi, asset_class, name, listing_closed_at IS NOT NULL AS delisted "
            "FROM instrument_current ORDER BY symbol COLLATE NOCASE, id"
        )
        ranges: dict[tuple[int, int], dict[str, Any]] = {}
        for r in self.con.execute(
            "SELECT instrument_id, timeframe_s, count(*) AS n, min(session_date) AS first, "
            "max(session_date) AS last FROM coverage_day WHERE bar_count > 0 "
            "GROUP BY instrument_id, timeframe_s"
        ):
            ranges[(r["instrument_id"], r["timeframe_s"])] = dict(r)
        for row in rows:
            row["delisted"] = bool(row["delisted"])
            for tf, label in ((TIMEFRAME_1M, "1m"), (TIMEFRAME_1D, "1d")):
                span = ranges.get((row["id"], tf))
                row[f"{label}_sessions"] = span["n"] if span else 0
                row[f"{label}_first"] = ymd_from_int(span["first"]).isoformat() if span else None
                row[f"{label}_last"] = ymd_from_int(span["last"]).isoformat() if span else None
        return rows

    def resolve(self, ref: Any) -> dict[str, Any]:
        """Instrument for a symbol (case-insensitive), FIGI, or integer id.

        A symbol resolves to its open listing, else its most recently closed
        listing, like Store::resolveSymbol.
        """
        if isinstance(ref, dict) and "id" in ref:
            return ref
        cols = ("c.id, c.symbol, c.figi, c.asset_class, c.name, c.timezone, c.currency, "
                "c.listing_closed_at")
        row = None
        text = str(ref).strip()
        if isinstance(ref, int) or text.isdigit():
            row = self.con.execute(
                f"SELECT {cols} FROM instrument_current c WHERE c.id = ?", (int(text),)
            ).fetchone()
        if row is None and len(text) == 12 and text.upper().startswith("BBG"):
            row = self.con.execute(
                f"SELECT {cols} FROM instrument_current c WHERE c.figi = ?", (text.upper(),)
            ).fetchone()
        if row is None:
            row = self.con.execute(
                f"SELECT {cols} FROM instrument_listing l JOIN instrument_current c "
                "ON c.id = l.instrument_id WHERE l.symbol = ? "
                "ORDER BY l.closed_at IS NOT NULL, l.closed_at DESC, l.id DESC LIMIT 1",
                (text,),
            ).fetchone()
        if row is None:
            known = ", ".join(
                r[0] for r in self.con.execute(
                    "SELECT symbol FROM instrument_current ORDER BY symbol COLLATE NOCASE"
                )
            )
            raise LookupError(f"{ref!r} is not in the store. Known symbols: {known}")
        return dict(row)

    def listings(self, ref: Any) -> list[dict[str, Any]]:
        inst = self.resolve(ref)
        rows = self.sql(
            "SELECT symbol, opened_at, closed_at, close_reason FROM instrument_listing "
            "WHERE instrument_id = ? ORDER BY opened_at, id",
            (inst["id"],),
        )
        for r in rows:
            r["opened_at"] = _iso_utc(r["opened_at"])
            r["closed_at"] = _iso_utc(r["closed_at"])
        return rows

    # -- corporate actions ------------------------------------------------------

    def corporate_actions(self, ref: Any) -> list[dict[str, Any]]:
        inst = self.resolve(ref)
        rows = self.sql(
            "SELECT ex_ts, type, split_ratio, amount, currency, source FROM corporate_action "
            "WHERE instrument_id = ? ORDER BY ex_ts, id",
            (inst["id"],),
        )
        for r in rows:
            r["ex_date"] = session_date_of(r["ex_ts"]).isoformat()
        return rows

    def splits(self, ref: Any) -> list[tuple[int, float]]:
        inst = self.resolve(ref)
        return [
            (r[0], r[1])
            for r in self.con.execute(
                "SELECT ex_ts, split_ratio FROM corporate_action WHERE instrument_id = ? "
                "AND type = 'split' AND split_ratio > 0 ORDER BY ex_ts",
                (inst["id"],),
            )
        ]

    # -- bars -------------------------------------------------------------------

    def _query_bars(self, instrument_id: int, tf: int, lo: int | None, hi: int | None) -> list[Bar]:
        sql = "SELECT ts, open, high, low, close, volume FROM bar WHERE instrument_id = ? AND timeframe_s = ?"
        params: list[Any] = [instrument_id, tf]
        if lo is not None:
            sql += " AND ts >= ?"
            params.append(lo)
        if hi is not None:
            sql += " AND ts < ?"
            params.append(hi)
        sql += " ORDER BY ts"
        return [Bar(*r) for r in self.con.execute(sql, params)]

    def bars(
        self,
        ref: Any,
        timeframe: Any = "1d",
        start: Any = None,
        end: Any = None,
        *,
        last: int | None = None,
        adjust: bool = True,
    ) -> list[Bar]:
        """OHLCV bars, oldest first.

        timeframe: '1m', '1d' (stored), or any whole-minute composite such as
            '5m', '15m', '30m', '1h' (built from 1m, anchored at 09:30 ET).
        start, end: inclusive New York session dates (YYYY-MM-DD, YYYYMMDD, date).
        last: keep only the most recent N sessions (applied after start/end).
        adjust: backward split adjustment (default). False returns as-traded prices.
        """
        inst = self.resolve(ref)
        tf = parse_timeframe(timeframe)
        source_tf = TIMEFRAME_1D if tf == TIMEFRAME_1D else TIMEFRAME_1M
        lo = _ny_midnight_utc(parse_date(start)) if start is not None else None
        hi = _ny_midnight_utc(parse_date(end) + timedelta(days=1)) if end is not None else None

        if last is not None:
            if last <= 0:
                return []
            lo = self._last_sessions_floor(inst["id"], source_tf, last, lo, hi)

        rows = self._query_bars(inst["id"], source_tf, lo, hi)
        if last is not None and rows:
            keep = sorted({session_date_of(b.ts) for b in rows})[-last:]
            first = keep[0]
            rows = [b for b in rows if session_date_of(b.ts) >= first]
        if adjust:
            rows = adjust_for_splits(rows, self.splits(inst))
        if tf not in (TIMEFRAME_1M, TIMEFRAME_1D):
            rows = resample(rows, tf)
        return rows

    def _last_sessions_floor(self, instrument_id: int, tf: int, n: int,
                             lo: int | None, hi: int | None) -> int | None:
        """A ts lower bound that includes at least the last n sessions with bars."""
        sql = "SELECT max(ts) FROM bar WHERE instrument_id = ? AND timeframe_s = ?"
        params: list[Any] = [instrument_id, tf]
        if hi is not None:
            sql += " AND ts < ?"
            params.append(hi)
        newest = self.con.execute(sql, params).fetchone()[0]
        if newest is None:
            return lo
        newest_day = session_date_of(newest)
        span = n * 7 // 5 + 7
        while True:
            floor = _ny_midnight_utc(newest_day - timedelta(days=span))
            if lo is not None and floor <= lo:
                return lo
            count = self.con.execute(
                "SELECT count(DISTINCT session_date) FROM coverage_day WHERE instrument_id = ? "
                "AND timeframe_s = ? AND bar_count > 0 AND session_date BETWEEN ? AND ?",
                (instrument_id, tf, ymd_int(newest_day - timedelta(days=span)), ymd_int(newest_day)),
            ).fetchone()[0]
            if count >= n or span > 40000:
                return floor
            span *= 2

    def latest(self, ref: Any, timeframe: Any = "1d", *, adjust: bool = True) -> Bar | None:
        """Most recent bar."""
        rows = self.bars(ref, timeframe, last=1, adjust=adjust)
        return rows[-1] if rows else None

    # -- coverage ---------------------------------------------------------------

    def coverage(self, ref: Any, timeframe: Any = "1m", start: Any = None,
                 end: Any = None) -> list[dict[str, Any]]:
        """One coverage_day row per ingested session, with an 'expected' note."""
        inst = self.resolve(ref)
        tf = parse_timeframe(timeframe)
        if tf not in (TIMEFRAME_1M, TIMEFRAME_1D):
            raise ValueError("coverage is recorded for 1m and 1d only")
        sql = ("SELECT session_date, bar_count, expected_count, status, first_ts, last_ts, "
               "ingested_at FROM coverage_day WHERE instrument_id = ? AND timeframe_s = ?")
        params: list[Any] = [inst["id"], tf]
        if start is not None:
            sql += " AND session_date >= ?"
            params.append(ymd_int(parse_date(start)))
        if end is not None:
            sql += " AND session_date <= ?"
            params.append(ymd_int(parse_date(end)))
        out: list[dict[str, Any]] = []
        for r in self.sql(sql + " ORDER BY session_date", params):
            day = ymd_from_int(r.pop("session_date"))
            r = {"session": day.isoformat(), **r}
            r["note"] = _coverage_note(day, tf, r["status"], r["bar_count"])
            r["ingested_at"] = _iso_utc(r["ingested_at"])
            out.append(r)
        return out

    def coverage_summary(self, ref: Any, timeframe: Any = "1m") -> dict[str, Any]:
        """Counts by status, stored range, and the sessions that need attention."""
        rows = self.coverage(ref, timeframe)
        inst = self.resolve(ref)
        by_status: dict[str, int] = {}
        for r in rows:
            by_status[r["status"]] = by_status.get(r["status"], 0) + 1
        with_bars = [r for r in rows if r["bar_count"] > 0]
        problems = [
            {k: r[k] for k in ("session", "status", "bar_count", "expected_count", "note")}
            for r in rows
            if r["note"] and not r["note"].startswith("expected")
        ]
        recorded = {r["session"] for r in rows}
        unrecorded: list[str] = []
        if with_bars:
            for day in nyse_sessions(with_bars[0]["session"], with_bars[-1]["session"]):
                if day.isoformat() not in recorded:
                    unrecorded.append(day.isoformat())
        return {
            "symbol": inst["symbol"],
            "timeframe": format_timeframe(parse_timeframe(timeframe)),
            "sessions_recorded": len(rows),
            "sessions_with_bars": len(with_bars),
            "first_session": with_bars[0]["session"] if with_bars else None,
            "last_session": with_bars[-1]["session"] if with_bars else None,
            "by_status": by_status,
            "needs_attention": problems,
            "sessions_not_recorded": unrecorded,
        }

    # -- fundamentals -----------------------------------------------------------

    def statement(self, ref: Any, statement: str = "income", timeframe: str = "annually",
                  items: Sequence[str] | None = None) -> list[dict[str, Any]]:
        """Financial statement as one row per period (oldest first, TTM last),
        one column per vendor line item. A missing cell is absent, not zero."""
        inst = self.resolve(ref)
        sql = ("SELECT period_end, line_item, value_kind, value_int, value_real, value_text "
               "FROM statement_cell WHERE instrument_id = ? AND statement = ? AND timeframe = ?")
        params: list[Any] = [inst["id"], statement, timeframe]
        if items:
            sql += f" AND line_item IN ({','.join('?' * len(items))})"
            params.extend(items)
        sql += " ORDER BY period_end = 'TTM', period_end, line_item"
        periods: dict[str, dict[str, Any]] = {}
        for r in self.con.execute(sql, params):
            value = {"int": r["value_int"], "real": r["value_real"], "text": r["value_text"]}[r["value_kind"]]
            periods.setdefault(r["period_end"], {"period_end": r["period_end"]})[r["line_item"]] = value
        return list(periods.values())

    def statements_available(self, ref: Any | None = None) -> list[dict[str, Any]]:
        sql = ("SELECT c.symbol, s.statement, s.timeframe, s.fetched_at, "
               "(SELECT count(DISTINCT period_end) FROM statement_cell x WHERE x.instrument_id = s.instrument_id "
               "AND x.statement = s.statement AND x.timeframe = s.timeframe) AS periods "
               "FROM statement_snapshot s JOIN instrument_current c ON c.id = s.instrument_id")
        params: list[Any] = []
        if ref is not None:
            sql += " WHERE s.instrument_id = ?"
            params.append(self.resolve(ref)["id"])
        rows = self.sql(sql + " ORDER BY c.symbol, s.statement, s.timeframe", params)
        for r in rows:
            r["fetched_at"] = _iso_utc(r["fetched_at"])
        return rows

    # -- options ----------------------------------------------------------------

    def option_expirations(self, ref: Any) -> list[dict[str, Any]]:
        inst = self.resolve(ref)
        rows = self.sql(
            "SELECT e.expiration, e.expiration_type, e.average_iv, e.fetched_at, "
            "(SELECT count(*) FROM option_quote q WHERE q.instrument_id = e.instrument_id AND "
            "q.expiration = e.expiration AND q.expiration_type = e.expiration_type) AS quotes "
            "FROM option_expiry e WHERE e.instrument_id = ? ORDER BY e.expiration, e.expiration_type",
            (inst["id"],),
        )
        for r in rows:
            r["expiration"] = ymd_from_int(r["expiration"]).isoformat()
            r["fetched_at"] = _iso_utc(r["fetched_at"])
        return rows

    def option_chain(self, ref: Any, expiration: Any = None,
                     expiration_type: str | None = None) -> list[dict[str, Any]]:
        """Stored quotes (a snapshot, not history). Percent fields are fractions."""
        inst = self.resolve(ref)
        sql = "SELECT * FROM option_quote WHERE instrument_id = ?"
        params: list[Any] = [inst["id"]]
        if expiration is not None:
            sql += " AND expiration = ?"
            params.append(ymd_int(parse_date(expiration)))
        if expiration_type is not None:
            sql += " AND expiration_type = ?"
            params.append(expiration_type)
        rows = self.sql(sql + " ORDER BY expiration, expiration_type, strike, right, vendor_symbol", params)
        for r in rows:
            r.pop("instrument_id", None)
            r["expiration"] = ymd_from_int(r["expiration"]).isoformat()
            r["trade_date"] = ymd_from_int(r["trade_date"]).isoformat() if r["trade_date"] else None
            r["fetched_at"] = _iso_utc(r["fetched_at"])
        return rows

    def option_underlying(self, ref: Any) -> dict[str, Any] | None:
        inst = self.resolve(ref)
        rows = self.sql("SELECT * FROM option_underlying WHERE instrument_id = ?", (inst["id"],))
        if not rows:
            return None
        r = rows[0]
        r["next_earnings"] = ymd_from_int(r["next_earnings"]).isoformat() if r["next_earnings"] else None
        r["dividend_ex"] = ymd_from_int(r["dividend_ex"]).isoformat() if r["dividend_ex"] else None
        r["fetched_at"] = _iso_utc(r["fetched_at"])
        return r

    # -- portfolios -------------------------------------------------------------

    def portfolios(self) -> list[dict[str, Any]]:
        return self.sql(
            "SELECT p.id, p.name, (SELECT count(*) FROM portfolio_holding h WHERE h.portfolio_id = p.id) "
            "AS holdings FROM portfolio p ORDER BY p.name COLLATE NOCASE, p.id"
        )

    def holdings(self, portfolio: Any) -> list[dict[str, Any]]:
        pid = self._portfolio_id(portfolio)
        rows = self.sql(
            "SELECT h.asset_kind, c.symbol, h.quantity, h.expiration, h.expiration_type, h.strike, "
            "h.right, h.vendor_symbol FROM portfolio_holding h LEFT JOIN instrument_current c "
            "ON c.id = h.instrument_id WHERE h.portfolio_id = ? ORDER BY h.asset_kind, c.symbol, "
            "h.expiration, h.strike, h.right",
            (pid,),
        )
        for r in rows:
            r["expiration"] = ymd_from_int(r["expiration"]).isoformat() if r["expiration"] else None
        return rows

    def _portfolio_id(self, portfolio: Any) -> int:
        text = str(portfolio).strip()
        if text.isdigit():
            row = self.con.execute("SELECT id FROM portfolio WHERE id = ?", (int(text),)).fetchone()
        else:
            row = self.con.execute("SELECT id FROM portfolio WHERE name = ?", (text,)).fetchone()
        if row is None:
            raise LookupError(f"no portfolio {portfolio!r}")
        return row[0]

    # -- ledgers and backtests ----------------------------------------------------

    def ledgers(self) -> list[dict[str, Any]]:
        rows = self.sql(
            "SELECT l.id, l.name, l.kind, l.created_at, "
            "(SELECT count(*) FROM trade_fill f WHERE f.ledger_id = l.id) AS fills, "
            "(SELECT min(ts) FROM trade_fill f WHERE f.ledger_id = l.id) AS first_fill, "
            "(SELECT max(ts) FROM trade_fill f WHERE f.ledger_id = l.id) AS last_fill, "
            "b.strategy_id, b.params_json "
            "FROM ledger l LEFT JOIN backtest_run b ON b.ledger_id = l.id "
            "ORDER BY l.kind = 'backtest', l.name COLLATE NOCASE, l.id"
        )
        for r in rows:
            r["created_at"] = _iso_utc(r["created_at"])
            r["first_fill"] = _iso_utc(r["first_fill"])
            r["last_fill"] = _iso_utc(r["last_fill"])
        return rows

    def _ledger_id(self, ledger: Any) -> int:
        text = str(ledger).strip()
        if text.isdigit():
            row = self.con.execute("SELECT id FROM ledger WHERE id = ?", (int(text),)).fetchone()
        else:
            row = self.con.execute(
                "SELECT id FROM ledger WHERE name = ? COLLATE NOCASE ORDER BY kind = 'backtest', id DESC LIMIT 1",
                (text,),
            ).fetchone()
        if row is None:
            raise LookupError(f"no ledger {ledger!r}; list them with `mdq.py ledgers`")
        return row[0]

    def fills(self, ledger: Any) -> list[dict[str, Any]]:
        """Fills in (ts, id) order. quantity is signed: + buy, - sell."""
        lid = self._ledger_id(ledger)
        rows = self.sql(
            "SELECT f.id, f.ts, c.symbol, f.instrument_id, f.asset_kind, f.expiration, "
            "f.expiration_type, f.strike, f.right, f.quantity, f.price, f.fees, f.note, f.external_id "
            "FROM trade_fill f JOIN instrument_current c ON c.id = f.instrument_id "
            "WHERE f.ledger_id = ? ORDER BY f.ts, f.id",
            (lid,),
        )
        for r in rows:
            r["time_ny"] = ny_datetime(r["ts"]).strftime("%Y-%m-%d %H:%M:%S")
        return rows

    def cash_flows(self, ledger: Any) -> list[dict[str, Any]]:
        lid = self._ledger_id(ledger)
        return self.sql(
            "SELECT id, ts, amount, note, external_id FROM ledger_cash_flow WHERE ledger_id = ? "
            "ORDER BY ts, id",
            (lid,),
        )

    def backtest_run(self, ledger: Any) -> dict[str, Any] | None:
        lid = self._ledger_id(ledger)
        rows = self.sql(
            "SELECT b.*, c.symbol FROM backtest_run b JOIN instrument_current c ON c.id = b.instrument_id "
            "WHERE b.ledger_id = ?",
            (lid,),
        )
        if not rows:
            return None
        r = rows[0]
        r["params"] = json.loads(r.pop("params_json"))
        r["config"] = json.loads(r.pop("config_json"))
        r["timeframe"] = format_timeframe(r["timeframe_s"])
        r["begin"] = _iso_utc(r["ts_begin"])
        r["end"] = _iso_utc(r["ts_end"])
        return r

    def ledger_book(self, ledger: Any) -> dict[str, Any]:
        """FIFO round trips, open positions, and cash, as the terminal derives them."""
        fills = self.fills(ledger)
        flows = self.cash_flows(ledger)
        splits: list[tuple[int, int, float]] = []
        for inst_id in {f["instrument_id"] for f in fills}:
            splits += [(ex, inst_id, ratio) for ex, ratio in self.splits(inst_id)]
        book = match_lots(fills, splits)
        deposits = sum(f["amount"] for f in flows)
        book["cash_flows"] = deposits
        book["cash"] = deposits + book["trade_cash"]
        return book


def _terminal_calendar_misses(day: date) -> bool:
    """NyseCalendar.cpp observes a Saturday New Year's Day on Friday Dec 31.
    NYSE does not, so the terminal treats those real sessions as holidays."""
    return day.month == 12 and day.day == 31 and day.weekday() == 4


def _coverage_note(day: date, tf: int, status: str, bar_count: int) -> str:
    """Empty when nothing is wrong, 'expected: ...' for known benign cases, else the problem."""
    session = day.weekday() < 5 and not is_nyse_holiday(day)
    if _terminal_calendar_misses(day):
        if bar_count == 0:
            return "never fetched: the terminal's calendar treats this session as a holiday"
        return "expected: real session the terminal's calendar calls a holiday; the bar is good"
    if status == "complete":
        if bar_count == 0 and session:
            return "recorded complete with 0 bars on a session day"
        return ""
    if tf == TIMEFRAME_1M and day in EARLY_CLOSES and bar_count == 211:
        return "expected: 13:00 early close"
    if day in SPECIAL_CLOSURES and bar_count == 0:
        return "expected: market closed"
    if status == "error":
        return "ingest HTTP error; re-run ingest for this session"
    if status == "partial":
        return "fewer minutes than expected: untraded minutes (thin names) or a gap"
    return "no bars: re-run ingest for this session"


# --------------------------------------------------------------------------
# FIFO lot matching (port of apps/terminal/src/trading/Ledger.cpp)
# --------------------------------------------------------------------------


def _position_key(fill: dict[str, Any]) -> tuple:
    if fill["asset_kind"] == "option":
        return (fill["instrument_id"], "option", fill["expiration"], fill["expiration_type"],
                fill["strike"], fill["right"])
    return (fill["instrument_id"], fill["asset_kind"], None, None, None, None)


def match_lots(fills: Sequence[dict[str, Any]],
               splits: Sequence[tuple[int, int, float]] = ()) -> dict[str, Any]:
    """First-in, first-out matching over fills sorted by ts (ties keep order).

    splits are (ex_ts, instrument_id, ratio); a split rescales open share lots
    on its ex_ts, before a fill at the same ts. Option lots are not split.
    Returns round_trips, positions, realized_pnl, fees_paid, trade_cash.
    """
    pending = sorted((s for s in splits if s[2] > 0), key=lambda s: s[0])
    next_split = 0
    lots: list[dict[str, Any]] = []
    trips: list[dict[str, Any]] = []
    realized = fees_paid = cash = 0.0

    def advance(ts: int) -> None:
        nonlocal next_split
        while next_split < len(pending) and pending[next_split][0] <= ts:
            _ex, inst_id, ratio = pending[next_split]
            for lot in lots:
                if lot["key"][1] != "option" and lot["key"][0] == inst_id:
                    lot["quantity"] *= ratio
                    lot["price"] /= ratio
            next_split += 1

    for fill in sorted(fills, key=lambda f: f["ts"]):
        key = _position_key(fill)
        advance(fill["ts"])
        mult = CONTRACT_MULTIPLIER if fill["asset_kind"] == "option" else 1.0
        size = abs(fill["quantity"])
        remaining = fill["quantity"]
        i = 0
        while i < len(lots) and remaining != 0.0:
            lot = lots[i]
            if lot["key"] != key or (lot["quantity"] > 0) == (remaining > 0):
                i += 1
                continue
            lot_size = abs(lot["quantity"])
            matched = min(abs(remaining), lot_size)
            closed = matched if lot["quantity"] > 0 else -matched
            open_fees = lot["fees"] * (matched / lot_size)
            close_fees = fill["fees"] * (matched / size)
            gross = closed * (fill["price"] - lot["price"]) * mult
            trip = {
                "symbol": fill.get("symbol") or lot["symbol"],
                "asset_kind": fill["asset_kind"],
                "side": "long" if closed > 0 else "short",
                "quantity": closed,
                "open_fill_id": lot["fill_id"],
                "close_fill_id": fill["id"],
                "opened_at": lot["opened_at"],
                "closed_at": fill["ts"],
                "entry_price": lot["price"],
                "exit_price": fill["price"],
                "multiplier": mult,
                "gross_pnl": gross,
                "fees": open_fees + close_fees,
                "net_pnl": gross - open_fees - close_fees,
            }
            if key[1] == "option":
                trip.update(expiration=key[2], expiration_type=key[3], strike=key[4], right=key[5])
            trips.append(trip)
            realized += trip["net_pnl"]
            lot["quantity"] -= closed
            lot["fees"] -= open_fees
            remaining += closed
            if abs(remaining) <= LOT_EPSILON:
                remaining = 0.0
            if abs(lot["quantity"]) <= LOT_EPSILON:
                lots.pop(i)
            else:
                i += 1
        if remaining != 0.0:
            lots.append({
                "key": key,
                "symbol": fill.get("symbol"),
                "fill_id": fill["id"],
                "opened_at": fill["ts"],
                "quantity": remaining,
                "price": fill["price"],
                "fees": fill["fees"] * (abs(remaining) / size),
            })
        fees_paid += fill["fees"]
        cash += -fill["quantity"] * fill["price"] * mult - fill["fees"]

    grouped: dict[tuple, dict[str, Any]] = {}
    for lot in lots:
        mult = CONTRACT_MULTIPLIER if lot["key"][1] == "option" else 1.0
        p = grouped.setdefault(lot["key"], {
            "symbol": lot["symbol"], "asset_kind": lot["key"][1], "expiration": lot["key"][2],
            "expiration_type": lot["key"][3], "strike": lot["key"][4], "right": lot["key"][5],
            "quantity": 0.0, "cost_basis": 0.0, "open_fees": 0.0, "lots": 0, "multiplier": mult,
        })
        p["quantity"] += lot["quantity"]
        p["cost_basis"] += lot["quantity"] * lot["price"] * mult
        p["open_fees"] += lot["fees"]
        p["lots"] += 1
    positions = []
    for key in sorted(grouped, key=lambda k: tuple("" if v is None else v for v in k)):
        p = grouped[key]
        p["average_price"] = p["cost_basis"] / (p["quantity"] * p["multiplier"])
        positions.append(p)
    return {
        "round_trips": trips,
        "positions": positions,
        "realized_pnl": realized,
        "fees_paid": fees_paid,
        "trade_cash": cash,
    }


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------


def _emit(rows: Any, fmt: str, out: str | None, limit: int | None) -> None:
    if isinstance(rows, dict):
        rows_out: Any = rows
    else:
        rows_list = [r.row() if isinstance(r, Bar) else r for r in rows]
        total = len(rows_list)
        if limit is not None:
            rows_list = rows_list[-limit:] if limit > 0 else rows_list
        elif out is None and total > CLI_ROW_CAP:
            sys.stderr.write(
                f"mdq: {total} rows; printing the last {CLI_ROW_CAP}. Use --out FILE for all of "
                "them, --limit N to choose, or the Python API for analysis.\n"
            )
            rows_list = rows_list[-CLI_ROW_CAP:]
        rows_out = rows_list

    stream: io.TextIOBase
    if out:
        Path(out).parent.mkdir(parents=True, exist_ok=True)
        stream = open(out, "w", newline="", encoding="utf-8")
    else:
        stream = sys.stdout
    try:
        if fmt == "json" or isinstance(rows_out, dict):
            json.dump(rows_out, stream, indent=2 if isinstance(rows_out, dict) else None, default=str)
            stream.write("\n")
        elif fmt == "csv":
            if rows_out:
                writer = csv.DictWriter(stream, fieldnames=_columns(rows_out), lineterminator="\n")
                writer.writeheader()
                writer.writerows(rows_out)
        else:
            _write_table(stream, rows_out)
    finally:
        if out:
            stream.close()
            sys.stderr.write(f"mdq: wrote {out}\n")


def _columns(rows: Sequence[dict[str, Any]]) -> list[str]:
    cols: dict[str, None] = {}
    for r in rows:
        cols.update(dict.fromkeys(r))
    return list(cols)


def _cell(v: Any) -> str:
    if v is None:
        return ""
    if isinstance(v, float):
        return f"{v:.6g}" if abs(v) < 1e6 else f"{v:,.0f}"
    return str(v)


def _write_table(stream: Any, rows: Sequence[dict[str, Any]]) -> None:
    if not rows:
        stream.write("(no rows)\n")
        return
    cols = _columns(rows)
    cells = [[_cell(r.get(c)) for c in cols] for r in rows]
    widths = [max(len(c), *(len(row[i]) for row in cells)) for i, c in enumerate(cols)]
    stream.write("  ".join(c.ljust(w) for c, w in zip(cols, widths)).rstrip() + "\n")
    for row in cells:
        stream.write("  ".join(v.ljust(w) for v, w in zip(row, widths)).rstrip() + "\n")


def _build_parser() -> argparse.ArgumentParser:
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--db", help="store path (default: $TERMINAL_DB or <repo>/data/market-data.sqlite)")
    common.add_argument("--format", "-f", choices=("table", "csv", "json"), default="table")
    common.add_argument("--out", "-o", help="write to this file instead of stdout")
    common.add_argument("--limit", "-n", type=int, help="keep only the last N rows (0 = all)")

    p = argparse.ArgumentParser(
        prog="mdq.py",
        description="Read-only queries against the terminal's market-data store. "
                    "See agent-data/README.md. Options go after the command: "
                    "mdq.py bars QQQ --tf 5m --last 3 -f csv",
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    def cmd(name: str, help: str) -> argparse.ArgumentParser:
        return sub.add_parser(name, help=help, parents=[common])

    cmd("instruments", help="symbols, FIGIs, and stored 1m/1d ranges")
    s = cmd("resolve", help="instrument row for a symbol, FIGI, or id")
    s.add_argument("ref")

    s = cmd("bars", help="OHLCV bars (split-adjusted, RTH)")
    s.add_argument("ref")
    s.add_argument("--tf", "-t", default="1d", help="1m, 5m, 15m, 30m, 1h, 1d (default 1d)")
    s.add_argument("--from", dest="start", help="first session date, inclusive")
    s.add_argument("--to", dest="end", help="last session date, inclusive")
    s.add_argument("--last", type=int, help="most recent N sessions")
    s.add_argument("--raw", action="store_true", help="as-traded prices (no split adjustment)")

    s = cmd("coverage", help="coverage summary and sessions that need attention")
    s.add_argument("ref")
    s.add_argument("--tf", "-t", default="1m")
    s.add_argument("--days", action="store_true", help="one row per session instead of a summary")
    s.add_argument("--from", dest="start")
    s.add_argument("--to", dest="end")

    s = cmd("actions", help="splits and dividends")
    s.add_argument("ref")

    s = cmd("statement", help="financial statement, one row per period")
    s.add_argument("ref", nargs="?")
    s.add_argument("--kind", "-k", default="income", choices=("income", "balance", "cashflow"))
    s.add_argument("--period", "-p", default="annually", choices=("annually", "quarterly", "trailing"))
    s.add_argument("--items", help="comma-separated vendor line items (e.g. revenue,netinccmn)")
    s.add_argument("--list", action="store_true", help="which statements are stored")

    s = cmd("options", help="stored option chain snapshot")
    s.add_argument("ref")
    s.add_argument("--exp", help="expiration date")
    s.add_argument("--type", choices=("weekly", "monthly"))
    s.add_argument("--expirations", action="store_true", help="list expirations only")
    s.add_argument("--underlying", action="store_true", help="IV rank, HV, earnings for the underlying")

    cmd("portfolios", help="hypothetical portfolios")
    s = cmd("holdings", help="holdings of one portfolio")
    s.add_argument("portfolio", help="id or name")

    cmd("ledgers", help="trade ledgers and backtest runs")
    s = cmd("fills", help="fills of one ledger")
    s.add_argument("ledger", help="id or name")
    s = cmd("trips", help="FIFO round trips of one ledger")
    s.add_argument("ledger")
    s = cmd("book", help="positions, realized P&L, fees, and cash of one ledger")
    s.add_argument("ledger")
    s = cmd("backtest", help="strategy, params, and config of a backtest ledger")
    s.add_argument("ledger")

    s = cmd("sessions", help="NYSE sessions with expected 1m bar counts")
    s.add_argument("start")
    s.add_argument("end")

    s = cmd("sql", help="one read-only SQL statement (last resort)")
    s.add_argument("query")
    s.add_argument("params", nargs="*")
    cmd("schema", help="print the live DDL")
    return p


def main(argv: Sequence[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    if args.cmd == "sessions":
        rows: Any = [
            {"session": d.isoformat(), "weekday": d.strftime("%a"), "rth_1m_bars": rth_minutes(d),
             "early_close": d in EARLY_CLOSES}
            for d in nyse_sessions(args.start, args.end)
        ]
        _emit(rows, args.format, args.out, args.limit)
        return 0

    try:
        md = MarketData(args.db)
    except (FileNotFoundError, RuntimeError) as exc:
        sys.stderr.write(f"mdq: {exc}\n")
        return 2
    try:
        with md:
            if args.cmd == "schema":
                sys.stdout.write(md.schema())
                return 0
            rows = _dispatch(md, args)
    except (LookupError, ValueError, sqlite3.Error) as exc:
        sys.stderr.write(f"mdq: {exc}\n")
        return 1
    _emit(rows, args.format, args.out, args.limit)
    return 0


def _dispatch(md: MarketData, args: argparse.Namespace) -> Any:
    cmd = args.cmd
    if cmd == "instruments":
        return md.instruments()
    if cmd == "resolve":
        return md.resolve(args.ref)
    if cmd == "bars":
        return md.bars(args.ref, args.tf, args.start, args.end, last=args.last, adjust=not args.raw)
    if cmd == "coverage":
        if args.days:
            return md.coverage(args.ref, args.tf, args.start, args.end)
        return md.coverage_summary(args.ref, args.tf)
    if cmd == "actions":
        return md.corporate_actions(args.ref)
    if cmd == "statement":
        if args.list or args.ref is None:
            return md.statements_available(args.ref)
        items = [i.strip() for i in args.items.split(",")] if args.items else None
        return md.statement(args.ref, args.kind, args.period, items)
    if cmd == "options":
        if args.underlying:
            return md.option_underlying(args.ref) or {}
        if args.expirations:
            return md.option_expirations(args.ref)
        return md.option_chain(args.ref, args.exp, args.type)
    if cmd == "portfolios":
        return md.portfolios()
    if cmd == "holdings":
        return md.holdings(args.portfolio)
    if cmd == "ledgers":
        return md.ledgers()
    if cmd == "fills":
        return md.fills(args.ledger)
    if cmd == "trips":
        trips = md.ledger_book(args.ledger)["round_trips"]
        for t in trips:
            t["opened_ny"] = ny_datetime(t["opened_at"]).strftime("%Y-%m-%d %H:%M")
            t["closed_ny"] = ny_datetime(t["closed_at"]).strftime("%Y-%m-%d %H:%M")
        return trips
    if cmd == "book":
        book = md.ledger_book(args.ledger)
        book["round_trip_count"] = len(book.pop("round_trips"))
        return book
    if cmd == "backtest":
        return md.backtest_run(args.ledger) or {}
    if cmd == "sql":
        return md.sql(args.query, args.params)
    raise ValueError(f"unknown command {cmd}")


if __name__ == "__main__":
    sys.exit(main())
