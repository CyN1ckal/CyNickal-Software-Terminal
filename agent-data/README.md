# agent-data: how agents query the market-data store

Everything the terminal knows lives in one SQLite file, `data/market-data.sqlite`: bars, splits, coverage, financial statements, option chains, portfolios, trade ledgers, and backtest runs. **Read it through `mdq.py` in this directory. Do not open it with `sqlite3` and write SQL by hand.**

`mdq.py` handles the conventions below for you, so its results match the terminal's charts and statistics. When hand-written SQL gets one of them wrong, you get no error. You just get wrong numbers.

- Look up symbols through the listing history, never with `instrument.id` constants.
- Split-adjust backward, matching `Adjust.h`.
- Build 5m/15m/1h bars from 1m bars anchored at the 09:30 ET open, matching `CChartTransform.h`.
- Convert between UTC timestamps and New York sessions, including DST.
- Use the NYSE calendar, with early closes and special closures.
- Match ledger lots first-in first-out, apply splits, and use the ×100 option multiplier, matching `trading/Ledger.h`.
- Open the file read-only, so running the terminal or `ingest` at the same time is safe.

It is a single stdlib-only Python 3.10+ file. pandas is optional.

## Rules

1. **Read only.** Never write to the store, and never run `ingest` unless the user asks. It calls a paid API. mdq opens the database with `mode=ro` and `PRAGMA query_only`.
2. **Start with the CLI to explore, and use the Python API for analysis.** CLI output is capped at 2,000 rows, so large pulls should go to `--out file.csv` or into Python.
3. **Check coverage before you draw conclusions.** Run `mdq.py coverage SYM --tf 1m` first. A missing session looks the same as a quiet market.
4. **Use raw SQL only as a last resort,** through `mdq.py sql`, and only after reading [SCHEMA.md](SCHEMA.md). If you need the same query twice, add a method to `mdq.py` with a test instead.
5. **Put research output in `research/<slug>/research/`**, not in `data/`. See [Writing research](#writing-research).

## Quick start

Run these from the repo root. Options go **after** the command.

```bash
python agent-data/mdq.py instruments
```

```bash
python agent-data/mdq.py bars QQQ --tf 5m --last 3 -f csv
```

```bash
python agent-data/mdq.py coverage SPY --tf 1m
```

`instruments` lists what is stored. It shows each symbol, its FIGI, and the first and last session for 1m and 1d. Check it first: most symbols have daily history only.

## CLI reference

`python agent-data/mdq.py <command> [args] [-f table|csv|json] [-o FILE] [-n LAST_N] [--db PATH]`

| Command | What it returns |
|---|---|
| `instruments` | Every instrument: id, symbol, FIGI, class, name, and stored 1m/1d session ranges |
| `resolve SYM` | One instrument by symbol (case-insensitive), FIGI, or id |
| `bars SYM [--tf 1m\|5m\|15m\|30m\|1h\|1d] [--from D] [--to D] [--last N] [--raw]` | OHLCV. Split-adjusted unless `--raw`. `--from`/`--to` are inclusive session dates. `--last N` returns the most recent N sessions |
| `coverage SYM [--tf 1m\|1d]` | Summary: counts by status, first and last session, sessions that need attention |
| `coverage SYM --days [--from D] [--to D]` | One row per session, with an explanatory `note` |
| `actions SYM` | Splits and dividends, with ex-dates |
| `statement --list` | Which financial statements are stored, per symbol |
| `statement SYM -k income\|balance\|cashflow -p annually\|quarterly\|trailing [--items a,b]` | One row per period, one column per vendor line item |
| `options SYM --expirations` / `--underlying` / `[--exp D] [--type weekly\|monthly]` | Stored option chain snapshot |
| `portfolios` / `holdings NAME_OR_ID` | Hypothetical portfolios |
| `ledgers` | Manual and backtest ledgers, with fill counts and strategy params |
| `fills L` / `trips L` / `book L` / `backtest L` | Raw fills, FIFO round trips, positions and cash, and backtest config for ledger id or name `L` |
| `sessions FROM TO` | NYSE sessions with expected 1m bar counts (390, or 211 on an early close) |
| `sql "SELECT ..." [params...]` | One read-only statement with `?` params |
| `schema` | The live DDL |

Dates are accepted as `YYYY-MM-DD` or `YYYYMMDD`.

## Python API

```python
import sys
sys.path.insert(0, "agent-data")          # path relative to the repo root
from mdq import MarketData, to_frame, nyse_sessions, rth_minutes

with MarketData() as md:                  # or MarketData("path/to/copy.sqlite")
    bars = md.bars("QQQ", "15m", start="2026-01-02", end="2026-09-25")
    bars[0].ts, bars[0].time, bars[0].session, bars[0].close   # UTC secs, NY datetime, NY date
    daily = md.bars("SPY", "1d", last=250)
    raw = md.bars("AMZN", "1d", adjust=False)                 # as-traded prices

    md.coverage_summary("SPY", "1m")      # dict; check needs_attention first
    md.corporate_actions("AAPL")
    md.statement("ADBE", "income", "quarterly", items=["revenue", "netinccmn"])
    md.option_chain("META", expiration="2026-10-16")
    md.ledgers(); md.backtest_run(6)
    book = md.ledger_book(6)              # round_trips, positions, realized_pnl, fees_paid, cash
    md.sql("SELECT ... WHERE x = ?", [value])   # read-only escape hatch

df = to_frame(bars)                       # pandas, indexed by New York time
```

`bars()` returns `Bar` objects with the fields `ts, open, high, low, close, volume`. Other methods return lists of dicts. Loading all 487k SPY 1m bars takes about 0.6 s.

## What the data means

### Identity
- `instrument.id` is the key on every fact row. It is local to this database, so never hard-code it: resolve by symbol or FIGI.
- A ticker lives only in `instrument_listing`. A symbol resolves to its open listing, or failing that, its most recently closed one. `instrument_current` (a view) gives each instrument with its current symbol.
- `$SPX` is an index. It has option data but no bars.

### Time
- Every `ts`/`*_at` column is **Unix seconds, UTC**. Sessions are **America/New_York** dates.
- A bar's `ts` is its **open**. A 1m bar at 15:59 ET covers 15:59:00–15:59:59.
- A **daily bar's `ts` is 09:30 ET of its session**, not midnight. A daily close is only known at 16:00 ET. Use it no earlier than `ts + 23400` to avoid look-ahead.
- `session_date`, `expiration`, `next_earnings`, and similar integer date columns are `YYYYMMDD`.

### Bars
- Only **1m and 1d are stored** (`timeframe_s` 60 and 86400). Every other timeframe is built from 1m. Buckets start at 09:30 ET, never span sessions, and a partial bucket still emits. For example, the last 1h bar is 15:30–16:00.
- 1m bars are **regular trading hours only** (09:30–15:59 ET). Extended hours are not stored, and the forming (still-open) bar is never written.
- A 1m minute with no trades has **no row**. Nothing is forward-filled. Thin names such as IGV often have 300–380 bars a session.
- Stored prices are **as traded**. mdq split-adjusts backward by default: for each split with `ex_ts > bar.ts`, OHLC ÷ ratio and volume × ratio. The ex-date bar is already post-split. **Dividends are not adjusted.**

### Coverage (`coverage_day`)
Each row records what one ingest found for one instrument, timeframe, and session. `status` is `complete`, `partial`, `missing`, or `error`. A holiday is recorded as `complete` with 0 bars. Known benign cases, which mdq labels `expected: ...`:
- **Early closes** (the day after Thanksgiving, Christmas Eve, July 3): 211 bars, `partial`.
- **2025-01-09** (the Carter day of mourning): `missing`.

### Fundamentals
- `statement` is `income`, `balance`, or `cashflow`. `timeframe` is `annually`, `quarterly`, or `trailing`. These are three separate series, not views of one another.
- Line items keep the vendor's spellings (`revenue`, `netinccmn`, `epsdil`, `defferedTaxAssets`). Run `statement SYM` with no `--items` to see them all.
- A missing cell means **unknown, not zero**. `period_end = 'TTM'` is a column label, not a date. Values are absolute dollars and share counts, and ratios are fractions.

### Options
- A chain is a **snapshot from `fetched_at`, not a history**. Each refresh replaces the previous one.
- Percent fields (`implied_vol`, `percent_change`, `average_iv`, `iv_rank_1y`) are **fractions**. The contract multiplier (100) is not stored.
- `expiration_type` is part of the key: `$SPX` weekly and monthly contracts on the same date are different contracts.

### Portfolios and ledgers
- A **portfolio** is hand-edited quantities: long is +, short is −, and cash is the row with `asset_kind = 'cash'`.
- A **ledger** is fills. Positions, round trips, and P&L are derived on read and never stored. `quantity` is signed (+ buy, − sell). Fees are never negative. Starting capital is a `ledger_cash_flow` row.
- A backtest ledger has one `backtest_run` row holding `strategy_id`, `params_json`, and `config_json`. `ts_begin` is the first bar's open, and `ts_end` is the last bar's close time. A `close_at_end` fill lands exactly at `ts_end`.
- `mdq.py book` and `trips` match fills the way the terminal does: FIFO per position key, a split rescales open share lots on its ex-date, options use ×100, and fees are allocated pro rata.

## Known data issues

| Issue | Effect | How mdq reports it |
|---|---|---|
| `NyseCalendar.cpp` observes a Saturday New Year's Day on the preceding Friday. NYSE does not. | **2021-12-31 has no 1m bars** for QQQ, SPY, or IGV. Coverage says `complete` with 0 bars. The 1d rows are real bars marked `partial`. | `coverage` lists it under `needs_attention`, and `is_nyse_holiday` in mdq treats it as a session |
| The C++ calendar has no early closes. | Early-close sessions show as `partial`. | Noted as `expected: 13:00 early close` |
| Thinly traded ETFs have untraded minutes. | `partial` 1m sessions (IGV, 2021–2025) | Noted as `fewer minutes than expected` |
| Some `statement_snapshot` rows have 0 periods (ETFs have no statements). | Empty result | `statement --list` shows `periods = 0` |

## Raw SQL, if you must

Read [SCHEMA.md](SCHEMA.md) first, then:
- Join through `instrument_current` or `instrument_listing` to get symbols.
- Always filter `bar` by `instrument_id` **and** `timeframe_s`, plus a `ts` range. The primary key is `(instrument_id, timeframe_s, ts)`.
- Apply split adjustment yourself from `corporate_action` (`type = 'split'`).
- Convert `ts` to New York time yourself. Do not use SQLite's `'localtime'`, which uses the machine's time zone.

```bash
python agent-data/mdq.py sql "SELECT c.symbol, count(*) AS days FROM coverage_day d JOIN instrument_current c ON c.id = d.instrument_id WHERE d.timeframe_s = ? AND d.bar_count > 0 GROUP BY c.symbol" 60
```

## Writing research

Each strategy gets one folder, `research/<slug>/`. Scripts, rules, and raw output go in `research/<slug>/research/`, and the report goes in `research/<slug>/report/`. [The research protocol](../.claude/skills/quant-research/SKILL.md) has the full layout. Scripts import mdq instead of re-deriving the calendar, split factors, and session times:

```python
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[3]   # research/<slug>/research/script.py
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions, rth_minutes
```

The protocol's lock and run-log steps (`RULES.lock`, `RUNLOG.md`) are required for new studies. Studies that predate the protocol do not have them.

In the write-up, record the session range you used and any coverage gaps you excluded.

## Changing mdq

- Keep it a single stdlib-only file, so any agent can run it with plain `python`.
- If a convention changes in the C++ code (`Adjust.h`, `CChartTransform.h`, `Ledger.h`, `NyseCalendar.cpp`, or the schema), update mdq and this README in the same change.
- Add a test to `test_mdq.py`. It builds a throwaway store from `libs/market-data/schema/*.sql`. Run it with `python agent-data/test_mdq.py`.
