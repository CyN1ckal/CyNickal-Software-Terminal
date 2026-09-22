-- Copyright 2026 CyNickal Software LLC
-- SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

-- market-data schema v2: MBoum modules financial statements.
-- Applied when PRAGMA user_version < 2, after v1 tables exist.
-- Idempotent: CREATE IF NOT EXISTS so a crashed migrate can retry
-- after a rollback; user_version is set only after success.
--
-- One HTTP response is one grid: a statement (income, balance, cashflow)
-- at one timeframe (annually, quarterly, trailing) for one instrument.
-- Those three timeframes are separate series. An annual flow on a year-end
-- date is the full year; the quarterly value on that same date is the quarter.
-- Trailing flows are twelve-month figures ending on that quarter date.
--
-- The response is columnar. Each body field becomes rows in statement_cell,
-- keyed by the vendor period (YYYY-MM-DD, or the literal TTM). A missing key
-- is not stored and must not be read back as zero. fiscalYear and
-- fiscalQuarter are ordinary cells: on quarterly and trailing responses the
-- year labels are not aligned to the period, so they are not columns on the
-- period. Line-item names are the vendor spellings (netinccmn, defferedTaxAssets).
--
-- JSON integers stay INTEGER. JSON floats stay REAL. JSON strings (fiscal
-- year, fiscal quarter) stay TEXT. Exactly one of the three value columns
-- is set. Dollars and share counts are absolute, not thousands. Ratios such
-- as fcfMargin are fractions. There is no currency column; these payloads
-- do not carry one.
--
-- statement_snapshot is one row per fetched grid. Replacing a grid deletes
-- that grid's cells first, so a line the vendor drops does not linger.
-- TTM is an annual balance-sheet and cash-flow column. It is not a date.
-- Income statements have no TTM column; their trailing view is timeframe
-- 'trailing'.
--
-- Module map: income-statement-v2 -> income, balance-sheet-v2 -> balance,
-- cashflow-statement-v2 -> cashflow. Timeframe tokens match the query.

CREATE TABLE IF NOT EXISTS statement_snapshot (
    instrument_id INTEGER NOT NULL
                    REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    statement     TEXT    NOT NULL
                    CHECK (statement IN ('income', 'balance', 'cashflow')),
    timeframe     TEXT    NOT NULL
                    CHECK (timeframe IN ('annually', 'quarterly', 'trailing')),
    source        TEXT    NOT NULL DEFAULT 'mboum'
                    CHECK (length(source) > 0 AND source = trim(source)),
    fetched_at    INTEGER NOT NULL CHECK (fetched_at >= 0),
    PRIMARY KEY (instrument_id, statement, timeframe)
) WITHOUT ROWID, STRICT;

CREATE TABLE IF NOT EXISTS statement_cell (
    instrument_id INTEGER NOT NULL,
    statement     TEXT    NOT NULL
                    CHECK (statement IN ('income', 'balance', 'cashflow')),
    timeframe     TEXT    NOT NULL
                    CHECK (timeframe IN ('annually', 'quarterly', 'trailing')),
    line_item     TEXT    NOT NULL
                    CHECK (length(line_item) > 0 AND line_item = trim(line_item)),
    period_end    TEXT    NOT NULL
                    CHECK (
                        period_end = 'TTM'
                        OR (
                            length(period_end) = 10
                            AND period_end GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'
                            AND CAST(substr(period_end, 6, 2) AS INTEGER) BETWEEN 1 AND 12
                            AND CAST(substr(period_end, 9, 2) AS INTEGER) BETWEEN 1 AND 31
                        )
                    ),
    value_kind    TEXT    NOT NULL
                    CHECK (value_kind IN ('int', 'real', 'text')),
    value_int     INTEGER,
    value_real    REAL,
    value_text    TEXT,
    PRIMARY KEY (instrument_id, statement, timeframe, line_item, period_end),
    FOREIGN KEY (instrument_id, statement, timeframe)
        REFERENCES statement_snapshot (instrument_id, statement, timeframe)
        ON DELETE CASCADE ON UPDATE RESTRICT,
    CHECK (
        (value_kind = 'int'
            AND value_int IS NOT NULL
            AND value_real IS NULL
            AND value_text IS NULL)
        OR (value_kind = 'real'
            AND value_real IS NOT NULL
            AND value_int IS NULL
            AND value_text IS NULL)
        OR (value_kind = 'text'
            AND value_text IS NOT NULL
            AND length(value_text) > 0
            AND value_int IS NULL
            AND value_real IS NULL)
    )
) WITHOUT ROWID, STRICT;

-- Period slice: every line item at one period end.
-- The primary key already covers one line item across periods.
CREATE INDEX IF NOT EXISTS statement_cell_period
    ON statement_cell (instrument_id, statement, timeframe, period_end, line_item);
