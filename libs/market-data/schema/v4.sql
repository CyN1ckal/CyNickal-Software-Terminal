-- Copyright 2026 CyNickal Software LLC
-- SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

-- market-data schema v4: the baseline. See docs/composite-figi-identity.md.
-- Frozen. Do not ALTER in place; add schema/v5.sql and bump kSchemaUserVersion.
-- Applied only when PRAGMA user_version = 0. Files stamped 1..3 predate the
-- FIGI identity and are refused; they do not migrate.
-- Idempotent: CREATE IF NOT EXISTS so a crashed create can retry
-- after a rollback; user_version is set only after success.
--
-- Identity. instrument.id is the key every fact row references. figi is the
-- OpenFIGI composite FIGI for an equity or ETF, or the index FIGI for an
-- index (those have no composite). It is required for equity, etf, and index
-- rows and unique when present. The check digit is verified in C++
-- (market_data/Figi.h), not here.
--
-- The ticker lives only in instrument_listing. One open listing per
-- instrument and per symbol. opened_at and closed_at are when this store
-- bound or unbound the ticker, not market dates; do not use them for
-- point-in-time symbol lookups. close_reason: renamed (the instrument moved
-- to another ticker), delisted (it no longer trades under any ticker), or
-- manual (ingest --delist).
--
-- verified_at is the last time OpenFIGI confirmed that the open listing's
-- ticker names this FIGI. A detected conflict clears it.
--
-- instrument_current is each instrument with its open listing, or its most
-- recently closed one when none is open.

CREATE TABLE IF NOT EXISTS instrument (
    id              INTEGER PRIMARY KEY,
    figi            TEXT
                      CHECK (figi IS NULL OR (
                          length(figi) = 12
                          AND figi GLOB '[B-DF-HJ-NP-TV-Z][B-DF-HJ-NP-TV-Z]G[0-9B-DF-HJ-NP-TV-Z][0-9B-DF-HJ-NP-TV-Z][0-9B-DF-HJ-NP-TV-Z][0-9B-DF-HJ-NP-TV-Z][0-9B-DF-HJ-NP-TV-Z][0-9B-DF-HJ-NP-TV-Z][0-9B-DF-HJ-NP-TV-Z][0-9B-DF-HJ-NP-TV-Z][0-9]'
                          AND substr(figi, 1, 2) NOT IN ('BS', 'BM', 'GG', 'GB', 'GH', 'KY', 'VG'))),
    asset_class     TEXT    NOT NULL
                      CHECK (asset_class IN ('equity','etf','index','future','crypto','other')),
    currency        TEXT    NOT NULL DEFAULT 'USD',
    timezone        TEXT    NOT NULL DEFAULT 'America/New_York',
    name            TEXT,
    listed_at       INTEGER CHECK (listed_at IS NULL OR listed_at >= 0),
    delisted_at     INTEGER CHECK (delisted_at IS NULL OR delisted_at >= 0),
    created_at      INTEGER NOT NULL CHECK (created_at >= 0),
    verified_at     INTEGER CHECK (verified_at IS NULL OR verified_at >= 0),
    CHECK (delisted_at IS NULL OR listed_at IS NULL OR delisted_at >= listed_at),
    CHECK (figi IS NOT NULL OR asset_class IN ('future', 'crypto', 'other'))
) STRICT;

CREATE UNIQUE INDEX IF NOT EXISTS instrument_figi
    ON instrument (figi) WHERE figi IS NOT NULL;

CREATE TABLE IF NOT EXISTS instrument_listing (
    id            INTEGER PRIMARY KEY,
    instrument_id INTEGER NOT NULL
                    REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    symbol        TEXT    NOT NULL COLLATE NOCASE
                    CHECK (length(symbol) > 0 AND symbol = trim(symbol)),
    opened_at     INTEGER NOT NULL CHECK (opened_at >= 0),
    closed_at     INTEGER CHECK (closed_at IS NULL OR closed_at >= opened_at),
    close_reason  TEXT    CHECK (close_reason IS NULL
                                 OR close_reason IN ('renamed', 'delisted', 'manual')),
    CHECK ((closed_at IS NULL) = (close_reason IS NULL))
) STRICT;

CREATE UNIQUE INDEX IF NOT EXISTS instrument_listing_open_instrument
    ON instrument_listing (instrument_id) WHERE closed_at IS NULL;

CREATE UNIQUE INDEX IF NOT EXISTS instrument_listing_open_symbol
    ON instrument_listing (symbol) WHERE closed_at IS NULL;

CREATE INDEX IF NOT EXISTS instrument_listing_symbol
    ON instrument_listing (symbol, closed_at);

CREATE VIEW IF NOT EXISTS instrument_current AS
SELECT i.id, i.figi, i.asset_class, i.currency, i.timezone, i.name,
       i.listed_at, i.delisted_at, i.created_at, i.verified_at,
       l.symbol, l.closed_at AS listing_closed_at
  FROM instrument AS i
  JOIN instrument_listing AS l ON l.id = (
        SELECT id FROM instrument_listing
         WHERE instrument_id = i.id
         ORDER BY closed_at IS NOT NULL, opened_at DESC, id DESC
         LIMIT 1);

-- Bars, corporate actions, and coverage (unchanged from v1).

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

-- MBoum modules financial statements (unchanged from v2).
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

-- MBoum GET /v3/markets/options chain snapshots (unchanged from v3).
--
-- The endpoint returns one underlying's current chain, not a history.
-- Query ticker is required. expiration=YYYY-MM-DD is optional and selects
-- one date; omitting it still returns a single server-chosen date plus the
-- full expiration calendar. On 2026-09-22 that default was 2026-09-25, not
-- the nearest listed date. HTTP status stays 200 for an unknown ticker and
-- for a date that is not listed.
--
-- meta.expirations is an object of ISO dates (weekly, monthly) for a real
-- underlying, including when body is empty. An unknown ticker sends
-- expirations as an empty array and body as an empty array. A listed date
-- can sit in both arrays: $SPX third Fridays are weekly and monthly.
--
-- body is an object with Call and Put arrays, or an empty array when that
-- date has no contracts. Every quote field arrives as a display string.
-- Prices, strikes, and greeks use thousands separators once they reach
-- 1,000 ($SPX bid "4,565.70", strike "3,200.00"). volume, openInterest,
-- and openInterestChange do too. Percents keep a trailing %. priceChange,
-- percentChange, and openInterestChange use the token unch for zero.
-- tradeTime is N/A, a prior session MM/DD/YY, or a same-session HH:MM ET.
-- There is no gamma, no bid size, and no underlying last. symbolCode is in
-- the field glossary and is absent on the contracts. symbolType repeats
-- optionType. rho is present. The vendor symbol is
-- BASE|YYYYMMDD|STRIKE[W]C/P. The W marks the SPX weekly (SPXW) product;
-- equity weeklies such as AAPL omit it. Weekly and monthly contracts on the
-- same $SPX date share strikes and differ in price, so the symbol is the
-- contract identity and the expiration type is part of the chain key.
-- averageVolatility is the at-the-money implied vol of that type and date
-- (11.25% weekly vs 11.09% monthly on $SPX 2026-10-16). Historic vol, IV
-- rank, earnings, and the dividend date are constant across the response
-- and belong to the underlying. N/A and -- are missing values, not text.
--
-- Stored percents are fractions: "24.94%" is 0.2494 and unch is 0.
-- A positive moneyness means the strike is below the underlying, so a call
-- is in the money and the put at that strike is out. Dates are YYYYMMDD.
-- Replacing one (underlying, expiration, type) drops that slice's quotes
-- and leaves every other slice. A calendar refresh drops slices the vendor
-- no longer lists. Quotes are not a time series.

CREATE TABLE IF NOT EXISTS option_underlying (
    instrument_id    INTEGER PRIMARY KEY
                       REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    source           TEXT    NOT NULL DEFAULT 'mboum'
                       CHECK (length(source) > 0 AND source = trim(source)),
    fetched_at       INTEGER NOT NULL CHECK (fetched_at >= 0),
    historic_vol_30d REAL    CHECK (historic_vol_30d IS NULL OR historic_vol_30d >= 0),
    iv_rank_1y       REAL    CHECK (iv_rank_1y IS NULL OR iv_rank_1y >= 0),
    next_earnings    INTEGER
                       CHECK (next_earnings IS NULL OR
                              (next_earnings >= 19000101 AND next_earnings <= 21001231)),
    dividend_ex      INTEGER
                       CHECK (dividend_ex IS NULL OR
                              (dividend_ex >= 19000101 AND dividend_ex <= 21001231)),
    earnings_time    TEXT
                       CHECK (earnings_time IS NULL OR
                              (length(earnings_time) > 0 AND earnings_time = trim(earnings_time)))
) STRICT;

CREATE TABLE IF NOT EXISTS option_expiry (
    instrument_id   INTEGER NOT NULL
                      REFERENCES instrument(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    expiration      INTEGER NOT NULL
                      CHECK (expiration >= 19000101 AND expiration <= 21001231),
    expiration_type TEXT    NOT NULL
                      CHECK (expiration_type IN ('weekly', 'monthly')),
    average_iv      REAL    CHECK (average_iv IS NULL OR average_iv >= 0),
    fetched_at      INTEGER CHECK (fetched_at IS NULL OR fetched_at >= 0),
    source          TEXT    NOT NULL DEFAULT 'mboum'
                      CHECK (length(source) > 0 AND source = trim(source)),
    PRIMARY KEY (instrument_id, expiration, expiration_type)
) WITHOUT ROWID, STRICT;

CREATE TABLE IF NOT EXISTS option_quote (
    instrument_id         INTEGER NOT NULL,
    expiration            INTEGER NOT NULL,
    expiration_type       TEXT    NOT NULL
                            CHECK (expiration_type IN ('weekly', 'monthly')),
    vendor_symbol         TEXT    NOT NULL
                            CHECK (length(vendor_symbol) > 0 AND vendor_symbol = trim(vendor_symbol)),
    strike                REAL    NOT NULL CHECK (strike > 0),
    right                 TEXT    NOT NULL CHECK (right IN ('call', 'put')),
    bid                   REAL    NOT NULL CHECK (bid >= 0),
    ask                   REAL    NOT NULL CHECK (ask >= 0),
    mid                   REAL    NOT NULL CHECK (mid >= 0),
    last                  REAL    NOT NULL CHECK (last >= 0),
    price_change          REAL    NOT NULL,
    percent_change        REAL    NOT NULL,
    volume                INTEGER NOT NULL CHECK (volume >= 0),
    open_interest         INTEGER NOT NULL CHECK (open_interest >= 0),
    open_interest_change  INTEGER NOT NULL,
    implied_vol           REAL    NOT NULL CHECK (implied_vol >= 0),
    delta                 REAL    NOT NULL,
    rho                   REAL    NOT NULL,
    vega                  REAL    NOT NULL,
    theta                 REAL    NOT NULL,
    moneyness             REAL    NOT NULL,
    days_to_expiration    INTEGER NOT NULL CHECK (days_to_expiration >= 0),
    trade_date            INTEGER
                            CHECK (trade_date IS NULL OR
                                   (trade_date >= 19000101 AND trade_date <= 21001231)),
    trade_minute          INTEGER
                            CHECK (trade_minute IS NULL OR
                                   (trade_minute >= 0 AND trade_minute < 1440)),
    fetched_at            INTEGER NOT NULL CHECK (fetched_at >= 0),
    PRIMARY KEY (instrument_id, vendor_symbol),
    FOREIGN KEY (instrument_id, expiration, expiration_type)
        REFERENCES option_expiry (instrument_id, expiration, expiration_type)
        ON DELETE CASCADE ON UPDATE RESTRICT,
    CHECK (trade_date IS NULL OR trade_minute IS NULL)
) WITHOUT ROWID, STRICT;

CREATE INDEX IF NOT EXISTS option_quote_chain
    ON option_quote (instrument_id, expiration, expiration_type, strike, right);
