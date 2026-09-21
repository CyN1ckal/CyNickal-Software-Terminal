-- Copyright 2026 CyNickal Software LLC
-- SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

-- market-data schema v1
-- Frozen. Do not ALTER in place; add schema/v2.sql and bump kSchemaUserVersion.
-- Applied only when PRAGMA user_version = 0.
-- Idempotent: CREATE IF NOT EXISTS so a crashed migrate can retry
-- after a rollback; user_version is set only after success.

CREATE TABLE IF NOT EXISTS instrument (
    id            INTEGER PRIMARY KEY,
    symbol        TEXT    NOT NULL COLLATE NOCASE,
    exchange      TEXT,
    asset_class   TEXT    NOT NULL DEFAULT 'equity'
                    CHECK (asset_class IN ('equity','etf','index','future','crypto','other')),
    currency      TEXT    NOT NULL DEFAULT 'USD',
    timezone      TEXT    NOT NULL DEFAULT 'America/New_York',
    name          TEXT,
    listed_at     INTEGER,
    delisted_at   INTEGER,
    created_at    INTEGER NOT NULL,
    CHECK (listed_at IS NULL OR listed_at >= 0),
    CHECK (delisted_at IS NULL OR delisted_at >= 0),
    CHECK (created_at >= 0),
    CHECK (delisted_at IS NULL OR listed_at IS NULL OR delisted_at >= listed_at)
) STRICT;

CREATE UNIQUE INDEX IF NOT EXISTS instrument_symbol_exchange
    ON instrument (symbol COLLATE NOCASE, ifnull(exchange, ''));

CREATE TABLE IF NOT EXISTS bar (
    instrument_id INTEGER NOT NULL
                    REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    timeframe_s   INTEGER NOT NULL CHECK (timeframe_s > 0),
    ts            INTEGER NOT NULL CHECK (ts >= 0),
    open          REAL    NOT NULL,
    high          REAL    NOT NULL,
    low           REAL    NOT NULL,
    close         REAL    NOT NULL,
    volume        REAL    NOT NULL CHECK (volume >= 0),
    CHECK (high >= low AND high >= open AND high >= close
       AND low  <= open AND low  <= close),
    PRIMARY KEY (instrument_id, timeframe_s, ts)
) WITHOUT ROWID, STRICT;

CREATE TABLE IF NOT EXISTS corporate_action (
    id            INTEGER PRIMARY KEY,
    instrument_id INTEGER NOT NULL
                    REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    ex_ts         INTEGER NOT NULL CHECK (ex_ts >= 0),
    type          TEXT    NOT NULL
                    CHECK (type IN ('split','dividend','spinoff','other')),
    split_ratio   REAL,
    amount        REAL,
    currency      TEXT,
    source        TEXT    NOT NULL DEFAULT 'mboum',
    CHECK (split_ratio IS NULL OR split_ratio > 0),
    CHECK (type != 'split'    OR split_ratio IS NOT NULL),
    CHECK (type != 'dividend' OR amount IS NOT NULL)
) STRICT;

CREATE UNIQUE INDEX IF NOT EXISTS corporate_action_identity
    ON corporate_action (
        instrument_id,
        ex_ts,
        type,
        ifnull(split_ratio, 0),
        ifnull(amount, 0)
    );

CREATE INDEX IF NOT EXISTS corporate_action_instrument_ex
    ON corporate_action (instrument_id, ex_ts);

CREATE TABLE IF NOT EXISTS coverage_day (
    instrument_id  INTEGER NOT NULL
                     REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    timeframe_s    INTEGER NOT NULL CHECK (timeframe_s > 0),
    session_date   INTEGER NOT NULL
                     CHECK (session_date >= 19000101 AND session_date <= 21001231),
    first_ts       INTEGER,
    last_ts        INTEGER,
    bar_count      INTEGER NOT NULL CHECK (bar_count >= 0),
    expected_count INTEGER CHECK (expected_count IS NULL OR expected_count >= 0),
    status         TEXT    NOT NULL
                     CHECK (status IN ('complete','partial','missing','error')),
    source         TEXT    NOT NULL DEFAULT 'mboum',
    ingested_at    INTEGER NOT NULL CHECK (ingested_at >= 0),
    CHECK (first_ts IS NULL OR last_ts IS NULL OR last_ts >= first_ts),
    CHECK (first_ts IS NULL OR first_ts >= 0),
    CHECK (last_ts  IS NULL OR last_ts  >= 0),
    PRIMARY KEY (instrument_id, timeframe_s, session_date)
) WITHOUT ROWID, STRICT;

CREATE INDEX IF NOT EXISTS coverage_day_status
    ON coverage_day (instrument_id, timeframe_s, status, session_date);
