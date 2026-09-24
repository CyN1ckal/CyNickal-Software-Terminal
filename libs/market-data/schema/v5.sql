-- Copyright 2026 CyNickal Software LLC
-- SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

-- libs/market-data/schema/v5.sql
-- market-data schema v5: hypothetical portfolios.
-- Applied when user_version is 0 (after v4.sql) or 4 (v4 tables already exist).
-- Idempotent: CREATE IF NOT EXISTS so a crashed migrate can retry
-- after a rollback; user_version is set only after success.
--
-- Does not ALTER instrument, instrument_listing, or instrument_current.
-- The public name of a non-cash holding is instrument.figi. This table
-- stores instrument.id, the same key bar uses. A copied FIGI would drift.
-- Cash is the only row with a null instrument_id. The v1 API treats that
-- row as USD. This CHECK does not spell the currency token.
-- Quantity is the only magnitude. Positive is long, negative is short.
-- Equity and ETF quantity is shares. Option quantity is contracts.
-- Weights, marks, lots, and the contract multiplier are not columns.

CREATE TABLE IF NOT EXISTS portfolio (
    id         INTEGER PRIMARY KEY,
    name       TEXT    NOT NULL COLLATE NOCASE
                 CHECK (length(name) > 0 AND name = trim(name)),
    created_at INTEGER NOT NULL CHECK (created_at >= 0),
    updated_at INTEGER NOT NULL CHECK (updated_at >= 0)
) STRICT;

CREATE UNIQUE INDEX IF NOT EXISTS portfolio_name
    ON portfolio (name COLLATE NOCASE);

CREATE TABLE IF NOT EXISTS portfolio_holding (
    id              INTEGER PRIMARY KEY,
    portfolio_id    INTEGER NOT NULL
                      REFERENCES portfolio(id) ON DELETE CASCADE ON UPDATE RESTRICT,
    instrument_id   INTEGER
                      REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    asset_kind      TEXT    NOT NULL
                      CHECK (asset_kind IN ('equity', 'etf', 'option', 'cash')),
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
    vendor_symbol   TEXT
                      CHECK (vendor_symbol IS NULL OR
                             (length(vendor_symbol) > 0 AND
                              vendor_symbol = trim(vendor_symbol))),
    quantity        REAL    NOT NULL CHECK (quantity != 0),
    CHECK (
        (asset_kind IN ('equity', 'etf')
            AND instrument_id IS NOT NULL
            AND expiration IS NULL
            AND expiration_type IS NULL
            AND strike IS NULL
            AND right IS NULL
            AND vendor_symbol IS NULL)
        OR (asset_kind = 'cash'
            AND instrument_id IS NULL
            AND expiration IS NULL
            AND expiration_type IS NULL
            AND strike IS NULL
            AND right IS NULL
            AND vendor_symbol IS NULL)
        OR (asset_kind = 'option'
            AND instrument_id IS NOT NULL
            AND expiration IS NOT NULL
            AND expiration_type IS NOT NULL
            AND strike IS NOT NULL
            AND right IS NOT NULL)
    )
) STRICT;

CREATE UNIQUE INDEX IF NOT EXISTS portfolio_holding_spot
    ON portfolio_holding (portfolio_id, instrument_id)
    WHERE asset_kind IN ('equity', 'etf');

CREATE UNIQUE INDEX IF NOT EXISTS portfolio_holding_option
    ON portfolio_holding (
        portfolio_id,
        instrument_id,
        expiration,
        expiration_type,
        strike,
        right
    )
    WHERE asset_kind = 'option';

CREATE UNIQUE INDEX IF NOT EXISTS portfolio_holding_cash
    ON portfolio_holding (portfolio_id)
    WHERE asset_kind = 'cash';

CREATE UNIQUE INDEX IF NOT EXISTS portfolio_holding_vendor_symbol
    ON portfolio_holding (portfolio_id, vendor_symbol)
    WHERE vendor_symbol IS NOT NULL;
