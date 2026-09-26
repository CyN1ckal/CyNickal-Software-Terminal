-- Copyright 2026 CyNickal Software LLC
-- SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

-- libs/market-data/schema/v6.sql
-- market-data schema v6: trade ledgers and backtest runs.
-- Applied when user_version is 0 (after v5.sql), 4 (after v5.sql), or 5.
-- Idempotent: CREATE IF NOT EXISTS so a crashed migrate can retry
-- after a rollback; user_version is set only after success.
--
-- Does not ALTER any earlier table. A ledger is not a portfolio: holdings
-- are hand-edited quantity, a ledger is fills from which positions derive.
-- Round trips, positions, equity, and statistics are derived on read.
-- Quantity is signed. Positive buys, negative sells. Equity and ETF
-- quantity is shares. Option quantity is contracts. Fees are never negative.
-- Cash is USD. Starting capital is a cash flow, not a column.
-- A backtest ledger is written once with its backtest_run row.
-- ledger.id is AUTOINCREMENT because chartbooks keep ledger ids: a deleted
-- ledger's id must never name a later ledger.

CREATE TABLE IF NOT EXISTS ledger (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL COLLATE NOCASE
                 CHECK (length(name) > 0 AND name = trim(name)),
    kind       TEXT    NOT NULL
                 CHECK (kind IN ('manual', 'backtest')),
    created_at INTEGER NOT NULL CHECK (created_at >= 0),
    updated_at INTEGER NOT NULL CHECK (updated_at >= 0)
) STRICT;

CREATE UNIQUE INDEX IF NOT EXISTS ledger_manual_name
    ON ledger (name COLLATE NOCASE)
    WHERE kind = 'manual';

CREATE TABLE IF NOT EXISTS trade_fill (
    id              INTEGER PRIMARY KEY,
    ledger_id       INTEGER NOT NULL
                      REFERENCES ledger(id) ON DELETE CASCADE ON UPDATE RESTRICT,
    instrument_id   INTEGER NOT NULL
                      REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    asset_kind      TEXT    NOT NULL
                      CHECK (asset_kind IN ('equity', 'etf', 'option')),
    expiration      INTEGER
                      CHECK (expiration IS NULL OR
                             (expiration >= 19000101 AND expiration <= 21001231)),
    expiration_type TEXT
                      CHECK (expiration_type IS NULL OR
                             expiration_type IN ('weekly', 'monthly')),
    strike          REAL
                      CHECK (strike IS NULL OR strike > 0),
    right           TEXT
                      CHECK (right IS NULL OR right IN ('call', 'put')),
    ts              INTEGER NOT NULL CHECK (ts >= 0),
    quantity        REAL    NOT NULL CHECK (quantity != 0),
    price           REAL    NOT NULL CHECK (price >= 0),
    fees            REAL    NOT NULL CHECK (fees >= 0),
    note            TEXT,
    external_id     TEXT
                      CHECK (external_id IS NULL OR
                             (length(external_id) > 0 AND
                              external_id = trim(external_id))),
    CHECK (
        (asset_kind IN ('equity', 'etf')
            AND expiration IS NULL
            AND expiration_type IS NULL
            AND strike IS NULL
            AND right IS NULL)
        OR (asset_kind = 'option'
            AND expiration IS NOT NULL
            AND expiration_type IS NOT NULL
            AND strike IS NOT NULL
            AND right IS NOT NULL)
    )
) STRICT;

CREATE INDEX IF NOT EXISTS trade_fill_ledger_ts
    ON trade_fill (ledger_id, ts, id);

CREATE INDEX IF NOT EXISTS trade_fill_instrument
    ON trade_fill (instrument_id);

CREATE UNIQUE INDEX IF NOT EXISTS trade_fill_external_id
    ON trade_fill (ledger_id, external_id)
    WHERE external_id IS NOT NULL;

CREATE TABLE IF NOT EXISTS ledger_cash_flow (
    id          INTEGER PRIMARY KEY,
    ledger_id   INTEGER NOT NULL
                  REFERENCES ledger(id) ON DELETE CASCADE ON UPDATE RESTRICT,
    ts          INTEGER NOT NULL CHECK (ts >= 0),
    amount      REAL    NOT NULL CHECK (amount != 0),
    note        TEXT,
    external_id TEXT
                  CHECK (external_id IS NULL OR
                         (length(external_id) > 0 AND
                          external_id = trim(external_id)))
) STRICT;

CREATE INDEX IF NOT EXISTS ledger_cash_flow_ledger_ts
    ON ledger_cash_flow (ledger_id, ts, id);

CREATE UNIQUE INDEX IF NOT EXISTS ledger_cash_flow_external_id
    ON ledger_cash_flow (ledger_id, external_id)
    WHERE external_id IS NOT NULL;

CREATE TABLE IF NOT EXISTS backtest_run (
    id             INTEGER PRIMARY KEY,
    ledger_id      INTEGER NOT NULL UNIQUE
                     REFERENCES ledger(id) ON DELETE CASCADE ON UPDATE RESTRICT,
    strategy_id    TEXT    NOT NULL
                     CHECK (length(strategy_id) > 0 AND strategy_id = trim(strategy_id)),
    params_json    TEXT    NOT NULL CHECK (json_valid(params_json)),
    config_json    TEXT    NOT NULL CHECK (json_valid(config_json)),
    instrument_id  INTEGER NOT NULL
                     REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    timeframe_s    INTEGER NOT NULL CHECK (timeframe_s > 0),
    ts_begin       INTEGER NOT NULL CHECK (ts_begin >= 0),
    ts_end         INTEGER NOT NULL CHECK (ts_end > ts_begin),
    engine_version INTEGER NOT NULL CHECK (engine_version > 0),
    created_at     INTEGER NOT NULL CHECK (created_at >= 0)
) STRICT;

CREATE INDEX IF NOT EXISTS backtest_run_instrument
    ON backtest_run (instrument_id);
