# Store schema reference (user_version 6)

This page is for agents that need `mdq.py sql`. The authoritative DDL is in `libs/market-data/schema/v4.sql`, `v5.sql`, and `v6.sql`, and each file's header comments carry the vendor details. `python agent-data/mdq.py schema` prints the live DDL.

All tables are `STRICT`. Timestamps are Unix seconds UTC. Integer dates are `YYYYMMDD`. The key on every fact row is `instrument_id → instrument.id`.

## Identity

| Table | Key | Notes |
|---|---|---|
| `instrument` | `id` | `figi` is the composite FIGI (index FIGI for indexes). It is unique and required for `equity`, `etf`, and `index`. `asset_class` is one of `equity`, `etf`, `index`, `future`, `crypto`, `other`. `timezone` defaults to `America/New_York`. |
| `instrument_listing` | `id` | The only place a ticker is stored. `symbol` is `COLLATE NOCASE`. There is at most one open listing (`closed_at IS NULL`) per instrument and per symbol. `close_reason` is `renamed`, `delisted`, or `manual`. `opened_at` and `closed_at` are when this store bound the ticker, not market dates. |
| `instrument_current` (view) | `id` | The instrument plus its open listing, or its latest closed one. It adds `symbol` and `listing_closed_at`. **Join through this to get symbols.** |

## Prices

| Table | Key | Notes |
|---|---|---|
| `bar` | `(instrument_id, timeframe_s, ts)` `WITHOUT ROWID` | `timeframe_s` is only ever 60 or 86400. `ts` is the bar open, and a daily bar's is 09:30 ET. Prices are as traded (unadjusted). RTH only. Volume is shares. A `CHECK` enforces that high ≥ open, close ≥ low. |
| `corporate_action` | `id` | `type` is one of `split`, `dividend`, `spinoff`, `other`. `ex_ts` is 09:30 ET on the ex-date. `split_ratio` is new/old (20 for a 20-for-1). `amount` holds the dividend cash. |
| `coverage_day` | `(instrument_id, timeframe_s, session_date)` | One ingest verdict per session. `status` is one of `complete`, `partial`, `missing`, `error`. `bar_count`/`expected_count` (390 for 1m, 1 for 1d, 0 on a holiday). `first_ts`/`last_ts` bound the bars. `ingested_at` records when it was written. |

Split adjustment: `adjusted = raw / Π(split_ratio for splits with ex_ts > bar.ts)`, and volume is multiplied by the same product.

## Fundamentals

| Table | Key | Notes |
|---|---|---|
| `statement_snapshot` | `(instrument_id, statement, timeframe)` | One row per fetched grid, holding `fetched_at`. |
| `statement_cell` | `(instrument_id, statement, timeframe, line_item, period_end)` | `period_end` is `YYYY-MM-DD` or `TTM`. Exactly one of `value_int`, `value_real`, or `value_text` is set, and `value_kind` says which. `fiscalYear` and `fiscalQuarter` are ordinary text cells. |

Read a value with `CASE value_kind WHEN 'int' THEN value_int WHEN 'real' THEN value_real ELSE value_text END`.

## Options (a snapshot, not a time series)

| Table | Key | Notes |
|---|---|---|
| `option_underlying` | `instrument_id` | `historic_vol_30d` and `iv_rank_1y` are fractions. `next_earnings` and `dividend_ex` are `YYYYMMDD`. `earnings_time` is text such as `After Close`. |
| `option_expiry` | `(instrument_id, expiration, expiration_type)` | `expiration_type` is `weekly` or `monthly`. `average_iv` is the ATM IV for that date, as a fraction. |
| `option_quote` | `(instrument_id, vendor_symbol)` | `vendor_symbol` looks like `BASE\|YYYYMMDD\|STRIKE[W]C/P`. `right` is `call` or `put`. Holds bid, ask, mid, last, volume, open_interest, implied_vol, delta, rho, vega, and theta, but no gamma. `moneyness > 0` means the strike is below spot. `trade_date` or `trade_minute` holds the last trade: a prior-session date or a same-session minute of day. |

## Portfolios (v5)

| Table | Key | Notes |
|---|---|---|
| `portfolio` | `id` | `name` is unique and case-insensitive. |
| `portfolio_holding` | `id` | `asset_kind` is one of `equity`, `etf`, `option`, `cash`. Cash has a null `instrument_id` and is in USD. Option rows fill `expiration`, `expiration_type`, `strike`, and `right`. `quantity` is signed: shares, or contracts for options. |

## Ledgers and backtests (v6)

| Table | Key | Notes |
|---|---|---|
| `ledger` | `id AUTOINCREMENT` | `kind` is `manual` or `backtest`. Manual names are unique. Ids are never reused, because chartbooks store them. |
| `trade_fill` | `id` | `ledger_id` (CASCADE), `asset_kind` (`equity`, `etf`, or `option`, with the option contract columns), `ts`, and a signed `quantity`, where + is a buy. `price` is per share or per contract (×100 for option cash). `fees` ≥ 0. `external_id` is unique per ledger. Read in `ORDER BY ts, id`. |
| `ledger_cash_flow` | `id` | Deposits (+) and withdrawals (−), including starting capital. |
| `backtest_run` | `id` | One per backtest ledger. `strategy_id`, `params_json`, `config_json` (initial_cash, sizing, commissions, slippage_bps, stops, allow_short, flatten_at_session_end, close_at_end), `timeframe_s`, `ts_begin` (first bar open), `ts_end` (last bar close), and `engine_version`. |

A fill moves cash by `-quantity × price × multiplier - fees`, where the multiplier is 1 for shares and 100 for options. Positions and P&L are not stored. Use `mdq.py book` or `trips`.
