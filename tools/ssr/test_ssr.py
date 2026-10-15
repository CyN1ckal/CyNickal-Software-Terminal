# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Parser and schema tests for the short-sale restriction loader.

Run: python tools/ssr/test_ssr.py
"""

from __future__ import annotations

import sqlite3
import sys
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

import load  # noqa: E402
import parse  # noqa: E402

FIXTURES = HERE / "fixtures"
NASDAQ = (FIXTURES / "nasdaq_sample.txt").read_text(encoding="utf-8")
NYSE_XLS = FIXTURES / "NYSEGroupSSRCircuitBreakers20251201.xls"


def utc(year: int, month: int, day: int, hour: int, minute: int, second: int = 0) -> int:
    return int(datetime(year, month, day, hour, minute, second, tzinfo=timezone.utc).timestamp())


class ParseNasdaqTest(unittest.TestCase):
    def test_sample_keeps_both_triggers_and_drops_the_repeated_line(self) -> None:
        parsed = parse.parse_nasdaq(NASDAQ, 20261013, "shorthalts20261013.txt")
        self.assertEqual(parsed.file_created_at, utc(2026, 10, 13, 20, 30, 13))
        self.assertEqual(len(parsed.rows), 3)
        symbols = [row.symbol for row in parsed.rows]
        self.assertEqual(symbols, ["POM", "VIVK", "POM"])
        vivk = parsed.rows[1]
        self.assertEqual(vivk.security_name, "Vivakor, Inc. Cm St")
        self.assertEqual(vivk.listing_market, "R")
        self.assertEqual(vivk.trigger_session, 20261013)
        self.assertEqual(vivk.trigger_ts, utc(2026, 10, 13, 13, 30))
        self.assertIsNone(vivk.deactivate_session)
        afternoon = parsed.rows[2]
        self.assertEqual(afternoon.trigger_ts, utc(2026, 10, 13, 19, 59, 58))
        carry = parsed.rows[0]
        self.assertEqual(carry.trigger_session, 20261012)
        self.assertEqual(carry.trigger_ts, utc(2026, 10, 12, 13, 30))

    def test_no_data_available_is_an_empty_session(self) -> None:
        parsed = parse.parse_nasdaq("\r\nNo data available", 20110225, "shorthalts20110225.txt")
        self.assertEqual(parsed.rows, ())
        self.assertIsNone(parsed.file_created_at)
        self.assertEqual(parsed.file_session, 20110225)

    def test_html_and_a_mismatched_trailer_fail(self) -> None:
        with self.assertRaises(parse.ParseError):
            parse.parse_nasdaq("<html>Incapsula</html>", 20261013, "shorthalts20261013.txt")
        with self.assertRaises(parse.ParseError):
            parse.parse_nasdaq(NASDAQ, 20261014, "shorthalts20261014.txt")

    def test_same_key_with_different_fields_fails(self) -> None:
        text = (
            "Symbol,Security Name,Market Category,Trigger Time\n"
            "AAA,One,Q,10/13/2026 9:30:00 AM\n"
            "AAA,Two,Q,10/13/2026 9:30:00 AM\n"
            "20261013163013\n"
        )
        with self.assertRaises(parse.ParseError):
            parse.parse_nasdaq(text, 20261013, "shorthalts20261013.txt")


class ParseNyseTest(unittest.TestCase):
    def test_status_header_and_padded_market(self) -> None:
        grid = [
            [
                "Trigger Date",
                "Issue Symbol",
                "Issue Name",
                "Primary Exchange",
                "SSR Status",
                "Trigger Time",
                "Anticipated Deactivation Date",
            ],
            ["01/04/2016", "AAM WS", "Example warrants", "MKT ", "A", "09:30:46", "01/06/2016"],
        ]
        parsed = parse.parse_nyse_grid(grid, 20160104, "NYSEGroupSSRCircuitBreakers20160104.xls")
        self.assertEqual(len(parsed.rows), 1)
        row = parsed.rows[0]
        self.assertEqual(row.symbol, "AAM WS")
        self.assertEqual(row.listing_market, "MKT")
        self.assertEqual(row.ssr_state, "A")
        self.assertEqual(row.trigger_session, 20160104)
        self.assertEqual(row.deactivate_session, 20160106)
        # 09:30:46 America/New_York in January is 14:30:46 UTC.
        self.assertEqual(row.trigger_ts, utc(2016, 1, 4, 14, 30, 46))
        self.assertIsNone(parsed.file_created_at)

    def test_header_only_is_an_empty_file(self) -> None:
        grid = [[
            "Trigger Date",
            "Issue Symbol",
            "Issue Name",
            "Primary Exchange",
            "SSR State",
            "Trigger Time",
            "Anticipated Deactivation Date",
        ]]
        parsed = parse.parse_nyse_grid(grid, 20251201, "empty.xls")
        self.assertEqual(parsed.rows, ())

    def test_workbook_fixture(self) -> None:
        parsed = parse.parse_nyse_xls(NYSE_XLS.read_bytes(), 20251201, NYSE_XLS.name)
        self.assertEqual(len(parsed.rows), 92)
        aht = next(row for row in parsed.rows if row.symbol == "AHT")
        self.assertEqual(aht.security_name, "Ashford Hospitality Trust, Inc.")
        self.assertEqual(aht.listing_market, "NYSE")
        self.assertEqual(aht.ssr_state, "A")
        self.assertEqual(aht.trigger_session, 20251201)
        self.assertEqual(aht.deactivate_session, 20251203)
        self.assertEqual(aht.trigger_ts, utc(2025, 12, 1, 17, 26, 40))
        self.assertTrue(any(" " in row.symbol for row in parsed.rows))


class SchemaTest(unittest.TestCase):
    def test_version_6_gains_the_tables_and_keeps_the_check(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "store.sqlite"
            con = sqlite3.connect(path)
            schema = ROOT / "libs" / "market-data" / "schema"
            for name in ("v4", "v5", "v6"):
                con.executescript((schema / f"{name}.sql").read_text(encoding="utf-8"))
            con.execute("PRAGMA user_version = 6")
            con.commit()
            load.ensure_schema(con)
            self.assertEqual(con.execute("PRAGMA user_version").fetchone()[0], 7)
            load.ensure_schema(con)
            self.assertEqual(con.execute("PRAGMA user_version").fetchone()[0], 7)
            con.execute(
                "INSERT INTO short_sale_restriction ("
                "source, file_session, symbol, security_name, listing_market, "
                "trigger_session, trigger_ts, deactivate_session, ssr_state"
                ") VALUES ('nasdaq', 20261013, 'POM', 'POMDOCTOR', 'Q', 20261012, 1, NULL, NULL)"
            )
            with self.assertRaises(sqlite3.IntegrityError):
                con.execute(
                    "INSERT INTO short_sale_restriction ("
                    "source, file_session, symbol, security_name, listing_market, "
                    "trigger_session, trigger_ts, deactivate_session, ssr_state"
                    ") VALUES ('nasdaq', 20261013, 'POM', 'POMDOCTOR', 'Q', 20261012, 2, 20261015, NULL)"
                )
            con.close()

    def test_version_5_is_refused(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "store.sqlite"
            con = sqlite3.connect(path)
            con.execute("PRAGMA user_version = 5")
            with self.assertRaises(load.SchemaError):
                load.ensure_schema(con)
            self.assertEqual(con.execute("PRAGMA user_version").fetchone()[0], 5)
            con.close()


class UrlTest(unittest.TestCase):
    def test_nyse_path_uses_the_year_and_month(self) -> None:
        self.assertEqual(
            load.nyse_url(20160104),
            "https://ftp.nyse.com/NYSEGroupSSRCircuitBreakers/"
            "NYSEGroupSSRCircuitBreakers_2016/"
            "NYSEGroupSSRCircuitBreakers_201601/"
            "NYSEGroupSSRCircuitBreakers20160104.xls",
        )


if __name__ == "__main__":
    unittest.main()
