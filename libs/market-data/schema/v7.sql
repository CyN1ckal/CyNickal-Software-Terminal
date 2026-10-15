-- Copyright 2026 CyNickal Software LLC
-- SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

-- libs/market-data/schema/v7.sql
-- market-data schema v7: published short-sale restriction files.
-- Applied when user_version is 0 (after v6.sql), 4 (after v5 and v6),
-- 5 (after v6), or 6. Idempotent: CREATE IF NOT EXISTS so a crashed
-- migrate can retry after a rollback; user_version is set only after success.
--
-- Does not ALTER any earlier table. These rows are the publisher's lines,
-- not instruments. Most restricted names are not in this store, and a
-- warrant printed with an internal space is not one of our listings.
-- See docs/short-sale-restriction.md.
--
-- source nasdaq: ftp://ftp.nasdaqtrader.com/SymbolDirectory/shorthalts/
--   shorthaltsYYYYMMDD.txt. The file date is a session the name is
--   restricted. One symbol may have two trigger times on that file.
--   deactivate_session and ssr_state are null. file_created_at is the
--   trailer clock, New York time stored as Unix seconds UTC.
-- source nyse_group: the daily NYSE Group SSR workbook. The file lists
--   that session's triggers, not the names still restricted from an
--   earlier trigger. A trigger restricts trigger_session inclusive
--   through the day before deactivate_session. ssr_state is the
--   published code (the column is "SSR State", or "SSR Status" in
--   older workbooks). file_created_at is null.
-- A missing file is not a row. An empty parsed file is a file row
-- with row_count 0.

CREATE TABLE IF NOT EXISTS short_sale_restriction_file (
    source          TEXT    NOT NULL
                      CHECK (source IN ('nasdaq', 'nyse_group')),
    file_session    INTEGER NOT NULL
                      CHECK (file_session >= 20101110 AND file_session <= 21001231),
    file_name       TEXT    NOT NULL
                      CHECK (length(file_name) > 0 AND file_name = trim(file_name)),
    row_count       INTEGER NOT NULL
                      CHECK (row_count >= 0),
    file_created_at INTEGER
                      CHECK (file_created_at IS NULL OR file_created_at >= 0),
    fetched_at      INTEGER NOT NULL
                      CHECK (fetched_at >= 0),
    PRIMARY KEY (source, file_session)
) STRICT;

CREATE TABLE IF NOT EXISTS short_sale_restriction (
    source             TEXT    NOT NULL
                         CHECK (source IN ('nasdaq', 'nyse_group')),
    file_session       INTEGER NOT NULL
                         CHECK (file_session >= 20101110 AND file_session <= 21001231),
    symbol             TEXT    NOT NULL
                         CHECK (length(symbol) > 0
                                AND symbol = trim(symbol)
                                AND symbol = upper(symbol)),
    security_name      TEXT
                         CHECK (security_name IS NULL OR
                                (length(security_name) > 0
                                 AND security_name = trim(security_name))),
    listing_market     TEXT    NOT NULL
                         CHECK (length(listing_market) > 0
                                AND listing_market = trim(listing_market)),
    trigger_session    INTEGER NOT NULL
                         CHECK (trigger_session >= 20101110 AND trigger_session <= 21001231),
    trigger_ts         INTEGER NOT NULL
                         CHECK (trigger_ts >= 0),
    deactivate_session INTEGER
                         CHECK (deactivate_session IS NULL OR
                                (deactivate_session >= 20101110
                                 AND deactivate_session <= 21001231)),
    ssr_state          TEXT
                         CHECK (ssr_state IS NULL OR
                                (length(ssr_state) > 0 AND ssr_state = trim(ssr_state))),
    PRIMARY KEY (source, file_session, symbol, trigger_ts),
    CHECK (
        (source = 'nasdaq'
            AND deactivate_session IS NULL
            AND ssr_state IS NULL)
        OR
        (source = 'nyse_group'
            AND deactivate_session IS NOT NULL
            AND deactivate_session > trigger_session
            AND ssr_state IS NOT NULL)
    )
) STRICT;

CREATE INDEX IF NOT EXISTS short_sale_restriction_symbol
    ON short_sale_restriction (symbol, file_session);

CREATE INDEX IF NOT EXISTS short_sale_restriction_active
    ON short_sale_restriction (source, trigger_session, deactivate_session);
