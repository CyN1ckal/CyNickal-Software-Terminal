# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Tests for mdq.py against a throwaway store built from libs/market-data/schema.

Run: python agent-data/test_mdq.py
"""

from __future__ import annotations

import contextlib
import io
import json
import sqlite3
import sys
import tempfile
import unittest
from datetime import date, datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import mdq  # noqa: E402

SCHEMA = HERE.parent / "libs" / "market-data" / "schema"

# 2022-06-03 and 2022-06-06 are EDT (UTC-4): 09:30 ET = 13:30 UTC.
FRI_OPEN = 1654263000  # 2022-06-03 09:30 ET
MON_OPEN = 1654522200  # 2022-06-06 09:30 ET, split ex-date


def _utc(year: int, month: int, day: int, hour: int, minute: int, second: int = 0) -> int:
    return int(datetime(year, month, day, hour, minute, second, tzinfo=timezone.utc).timestamp())


# 2026-10-13 and 2026-10-14 are EDT (UTC-4).
OLD_TRIGGER = _utc(2026, 10, 13, 13, 30)       # 09:30 ET
POM_TRIGGER = _utc(2026, 10, 14, 13, 30)       # 09:30 ET
AHT_TRIGGER = _utc(2026, 10, 13, 16, 26, 40)   # 12:26:40 ET
DONE_TRIGGER = _utc(2026, 10, 13, 14, 0)       # 10:00 ET


def build_store(path: Path) -> None:
    con = sqlite3.connect(path)
    for version in ("v4", "v5", "v6", "v7"):
        con.executescript((SCHEMA / f"{version}.sql").read_text(encoding="utf-8"))
    con.execute("PRAGMA user_version = 7")
    con.executemany(
        "INSERT INTO instrument (id, figi, asset_class, name, created_at) VALUES (?, ?, ?, ?, 0)",
        [(1, "BBG000BVPV84", "equity", "AMAZON"), (2, "BBG000BDTBL9", "etf", "SPDR")],
    )
    con.executemany(
        "INSERT INTO instrument_listing (instrument_id, symbol, opened_at, closed_at, close_reason) "
        "VALUES (?, ?, ?, ?, ?)",
        [(1, "AMZN", 0, None, None), (2, "OLDSPY", 0, 5, "renamed"), (2, "SPY", 5, None, None)],
    )
    con.execute(
        "INSERT INTO corporate_action (instrument_id, ex_ts, type, split_ratio) VALUES (1, ?, 'split', 20)",
        (MON_OPEN,),
    )
    bars = []
    for day_open in (FRI_OPEN, MON_OPEN):
        price = 2000.0 if day_open == FRI_OPEN else 100.0
        bars.append((1, 86400, day_open, price, price + 10, price - 10, price + 5, 1000))
        for minute in range(390):
            p = price + minute * 0.01
            bars.append((1, 60, day_open + minute * 60, p, p + 1, p - 1, p + 0.5, 10))
    con.executemany("INSERT INTO bar VALUES (?, ?, ?, ?, ?, ?, ?, ?)", bars)
    con.executemany(
        "INSERT INTO coverage_day (instrument_id, timeframe_s, session_date, bar_count, "
        "expected_count, status, ingested_at) VALUES (?, ?, ?, ?, ?, ?, 0)",
        [
            (1, 60, 20220603, 390, 390, "complete"),
            (1, 60, 20220606, 390, 390, "complete"),
            (1, 86400, 20220603, 1, 1, "complete"),
            (1, 86400, 20220606, 1, 1, "complete"),
            (2, 60, 20211231, 0, 0, "complete"),
            (2, 60, 20241224, 211, 390, "partial"),
            (2, 60, 20250109, 0, 390, "missing"),
            (2, 60, 20250110, 300, 390, "partial"),
        ],
    )
    con.execute("INSERT INTO ledger (id, name, kind, created_at, updated_at) VALUES (1, 'Book', 'manual', 0, 0)")
    con.execute("INSERT INTO ledger_cash_flow (ledger_id, ts, amount) VALUES (1, 0, 10000)")
    con.executemany(
        "INSERT INTO trade_fill (id, ledger_id, instrument_id, asset_kind, expiration, expiration_type, "
        "strike, right, ts, quantity, price, fees) VALUES (?, 1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        [
            (1, 1, "equity", None, None, None, None, FRI_OPEN, 10, 2000.0, 2.0),
            (2, 1, "equity", None, None, None, None, MON_OPEN + 60, -250, 110.0, 5.0),
            (3, 2, "option", 20220617, "monthly", 400.0, "call", FRI_OPEN, 2, 3.0, 1.0),
            (4, 2, "option", 20220617, "monthly", 400.0, "call", MON_OPEN, -1, 4.0, 1.0),
        ],
    )
    con.executemany(
        "INSERT INTO short_sale_restriction_file "
        "(source, file_session, file_name, row_count, file_created_at, fetched_at) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        [
            ("nasdaq", 20261013, "shorthalts20261013.txt", 1, OLD_TRIGGER, 0),
            ("nasdaq", 20261014, "shorthalts20261014.txt", 1, POM_TRIGGER, 0),
            ("nyse_group", 20261013, "NYSEGroupSSRCircuitBreakers20261013.xls", 2, None, 0),
        ],
    )
    con.executemany(
        "INSERT INTO short_sale_restriction ("
        "source, file_session, symbol, security_name, listing_market, "
        "trigger_session, trigger_ts, deactivate_session, ssr_state"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
        [
            ("nasdaq", 20261013, "OLD", "Old Name", "Q", 20261013, OLD_TRIGGER, None, None),
            ("nasdaq", 20261014, "POM", "Pom Name", "Q", 20261014, POM_TRIGGER, None, None),
            ("nyse_group", 20261013, "AHT", "Ashford", "NYSE", 20261013, AHT_TRIGGER, 20261015, "A"),
            ("nyse_group", 20261013, "DONE", "Done Name", "NYSE", 20261013, DONE_TRIGGER, 20261014, "A"),
        ],
    )
    con.commit()
    con.close()


class StoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = tempfile.TemporaryDirectory()
        cls.path = Path(cls.tmp.name) / "store.sqlite"
        build_store(cls.path)
        cls.md = mdq.MarketData(cls.path)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.md.close()
        cls.tmp.cleanup()

    def test_resolve(self) -> None:
        self.assertEqual(self.md.resolve("amzn")["id"], 1)
        self.assertEqual(self.md.resolve("BBG000BDTBL9")["symbol"], "SPY")
        self.assertEqual(self.md.resolve(2)["symbol"], "SPY")
        self.assertEqual(self.md.resolve("OLDSPY")["id"], 2, "closed listing still resolves")
        with self.assertRaisesRegex(LookupError, "Known symbols: AMZN, SPY"):
            self.md.resolve("NOPE")

    def test_read_only(self) -> None:
        with self.assertRaises(sqlite3.OperationalError):
            self.md.sql("DELETE FROM ledger")

    def test_daily_bars_split_adjusted(self) -> None:
        adj = self.md.bars("AMZN", "1d")
        self.assertEqual([b.session for b in adj], [date(2022, 6, 3), date(2022, 6, 6)])
        self.assertAlmostEqual(adj[0].close, 2005.0 / 20)
        self.assertAlmostEqual(adj[0].volume, 1000 * 20)
        self.assertEqual(adj[1].close, 105.0, "ex-date bar is already post-split")
        raw = self.md.bars("AMZN", "1d", adjust=False)
        self.assertEqual(raw[0].close, 2005.0)

    def test_range_and_last(self) -> None:
        self.assertEqual(len(self.md.bars("AMZN", "1m", start="2022-06-06")), 390)
        self.assertEqual(len(self.md.bars("AMZN", "1m", end="20220603")), 390)
        last = self.md.bars("AMZN", "1m", last=1)
        self.assertEqual({b.session for b in last}, {date(2022, 6, 6)})
        self.assertEqual(self.md.latest("AMZN").ts, MON_OPEN)

    def test_resample_anchored_at_open(self) -> None:
        five = self.md.bars("AMZN", "5m", start="2022-06-06")
        self.assertEqual(len(five), 78)
        self.assertEqual(five[0].ts, MON_OPEN)
        self.assertEqual(five[0].open, 100.0)
        self.assertAlmostEqual(five[0].close, 100.04 + 0.5)
        self.assertEqual(five[0].volume, 50)
        hours = self.md.bars("AMZN", "1h", start="2022-06-06")
        self.assertEqual(len(hours), 7, "09:30..15:30, last hour is 30 minutes")
        self.assertEqual(hours[-1].ts - MON_OPEN, 6 * 3600)
        self.assertEqual(hours[-1].volume, 300)
        both = self.md.bars("AMZN", "1h")
        self.assertEqual(len(both), 14, "buckets never span sessions")
        daily = mdq.resample(self.md.bars("AMZN", "1m", adjust=False), 86400)
        self.assertEqual([b.ts for b in daily], [FRI_OPEN, MON_OPEN])

    def test_coverage_notes(self) -> None:
        notes = {r["session"]: r["note"] for r in self.md.coverage("SPY")}
        self.assertTrue(notes["2021-12-31"].startswith("never fetched"))
        self.assertEqual(notes["2024-12-24"], "expected: 13:00 early close")
        self.assertEqual(notes["2025-01-09"], "expected: market closed")
        self.assertTrue(notes["2025-01-10"].startswith("fewer minutes"))
        summary = self.md.coverage_summary("SPY")
        flagged = [r["session"] for r in summary["needs_attention"]]
        self.assertEqual(flagged, ["2021-12-31", "2025-01-10"])

    def test_ledger_book(self) -> None:
        book = self.md.ledger_book("book")
        shares = [t for t in book["round_trips"] if t["asset_kind"] == "equity"]
        # 10 shares at 2000 become 200 at 100 on the split; selling 250 closes 200 and opens 50 short.
        self.assertEqual(len(shares), 1)
        self.assertAlmostEqual(shares[0]["quantity"], 200)
        self.assertAlmostEqual(shares[0]["entry_price"], 100.0)
        self.assertAlmostEqual(shares[0]["gross_pnl"], 200 * 10.0)
        self.assertAlmostEqual(shares[0]["fees"], 2.0 + 5.0 * 200 / 250)
        option = [t for t in book["round_trips"] if t["asset_kind"] == "option"][0]
        self.assertAlmostEqual(option["gross_pnl"], 1 * (4.0 - 3.0) * 100)
        self.assertAlmostEqual(option["fees"], 0.5 + 1.0)
        positions = {p["asset_kind"]: p for p in book["positions"]}
        self.assertAlmostEqual(positions["equity"]["quantity"], -50)
        self.assertAlmostEqual(positions["option"]["quantity"], 1)
        self.assertAlmostEqual(positions["option"]["cost_basis"], 300.0)
        self.assertAlmostEqual(book["fees_paid"], 9.0)
        expected_cash = -10 * 2000 - 2 + 250 * 110 - 5 - 2 * 300 - 1 + 400 - 1
        self.assertAlmostEqual(book["trade_cash"], expected_cash)
        self.assertAlmostEqual(book["cash"], 10000 + expected_cash)

    def test_cli(self) -> None:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = mdq.main(["bars", "AMZN", "--tf", "1d", "--db", str(self.path), "-f", "json"])
        self.assertEqual(code, 0)
        rows = json.loads(out.getvalue())
        self.assertEqual(rows[0]["session"], "2022-06-03")
        self.assertEqual(rows[0]["close"], 100.25)
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = mdq.main(["bars", "NOPE", "--db", str(self.path)])
        self.assertEqual(code, 1)
        self.assertIn("not in the store", err.getvalue())

    def test_ssr_session_rule(self) -> None:
        report = self.md.short_sale_session("2026-10-14")
        self.assertEqual(report["session"], "2026-10-14")
        self.assertEqual(report["files"], {"nasdaq": True, "nyse_group": False})
        self.assertEqual({row["symbol"] for row in report["rows"]}, {"POM", "AHT"})
        pom = next(row for row in report["rows"] if row["symbol"] == "POM")
        self.assertEqual(pom["source"], "nasdaq")
        self.assertEqual(pom["trigger_time"], "2026-10-14 09:30:00")
        self.assertIsNone(pom["deactivate_session"])
        aht = next(row for row in report["rows"] if row["symbol"] == "AHT")
        self.assertEqual(aht["trigger_time"], "2026-10-13 12:26:40")
        self.assertEqual(aht["deactivate_session"], "2026-10-15")
        on_trigger_day = {row["symbol"] for row in self.md.short_sale_restrictions(session="20261013")}
        self.assertEqual(on_trigger_day, {"OLD", "AHT", "DONE"})
        self.assertEqual(self.md.short_sale_restrictions("AHT", "2026-10-15"), [])
        self.assertEqual(self.md.short_sale_restrictions(" pom ")[0]["symbol"], "POM")
        files = self.md.short_sale_files(source="nasdaq")
        self.assertEqual([row["file_session"] for row in files], ["2026-10-13", "2026-10-14"])

    def test_ssr_cli_notes_a_missing_nyse_file(self) -> None:
        out = io.StringIO()
        err = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = mdq.main(["ssr", "--session", "20261014", "--db", str(self.path), "-f", "json"])
        self.assertEqual(code, 0)
        payload = json.loads(out.getvalue())
        self.assertFalse(payload["files"]["nyse_group"])
        self.assertEqual({row["symbol"] for row in payload["rows"]}, {"AHT", "POM"})
        self.assertIn("no nyse_group file stored for 2026-10-14", err.getvalue())
        self.assertNotIn("no nasdaq file", err.getvalue())

    def test_ssr_needs_schema_v7(self) -> None:
        path = Path(self.tmp.name) / "v6.sqlite"
        con = sqlite3.connect(path)
        for version in ("v4", "v5", "v6"):
            con.executescript((SCHEMA / f"{version}.sql").read_text(encoding="utf-8"))
        con.execute("PRAGMA user_version = 6")
        con.commit()
        con.close()
        older = mdq.MarketData(path)
        try:
            self.assertEqual(older.schema_version, 6)
            self.assertEqual(older.instruments(), [])
            with self.assertRaisesRegex(RuntimeError, "version 7"):
                older.short_sale_restrictions(session="2026-10-14")
        finally:
            older.close()


class CalendarTests(unittest.TestCase):
    def test_holidays(self) -> None:
        self.assertFalse(mdq.is_nyse_holiday(date(2021, 12, 31)), "Saturday New Year is not observed")
        self.assertTrue(mdq.is_nyse_holiday(date(2022, 12, 26)))
        self.assertTrue(mdq.is_nyse_holiday(date(2024, 3, 29)), "Good Friday")
        self.assertFalse(mdq.is_nyse_holiday(date(2021, 6, 18)), "Juneteenth starts in 2022")
        self.assertTrue(mdq.is_nyse_holiday(date(2022, 6, 20)))
        self.assertTrue(mdq.is_nyse_holiday(date(2025, 1, 9)))
        self.assertEqual(len(mdq.nyse_sessions("2024-01-01", "2024-12-31")), 252)

    def test_rth_minutes(self) -> None:
        self.assertEqual(mdq.rth_minutes("2025-11-28"), 211)
        self.assertEqual(mdq.rth_minutes("2025-11-27"), 0)
        self.assertEqual(mdq.rth_minutes("2025-12-01"), 390)

    def test_dst(self) -> None:
        self.assertEqual(mdq.rth_window(date(2026, 1, 5))[0] % 86400, 14 * 3600 + 30 * 60)
        self.assertEqual(mdq.rth_window(date(2026, 7, 6))[0] % 86400, 13 * 3600 + 30 * 60)
        # 2026-11-01 01:30 EST is 06:30 UTC; 23:59 EST Oct 31 is 03:59 UTC Nov 1.
        self.assertEqual(mdq.session_date_of(1793514600), date(2026, 11, 1))
        self.assertEqual(mdq.session_date_of(1793505540), date(2026, 10, 31))

    def test_dst_fallback_matches_zoneinfo(self) -> None:
        saved = mdq._NY
        try:
            with_tz = [mdq._ny_offset(ts) for ts in range(1640995200, 1798761600, 3 * 3600 + 7)]
            mdq._NY = None
            mdq._offset_cache.clear()
            without = [mdq._ny_offset(ts) for ts in range(1640995200, 1798761600, 3 * 3600 + 7)]
        finally:
            mdq._NY = saved
            mdq._offset_cache.clear()
        self.assertEqual(with_tz, without)

    def test_timeframes(self) -> None:
        self.assertEqual(mdq.parse_timeframe("15m"), 900)
        self.assertEqual(mdq.parse_timeframe("1h"), 3600)
        self.assertEqual(mdq.parse_timeframe("1d"), 86400)
        with self.assertRaises(ValueError):
            mdq.parse_timeframe("1w")


if __name__ == "__main__":
    unittest.main()
