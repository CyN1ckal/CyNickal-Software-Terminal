# Short-sale restriction data

| Field | Value |
|---|---|
| Status | Plan, before the loader runs |
| Date | 2026-10-14 |
| Scope | Source the published lists into the market-data store. No strategy return is computed here. |

This is the data step for a later study of short-sale restricted names. The study itself is not this change.

## What is being stored

A name is short-sale restricted when its listing market has triggered the Regulation SHO Rule 201 circuit breaker. Rule 201 (compliance date 2010-11-10) says the listing market decides that the price has fallen 10% or more from the prior regular-hours close. For the rest of that day, and the next day a national best bid is still being disseminated, a short sale may not be executed or displayed at or below the current national best bid, unless an exception applies (including a short-exempt mark). The listing market publishes the trigger. 17 CFR § 242.201.

This is not the Reg SHO threshold list (fails to deliver), not FINRA or exchange short-volume files, and not semi-monthly short interest. Those stay out of these tables.

## Sources

Two publishers cover the Nasdaq-listed names and the NYSE Group listings. Both archives were checked on 2026-10-14.

### Nasdaq

`ftp://ftp.nasdaqtrader.com/SymbolDirectory/shorthalts/shorthaltsYYYYMMDD.txt`

The directory begins with `shorthalts20110225.txt`. Earlier dates answer FTP 550. The web copy on nasdaqtrader.com sits behind an Incapsula page, so the loader uses the FTP file.

The body is CSV, not the pipe layout in Nasdaq's field notes:

```text
Symbol,Security Name,Market Category,Trigger Time
POM,POMDOCTOR LIMITED ADS,Q,10/12/2026 9:30:00 AM
POM,POMDOCTOR LIMITED ADS,Q,10/13/2026 9:30:00 AM
20261013163013
```

- `Market Category` is the Nasdaq code as printed (`Q`, `R`, and any later code).
- `Trigger Time` is an America/New_York wall clock.
- The last line is `YYYYMMDDHHMMSS`, also New York time, and means the file is complete.
- The file date is the session the name is restricted. The same symbol can appear twice, with two trigger times: the carry from the prior session and a new trigger. A 2026-10-13 file had 790 rows and 677 symbols, and no repeated symbol-plus-trigger.
- A missing session is FTP 550. That is not stored. A body that is not this CSV is a failure, not an empty day.

### NYSE Group

`https://ftp.nyse.com/NYSEGroupSSRCircuitBreakers/NYSEGroupSSRCircuitBreakers_YYYY/NYSEGroupSSRCircuitBreakers_YYYYMM/NYSEGroupSSRCircuitBreakersYYYYMMDD.xls`

`http://ftp.nyse.com/...` redirects there. Year directories exist from 2015 through the current year. The file is a BIFF `.xls` workbook, sheet `SSR_data`, one header row.

| Column | Notes |
|---|---|
| Trigger Date | `MM/DD/YYYY` |
| Issue Symbol | As printed. Warrants keep an internal space (`AAM WS`). |
| Issue Name | |
| Primary Exchange | Trimmed. Observed codes include `NYSE`, `MKT` (NYSE American; older files pad it), `ARCA`, and `P`. Stored as printed. Not mapped. |
| SSR State | Older files label this column `SSR Status`. The value seen so far is `A`. |
| Trigger Time | `HH:MM:SS` on the trigger date, New York time. |
| Anticipated Deactivation Date | The session on which the restriction is expected to lift. |

A 2016-01-04 trigger deactivates 2016-01-06, and a 2025-12-01 trigger deactivates 2025-12-03. That is the trigger day plus the next trading day, then the following session. The file lists that day's triggers. It does not repeat yesterday's names. One symbol once per file in the samples.

A missing object is HTTP 404. That is not stored.

## Not in this load

- Cboe BZX, BYX, EDGA, and EDGX publish circuit breakers as start/end intervals on their website, including a yearly CSV. The grain is an interval, not a daily file, and many of those names are Cboe-listed leveraged ETPs. They are absent until a later load.
- IEX, MEMX, and LTSE are not in these two files.
- Nothing here reconstructs the 10% rule from prices. The published file is the fact.

## Tables

Schema v7 adds two tables and alters none. `kSchemaUserVersion` becomes 7. An empty file still applies `v4.sql`, then `v5.sql`, `v6.sql`, and `v7.sql`. A healthy version-6 file applies `v7.sql` only. A version-6 file missing a ledger table is refused before the stamp moves. Versions 1–3 still get the identity reset message.

`data/market-data.sqlite` is the database. A binary built at version 6 refuses a file stamped 7, so the terminal has to be rebuilt before that file is opened again. Quit the terminal and `ingest` first, and copy the sqlite file plus its `-wal` and `-shm` sidecars.

Rows are keyed by the published symbol, not by `instrument_id`. Most restricted names are not in this store, and a warrant printed as `AAM WS` is not one of our listings. A later study can join a symbol to `instrument_listing` when it wants to.

### `short_sale_restriction_file`

One row per published file that was fetched and parsed. Primary key `(source, file_session)`.

| Column | Meaning |
|---|---|
| `source` | `nasdaq` or `nyse_group` |
| `file_session` | Trade date in the file name, `YYYYMMDD` |
| `file_name` | The remote file name |
| `row_count` | Parsed data rows, including zero |
| `file_created_at` | Nasdaq trailer as Unix seconds UTC. Null for NYSE. |
| `fetched_at` | When this loader wrote the row |

No row means the day was not fetched, or the publisher had no file. It does not mean the day was checked and empty. An empty parsed file does get a row with `row_count = 0`.

### `short_sale_restriction`

One published trigger line. Primary key `(source, file_session, symbol, trigger_ts)`.

| Column | Meaning |
|---|---|
| `symbol` | Trimmed, uppercased, internal spaces kept |
| `security_name` | Trimmed, or null |
| `listing_market` | Nasdaq market category or NYSE primary exchange, trimmed |
| `trigger_session` | New York date of the trigger, `YYYYMMDD` |
| `trigger_ts` | That New York clock time as Unix seconds UTC |
| `deactivate_session` | NYSE anticipated deactivation date. Null for Nasdaq. |
| `ssr_state` | NYSE state code. Null for Nasdaq. |

Replacing a file deletes that source and `file_session`, then inserts the new lines and the file row, in one transaction.

## When a name is restricted

The two files do not mean the same thing, so the read path does not treat them as one list.

- **Nasdaq.** The name is restricted on session D when a `nasdaq` row has `file_session = D`. The next day is not invented from the trigger time. If that file was not stored, session D is uncovered for Nasdaq names.
- **NYSE Group.** A trigger is in force on session D when `trigger_session <= D` and `D < deactivate_session`. The trigger file has to have been stored. Whether D itself had a file does not matter for a name that triggered earlier and has not reached its deactivation date.

A name can have two Nasdaq rows on one session. Both are kept. A reader that wants the set of symbols takes the distinct symbol.

Overlap between the two sources, if a symbol is ever on both, is kept as two rows. It is not merged.

## Loader

`python tools/ssr/load.py [--from DATE] [--to DATE] [--source nasdaq|nyse_group] [--refresh] [--db PATH]`

- Default database is `data/market-data.sqlite`.
- Default range is 2011-02-25 through today for Nasdaq and 2015-01-01 through today for NYSE Group.
- It walks weekdays, not the NYSE holiday calendar, so a calendar miss does not skip a file the publisher actually posted.
- A stored `(source, file_session)` is skipped unless `--refresh`.
- It refuses a `user_version` other than 6 or 7. At 6 it applies `libs/market-data/schema/v7.sql` and stamps 7 before inserting.
- It does not call MBoum.
- NYSE `.xls` is read with `xlrd`. The rest of the tool is the Python standard library.
- Transport errors retry. A 550 or a 404 is a skip. A body with the wrong header is a failure, and that file is not written, so a later run tries it again. A Nasdaq body that says `No data available` is a published session with no names, stored with `row_count` 0.

## Reads

`mdq.py` gains three read-only commands:

- `ssr SYMBOL` prints the stored lines for that published symbol.
- `ssr --session DATE` prints the lines that make a name restricted on that session, under the rule above, plus whether each source has the file coverage that rule needs.
- `ssr-files` prints the ingested file rows.

`agent-data/SCHEMA.md` and the schema section of `docs/market-data-store.md` change with the tables. `mdq` keeps accepting user_version 6 for the older commands. The new commands require 7.

## Tests

- Parser tests on a Nasdaq text fixture, including a quoted name and two trigger times for one symbol, and on an NYSE sheet with both the `SSR State` and `SSR Status` headers. One checked-in `.xls` fixture covers the BIFF reader.
- `market_data_tests` covers empty-to-v7, version 6 to v7 with existing rows kept, a damaged version 6 refused before the stamp, and a version 7 file missing the new tables refused.
- `agent-data/test_mdq.py` builds the throwaway store through `v7.sql` and checks the session rule on a few inserted lines.

## After the files are in

The first backfill is part of finishing this change: run the loader against the local store and record the file counts and the session span in the pull request. The sqlite file stays gitignored. The research rules for a strategy that uses these flags come later, in `research/<slug>/`, and they do not get written until this load is done.
