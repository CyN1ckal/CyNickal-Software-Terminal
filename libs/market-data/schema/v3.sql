-- Copyright 2026 CyNickal Software LLC
-- SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

-- market-data schema v3: MBoum GET /v3/markets/options chain snapshots.
-- Applied when PRAGMA user_version < 3, after v1 and v2 tables exist.
-- Idempotent: CREATE IF NOT EXISTS so a crashed migrate can retry
-- after a rollback; user_version is set only after success.
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
