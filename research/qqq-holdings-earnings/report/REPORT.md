# QQQ holdings earnings

This study was not run. Daily bars exist, but an earnings setup needs the announcement date and whether it was before or after the close. A single `next_earnings` stamp cannot reconstruct 2021–2026 events. Announcements were not approximated from fiscal period ends. No date that is not already stored is proposed as a fill-in. A future study needs a point-in-time announcement calendar loaded before any return is computed. That is an idea only. No result is attached to it.

No return, forward return, hit rate, or price chart was computed.

| | |
|---|---|
| Date | 2026-10-03 |
| Status | **Not run.** The store has no historical earnings calendar. |
| Instruments | The 101 named holdings. Both GOOGL and GOOG are separate instruments. All 101 are equities with an open listing. |
| Data | Daily sessions with bars, last session 2026-10-02, read via `agent-data/mdq.py`. Prices were not read. |
| Rules | None. Nothing was locked. |

## Next earnings

`option_underlying` holds one row per instrument. The primary key is `instrument_id`, and a later chain refresh replaces that row. `next_earnings` is a single `YYYYMMDD` date. `earnings_time` is a single text flag, such as `After Close`, or null. The options chain this row came from is a snapshot, not a history.

The table has four rows in the whole store. Two of the 101 holdings have one:

| Symbol | next_earnings | earnings_time | fetched_at (UTC) |
|---|---|---|---|
| META | 2026-11-04 | null | 2026-09-27T00:21:44Z |
| ADBE | 2026-12-09 | After Close | 2026-09-25T22:06:53Z |

The other two rows are SPY and `$SPX`. Both of their earnings fields are null. They are not in the 101.

The other 99 holdings have no `option_underlying` row. Both dates above are after the last stored session, 2026-10-02, so neither stamp falls on a stored bar. META's row also has no before-or-after-close flag.

## Statements

Three of the 101 holdings have a `statement_snapshot`. A snapshot is one fetched grid for one statement and one timeframe. `period_end` on `statement_cell` is the fiscal period end (`YYYY-MM-DD`, or `TTM`). It is not the announcement date. Those period ends were not read, and they are not treated as earnings dates.

| Symbol | Statement | Timeframe | Fiscal periods | fetched_at (UTC) |
|---|---|---|---|---|
| ADBE | balance | annually | 6 | 2026-09-25T21:50:51Z |
| ADBE | cashflow | annually | 6 | 2026-09-25T21:51:05Z |
| ADBE | income | annually | 5 | 2026-09-25T21:51:08Z |
| ADBE | income | quarterly | 20 | 2026-09-25T21:51:22Z |
| GOOGL | balance | annually | 6 | 2026-09-25T05:07:56Z |
| META | balance | annually | 6 | 2026-09-24T02:39:53Z |
| META | income | quarterly | 20 | 2026-09-25T22:12:19Z |

The other 98 holdings have no statement snapshot. GOOG has none. GOOGL has only the annual balance sheet above.

Stored line-item names that contain "earn" are retained-earnings and unearned-revenue labels. None is an announcement date. QQQ and SPY each have an income quarterly snapshot with zero periods. Those rows are the ETFs, not holdings, and they add no dates.

## No other earnings history

The live tables are `instrument`, `instrument_listing`, `bar`, `corporate_action`, `coverage_day`, `statement_snapshot`, `statement_cell`, `option_underlying`, `option_expiry`, `option_quote`, `portfolio`, `portfolio_holding`, `ledger`, `trade_fill`, `ledger_cash_flow`, and `backtest_run`, plus the `instrument_current` view. The only object whose definition mentions `next_earnings` or `earnings_time` is `option_underlying`. There is no announcement table. There is also no point-in-time membership table for QQQ. The only portfolios stored are `Hypo` (4 holdings) and `New` (empty).

## Daily bars

All 101 symbols have at least one daily session with bars. The last of those sessions is 2026-10-02 for every name. The earliest first session is 2021-09-16. Session counts run from 77 to 1,267. Ten names are in the store only from a later first session:

| Symbol | First session with a daily bar | Sessions with a daily bar |
|---|---|---:|
| CEG | 2022-01-19 | 1,181 |
| WBD | 2022-04-04 | 1,129 |
| GEHC | 2022-12-15 | 952 |
| FER | 2023-08-01 | 635 |
| ARM | 2023-09-14 | 766 |
| ALAB | 2024-03-20 | 637 |
| NBIS | 2024-10-21 | 489 |
| SNDK | 2025-02-13 | 411 |
| CRWV | 2025-03-28 | 381 |
| HONA | 2026-06-15 | 77 |

That is a count of sessions with bars, not a check that every session in between is present, and it is not a return. One-minute bars exist for GOOGL and ADBE only. Minutes do not supply an announcement date.

These 101 names are the current holdings list that was ingested. They are not the membership of QQQ on earlier dates.

## Prior exposure

No study under `research/` locks a pre-earnings or post-earnings rule, and none reports an announcement-date test. A text search of the study rules and reports finds the word only as an aside: news days, or a filter the rules forbid. Those studies' results are not used here. Several of them traded the QQQ ETF. That is not these 101 stocks, and it is not an earnings reaction.

## Idea for a later study

A later study can test pre-earnings and post-earnings setups only after a point-in-time announcement calendar is loaded: the announcement date, and whether it was before or after the close, as known at the time. That calendar has to be in the store before any return is computed, and the study needs its own locked rules. Fiscal period ends are not that calendar. The two `next_earnings` stamps are not that calendar. The current 101-name list is not a point-in-time membership history either. This study did not run that test and has nothing to carry forward.

## Looks

Read-only `mdq.py` on 2026-10-03: the table list; a schema search for earnings columns; every `option_underlying` row's symbol, `next_earnings`, `earnings_time`, and `fetched_at`; every `statement_snapshot`, with a count of distinct period ends and no period-end values; statement line-item names matching earn, announce, report, or call; session counts and first and last session for 1d and 1m; portfolio names. No bar prices. No ingest. `research/README.md` was not edited.
