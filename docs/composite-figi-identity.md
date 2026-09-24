# Composite FIGI as the security key

| Field | Value |
|---|---|
| Status | Implemented 2026-09-23, slices 1–7. The v3 file is kept as data/market-data.v3-backup.sqlite; bars were re-ingested, while financials and option chains are re-fetched in the terminal. Revision 3 of the plan; OpenFIGI behavior checked against the live API on 2026-09-23. See "As built" for where the code differs from the plan text. |
| Date | 2026-09-23 |
| Audience | Implementer of `libs/market-data` |
| Related | `docs/market-data-store.md` is the current store spec. This plan replaces its instrument identity rules. |

A CUSIP cannot be the identity of this store. Indexes such as `$SPX` never receive one, and a reorganization can replace a CUSIP while the security goes on. The OpenFIGI allocation rules keep the composite FIGI in place across a ticker change and across corporate actions. Equities, ETFs, and indexes all receive one. The identifier is an OMG standard and is free to store.

The integer `instrument.id` stays the primary key that bars, coverage, corporate actions, statements, and option rows reference. The composite FIGI is the unique public key. The ticker is a listing with an open time and a close time.

MBoum has no identifier other than the ticker. OpenFIGI decides which security a ticker names, and the store checks that answer again before it appends to a known ticker.

## As built

The code follows this document except for these points:

- **Ingest API.** The ingest functions take an `OpenFigiClient&`, not a bare `const HttpPost&`. The client wraps the `HttpPost` and keeps the rate-limit wait between calls, which a bare callback could not. `apps/common/OpenFigiSession.h` builds it for the CLI and `IngestWorker`: a bearer-less `CurlClient` plus the optional `X-OPENFIGI-APIKEY` header.
- **Notices.** Every ingest result carries `identity_notice`. The CLI prints it, and `IngestWorker` shows it in the status line.
- **HTTP types.** `HttpResponse`, `HttpGet`, and `HttpPost` live in `market_data/Http.h`. `HttpResponse` gained lowercase response `headers`, which `CurlClient` fills for both GET and POST.
- **Store additions.**
  - `applyListingChanges` applies a whole verification run in one transaction.
  - `viewNames` lets the open check require `instrument_current`.
  - `testingInsertInstrument(symbol, asset_class)` derives the FIGI with `testingFigiFor(symbol)` (a valid `ZZG…` value) and sets `verified_at`, so tests that do not exercise identity make no OpenFIGI calls.
- **Blocked renames.** A rename whose new ticker is still open on another instrument leaves the old listing closed as `renamed` with no new listing. Ingesting that ticker afterwards throws `… identity conflict …` rather than a "no longer listed" message.
- **Unresolved outcome.** Reverse and confirmation lookups that fail after a forward mismatch report `Unresolved`, not `Unreachable`. The grace window covers only a forward lookup that learned nothing.
- **Unreadable hits.** A `data` array whose hits carry no FIGI (forward) or no ticker (reverse) is `Unreachable`, never `NoMatch`. Only an empty array or the warning means "not listed", so one malformed element cannot delist a live security.
- **Symbol spelling.** Listing symbols are stored uppercase (`canonicalListingSymbol`; a leading `$` is kept) on insert, relink, and reopen. The chart hotkey accepts one leading `$`, so `$SPX` can be typed on a chart.
- **Tests.** `tests/FakeOpenFigi.h` answers each job from the recorded fixtures, matching on the job's JSON. Its permissive mode confirms any ticker as `testingFigiFor`, which the existing ingest tests use.

## What changed from revision 1

Revision 1 kept the old data and asked OpenFIGI only about tickers the store had never seen. That left three gaps:

- **Recycled tickers.** A known ticker was trusted forever, so a recycled ticker still wrote the new company's bars onto the old row.
- **Renames.** A rename was noticed only when a user typed the new ticker.
- **Backfilled FIGIs.** The backfill attached today's holder of a ticker to history that could belong to an earlier holder.

It also specified a `mergeInstruments` that the v1–v3 foreign keys reject. `statement_cell` and `option_quote` are `ON UPDATE RESTRICT` toward their parents, and `instrument_listing` is `ON DELETE RESTRICT`.

Revision 2 starts from an empty database, so every equity, ETF, and index row gets its FIGI when it is created. Identity is checked in both directions: ticker to FIGI and FIGI to ticker. Merges are no longer needed.

Revision 3 replaces revision 2's guesses about OpenFIGI with observed behavior. The requests and responses are stored under `libs/market-data/tests/fixtures/openfigi/`. Four rules changed:

- **Index queries.** An index is queried as `SPX Index`. A plain `SPX` query returns 60 Spirax listings.
- **Reverse lookup.** It uses `ID_BB_GLOBAL` for every asset class.
- **Delisting is inferred.** A reverse lookup of a delisted FIGI still returns its last ticker, and there is no active flag. Delisting is detected when the ticker no longer maps forward to the same FIGI.
- **`FB` is now a live recycled ticker.** It names the ProShares S&P Dynamic Buffer ETF (`BBG01VRMNFB1`), not Meta.

Decisions recorded with the owner on 2026-09-23:

- Futures and crypto are deferred.
- Verification has a 7-day grace period when OpenFIGI cannot be reached.
- Ingesting a ticker that was renamed and is now unused is refused, with a hint that names the new ticker. If the old ticker now names a different security, as `FB` does, that security is ingested as a new instrument and a notice names the former holder.

## What breaks today

`instrument_symbol_exchange` is unique on `(symbol COLLATE NOCASE, ifnull(exchange, ''))`. `upsertInstrument` and `ensureInstrument` match that pair. A rename inserts a second instrument and leaves the old bars behind. A recycled ticker writes the new company's bars onto the old row. A typo inserts a row: the current file has `APPL` with no bars.

`ensureInstrument` always inserts `AssetClass::Equity`, and `ensureOptionInstrument` only recognizes a leading `$`. The current file stores `QQQ` as `equity`.

`exchange` is never set by ingest. It exists only to separate `NMS` from `XNAS` rows in tests, and it is the only reason `findInstrumentsBySymbol` can return more than one row.

## Fresh start

The existing `data/market-data.sqlite` is discarded. Schema v4 is a new baseline, not a migration.

- A file with `user_version` 0 runs `v4.sql` and is stamped 4.
- A file with `user_version` 1, 2, or 3 is refused with this message and is not modified:
  `market-data.sqlite is schema v<N>. v4 changed instrument identity and does not migrate. Close the terminal, delete <path> and its -wal and -shm files, and re-ingest.`
- `user_version` above 4 still throws.

`v1.sql`, `v2.sql`, `v3.sql`, their `schema_v*.inc.in` files, `schemaV1()` through `schemaV3()`, and `testingCreateSchemaV1`/`V2` are deleted. Git keeps the history. `v4.sql` holds the full DDL. From here on it is frozen the way v1 was, and v5 migrates forward from it.

Reset procedure, after the code lands:

1. Close `terminal.exe`.
2. Delete `data/market-data.sqlite`, `data/market-data.sqlite-wal`, and `data/market-data.sqlite-shm`.
3. Optionally add the `openfigi` key to `secrets.json`.
4. Re-ingest `QQQ META DDOG ADBE AXON AMZN JPM` with the same timeframes and ranges as before. Re-fetch financials for `AXON` and `META` and option chains for `JPM` and `META` in the terminal. `APPL` is refused, because it does not map.

`data/chartbooks/chartbook1.chartbook.json` keeps working. It names symbols and has no `figi` key, so it resolves again after re-ingest.

## Identity rules

One instrument is one security in this store: one US equity or ETF line, or one index. That matches the composite FIGI (`compositeFIGI` in the mapping response), which is one security in one country. The per-venue `figi` is not stored for equities. The share-class FIGI groups listings across countries, which this store does not combine, and it is often missing on indexes.

- **FIGI required.** Equity, ETF, and index rows must have a FIGI. The schema enforces it. No FIGI means no row.
- **Exception for futures, crypto, and `other`.** These rows may have a NULL FIGI. Futures and crypto are deferred, so ingest never creates them in this change.
- **Two share classes stay two instruments.** `BRK.A` and `BRK.B` have different composite FIGIs.
- **A rename keeps the id.** The old listing closes (`renamed`) and the new one opens on the same id. Bars stay.
- **A recycled ticker gets a new id.** When a ticker maps to a FIGI other than the one stored, the old listing closes and the ticker opens on a new row. Old bars are never appended to.
- **A merged or delisted security keeps its id and FIGI.** Its listing closes (`delisted`) and `delisted_at` is set. Bars are never moved between instruments.
- **Option contracts stay keyed by `option_quote.vendor_symbol`.** The underlying equity or index carries the FIGI.
- **Listing times record when the store saw a change, not market dates.** `opened_at` is when this store first bound the ticker to the id, and `closed_at` is when it learned otherwise. Do not use them for point-in-time symbol lookups.

OpenFIGI stops returning the old ticker for the renamed security. A store that ingested Facebook as `FB` before 2022 would have a row whose FIGI is `BBG000MM2P62`. `FB` now maps forward to `BBG01VRMNFB1`, the ProShares ETF, and a reverse lookup of `BBG000MM2P62` returns `META`. Verification relinks that row to `META`, and a later ingest of `FB` creates a separate ETF instrument. The local listing history is what lets a chart that never pinned a FIGI still resolve `FB` to Meta until the ETF is ingested.

## Schema v4

New `libs/market-data/schema/v4.sql`, embedded like v3 (`schema_v4.inc.in`, `CMakeLists.txt`, `schemaV4()`). `kSchemaUserVersion` becomes 4.

`bar`, `corporate_action`, `coverage_day`, `statement_snapshot`, `statement_cell`, `option_underlying`, `option_expiry`, and `option_quote` are copied verbatim from v1–v3, including their indexes and header comments. `instrument` changes, and `instrument_listing` and `instrument_current` are new:

```sql
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
```

Notes:

- **Changes to `instrument`.** It no longer has `symbol` or `exchange`. The ticker lives only in `instrument_listing`, so there is no second copy to keep in sync. `instrument_current` returns each instrument with its open listing, or its most recent closed one. Every instrument is created with a listing in the same transaction, so the join never drops a row that `Store` wrote.
- **Open listings are unique by symbol alone.** `NMS` versus `XNAS` duplicates cannot form, because the composite FIGI is country-level.
- **Listings use an `INTEGER PRIMARY KEY`.** Two listing changes in the same second do not collide.
- **The `figi` CHECK is the full FIGI shape.** Positions 1–2 are consonants and not one of the reserved prefixes, position 3 is `G`, positions 4–11 are consonants or digits, and position 12 is a digit. The check digit is not computed in SQL. `isValidFigi` in `libs/market-data/src/market_data/Figi.h` and `Figi.cpp` computes it, and every write path calls it. Reads return a stored value as-is, so a hand-edited file still opens.
- **Check digit algorithm.** Map letters to numbers (`A`=10 … `Z`=35), double the value at every even (1-based) position among the first 11 characters, sum the decimal digits of all values, and take `(10 - sum % 10) % 10`. Verified on `BBG000BLNNH6` (IBM), `BBG000B9XRY4` (AAPL), `BBG000MM2P62` (META), and `BBG000BLNNV0` (IBM venue FIGI), and on all 182 distinct FIGIs in the OpenFIGI fixtures.
- **`verified_at`** is the last time OpenFIGI confirmed that the open listing's ticker names this FIGI. A detected conflict sets it to NULL.

The DDL above was applied to SQLite 3.50.4 with foreign keys on. It accepts the four FIGIs above. It rejects a vowel, a bad prefix, a missing `G`, a lowercase value, an 11-character value, and a NULL FIGI on an equity. It rejects a second open listing for `fb` while `FB` is open, but accepts `FB` on a new id once the old listing closes. The symbol lookup uses `instrument_listing_symbol`.

## Store

`Instrument` loses `exchange` and gains `std::optional<std::string> figi`, `std::optional<UnixSeconds> verified_at`, and `bool listing_open`. `symbol` comes from `instrument_current`. Every instrument read selects from that view.

Removed: `upsertInstrument`, `findInstrument(symbol, exchange)`, `findInstrumentsBySymbol`, and `coerceExchange`.

New:

| Method | Behavior |
|---|---|
| `insertInstrument(const NewInstrument&, symbol, now)` | Inserts the row and an open listing in one transaction. Throws on an invalid FIGI, a FIGI already stored, or a symbol already open. |
| `findInstrumentById(id)` | Unchanged apart from the view. |
| `findInstrumentByFigi(figi)` | Exact match. |
| `findOpenListing(symbol)` | The instrument whose listing for `symbol` is open, or `nullopt`. Ingest uses this. |
| `resolveSymbol(symbol)` | The open listing if there is one. Otherwise the instrument whose `symbol` listing closed most recently. Otherwise `nullopt`. Charts and panels use this. |
| `listingHistory(id)` | All listings for an instrument, oldest first. |
| `openListing(id, symbol, now)` | Requires no open listing on `id` and none on `symbol`. |
| `closeListing(id, now, reason)` | Closes the open listing. `delisted` also sets `delisted_at` when it is NULL. |
| `relinkSymbol(id, symbol, now)` | `closeListing(id, now, renamed)` then `openListing`, in one transaction. |
| `attachFigi(id, figi)` | Only for a NULL-FIGI row (future, crypto, or other). The same value is a no-op. A different value, or one stored on another id, throws. |
| `markVerified(id, now)` / `clearVerified(id)` | Set or clear `verified_at`. |
| `updateDescriptive(id, name, asset_class, currency, timezone)` | A timezone change after bars exist still throws. |

`testingInsertInstrument(symbol, asset_class = equity)` (FIGI from `testingFigiFor(symbol)`, verified now) and `insertInstrument` replace the `upsertInstrument` calls in the fact-table tests (`coverage_tests.h`, `statement_tests.h`, `option_tests.h`, `ingest_tests.h`, `chart_load_tests.h`, and `chart_transform_tests.h`). Each fixture uses a FIGI that passes `isValidFigi`.

## OpenFIGI client

### Transport

`POST https://api.openfigi.com/v3/mapping` with a JSON array of jobs and `Content-Type: application/json`. `CurlClient` gains `post(url, body, headers)`, which does not send the MBoum bearer. market-data declares `using HttpPost = std::function<HttpResponse(std::string_view url, std::string_view body)>` beside `HttpGet` in `market_data/Http.h`. The apps build the callback and add the key header (`apps/common/OpenFigiSession.h`).

The optional key is `secrets.json` key `openfigi`, sent as `X-OPENFIGI-APIKEY`. `loadOptionalSecret(path, "openfigi")` returns `nullopt` when the key is absent. The key is never logged, never placed in a URL, and never written to SQLite.

### Limits and batching

| | Requests | Jobs per request |
|---|---|---|
| No key | 25 per 60 s | 10 |
| With key | 25 per 6 s | 100 |

Every response carries `ratelimit-policy` (for example `25;w=60`), `ratelimit-limit`, `ratelimit-remaining`, and `ratelimit-reset` (seconds until the window resets). `OpenFigiClient`:

- owns the batch size;
- waits `ratelimit-reset` seconds before sending when `ratelimit-remaining` reaches 0;
- on a 429, waits `ratelimit-reset` seconds, or one full window when the header is missing, and retries up to 3 times. After that, every job in the batch is `unreachable`.

A 413 means the batch was too large. That is a programming error, and it throws. Any other non-200 response, a body that is not a JSON array, or an array whose length does not equal the job count makes the whole batch `unreachable`.

### Response parsing

Each array element lines up with one job:

- `{"data": [...]}` → `hits`.
- `{"warning": "No identifier found."}` → `no_match`. This is an answer, not a failure. An unknown or malformed FIGI sent as `ID_BB_GLOBAL` also gets this warning, not an error.
- `{"error": "..."}` → `unreachable` for that job, with the message kept for the report. The observed error is a job the server rejects, such as `securityType2 required with BASE_TICKER(idType).`

Each hit keeps `figi`, `compositeFIGI`, `ticker`, `name`, `exchCode`, `marketSector`, and `securityType`. The response does not say whether a security is active. There is no status field.

### Symbol translation

`toOpenFigiTicker(symbol)` and `fromOpenFigiTicker(ticker)` live in `OpenFigi.cpp`:

- **Indexes.** One leading `$` is stripped and ` Index` is appended: `$SPX` → `SPX Index`. Coming back, a ticker ending in ` Index` loses the suffix and gains `$`. The stored listing symbol stays `$SPX`.
- **Share classes.** The store and MBoum both use `.` (`BRK.B`, which matches `chart_command_tests.h`). OpenFIGI answers only to `/`: `BRK/B` maps, while `BRK.B` and `BRK-B` return `no_match`. Only OpenFIGI jobs translate the separator.
- **Case.** The comparison ignores case.

### Jobs

| Purpose | Asset class | Job |
|---|---|---|
| Forward (ticker → FIGI) | equity, ETF | `{"idType":"TICKER","idValue":T,"exchCode":"US","marketSecDes":"Equity"}` |
| Forward | index (`$` prefix) | `{"idType":"TICKER","idValue":"<T> Index","marketSecDes":"Index"}` |
| Reverse (FIGI → ticker) | all | `{"idType":"ID_BB_GLOBAL","idValue":F}` |

What each job shape returned:

- **Forward, equity or ETF.** Returns only active securities: `TWTR` and `ATVI` return `no_match`. `includeUnlistedEquities` is never sent. With it, `TWTR` returns the delisted `BBG000H6HNW3`, and that would hide the signal verification relies on.
- **Forward, index.** `SPX Index` returns one hit, `BBG000H4FSM0`, with a null `compositeFIGI` and `exchCode`, `marketSector` `Index`, and `securityType` `Equity Index`. `VIX Index` and `NDX Index` behave the same way. Plain `SPX` returns 60 unrelated Spirax equity listings, and `SPX` with `marketSecDes` `Index` returns `no_match`.
- **Reverse.** `ID_BB_GLOBAL` with a composite FIGI returns exactly the composite row (`BBG000MM2P62` → one `META` hit on `US`). With an index FIGI it returns the index row. `COMPOSITE_ID_BB_GLOBAL` without `exchCode` returns all 21 venue listings, so it is not used.
- **Delisted securities.** A reverse lookup of a delisted FIGI returns its last ticker: `BBG000H6HNW3` → `TWTR`. OpenFIGI never returns `no_match` for a FIGI that once existed.

Reducing hits:

- **Forward, equity or ETF.** Collect the distinct non-null `compositeFIGI` values. Exactly one means `confirmed(F)`; more than one means `ambiguous`.
- **Forward, index.** Use `compositeFIGI` when present, otherwise `figi`, and apply the same rule.
- **Reverse.** Collect the distinct `ticker` values after `fromOpenFigiTicker`. Exactly one means `ticker(T)`; more than one means `ambiguous`.

Asset class comes from the confirming hit:

| `marketSector` | `securityType` | Asset class | Observed |
|---|---|---|---|
| `Index` | `Equity Index` | `index` | SPX, VIX, NDX |
| `Equity` | `ETP` | `etf` | QQQ, FB (ProShares) |
| `Equity` | anything else | `equity` | AAPL, META, BRK/B (`Common Stock`) |
| anything else | | refused as `unsupported` | |

`name` comes from the hit. Every FIGI returned passes `isValidFigi`, or the result is `unreachable`. All 182 distinct FIGIs in the fixtures pass the check digit.

### Fixtures

Captured from the live API without a key on 2026-09-23, and stored under `libs/market-data/tests/fixtures/openfigi/`. Each `*.request.json` is the exact body that was posted, and `*.response.json` is the body that came back.

| Fixture | Contents |
|---|---|
| `forward_equity` | `AAPL` (Common Stock), `QQQ` (ETP), `BRK/B`, `META`, `FB` (recycled to an ETP), and `no_match` for `APPL`, `SPX` as an index, `BRK.B`, and `BRK-B`. |
| `index_shapes` | Plain `SPX` (60 Spirax hits), `SPX` with `securityType`, a `BASE_TICKER` error element, `SPX Index`, `VIX` and `NDX` without the suffix (`no_match`), `VENDOR_INDEX_CODE` (3 hits), and `INDU`. |
| `reverse` | `VIX Index`, `NDX Index`, `SPX Index`; META by `COMPOSITE_ID_BB_GLOBAL` with and without `exchCode`, and by `ID_BB_GLOBAL`; the SPX FIGI by `ID_BB_GLOBAL`; delisted ISINs filtered to `US` (`no_match`); and a malformed FIGI (`no_match`). |
| `delisted_forward` | Twitter and Activision by ISIN and by CUSIP (foreign listings only), `TWTR` and `ATVI` as active tickers (`no_match`), and `TWTR` with `includeUnlistedEquities`. |
| `delisted_reverse` | Twitter's composite FIGI by `ID_BB_GLOBAL` with and without `includeUnlistedEquities`, `ATVI` with `includeUnlistedEquities`, `FB` with `includeUnlistedEquities` (still only the ETF), `SPX Index`, and the ProShares FIGI. |
| `ratelimit.headers.txt` | The rate-limit headers from a keyless response. |

The tests read only these files and never call the network. To add a case, capture it the same way and commit the request and response together.

## Identity resolution

`Identity.h` and `Identity.cpp` hold `ensureInstrument`, `ensureOptionInstrument`, and `verifyIdentities`. `ingestSymbol`, `ingestDailySymbol`, `ingestSplits`, `ingestStatement`, and `ingestOptions` gain an `OpenFigiClient&` and return the call's `identity_notice`. Identity is resolved before the MBoum request, except for options: there the canonical base symbol comes from the payload, so identity is resolved after the GET but before any write.

Constants in `Identity.h`:

- `kVerifyMaxAge` = 24 h. A verification younger than this is trusted without a network call.
- `kVerifyGrace` = 7 days. When OpenFIGI cannot be reached, a known ticker verified within this window keeps ingesting and prints a warning.

### `ensureInstrument(store, post, symbol, now)`

1. **Open listing on `symbol`:**
   1. If the instrument's FIGI is NULL, return its id.
   2. If `verified_at` is younger than `kVerifyMaxAge`, return its id. No OpenFIGI call.
   3. Otherwise run `verifyIdentities` on that id:
      - `ok`: return the id.
      - `renamed` or `delisted`: the listing is now closed, so go to step 2.
      - `unreachable`: return the id with a warning if `verified_at` is within `kVerifyGrace`. Otherwise throw `cannot confirm SYMBOL: OpenFIGI unreachable, last verified <date>`.
      - `unresolved`: throw. OpenFIGI already said the ticker no longer names the stored FIGI, so the grace window does not apply and no bars are appended.
      - `conflict`: throw with the conflict text.
2. **No open listing: map `symbol` forward.**
   - `confirmed(F)` and F is stored on instrument X:
     - If X has an open listing under another ticker, call `relinkSymbol(X, symbol)`. A rename was found from the new ticker's side.
     - If X has no open listing, call `openListing(X, symbol)`. The security is trading again.
     - Return X.
   - `confirmed(F)` and F is new: `insertInstrument` with F, the asset class, and the name from the hit, and mark it verified. If `symbol` has a closed `renamed` listing on another instrument, the result carries a notice for the log or status line: `FB now names PROSHARES S&P DYNAMIC BUFFER; the former FB trades as META`. The insert goes ahead because the ticker is valid today, and refusing it would leave the new security impossible to ingest.
   - `no_match`: throw, and write nothing.
     - If `symbol` has a closed `renamed` listing, the message names the instrument's current ticker: `XYZ is no longer listed; that security now trades as ABC`.
     - If `symbol` has a closed `delisted` listing: `XYZ is delisted`.
     - Otherwise: `XYZ has no US listing in OpenFIGI`.
   - `ambiguous`, `unsupported`, or `unreachable`: throw. Nothing is written.

`ensureOptionInstrument` is the same function with the same rules. The old copy is deleted.

A rename never makes ingest silently follow the old ticker to the new one. The MBoum request was for the old ticker, so the user re-runs with the new one.

### `verifyIdentities(store, post, now, ids)`

This is batched. It is the only code that closes a listing on its own. It works from one observed fact: a forward lookup returns only active securities, and a reverse lookup returns the last ticker even after delisting.

1. **Forward pass.** For every id that has a FIGI and an open listing S, map S forward.
   - `confirmed(F)` equal to the stored FIGI → `ok`.
   - `unreachable` → `unreachable`, no change.
   - Anything else (`no_match`, a different F′, or `ambiguous`) → reverse pass.
2. **Reverse pass.** Map the stored FIGI F.
   - `ticker(T)` with T equal to S → `delisted`. F still names S as its last ticker, but S no longer maps forward to F: either nothing is listed under S, or another security took it.
   - `ticker(T)` with T ≠ S → confirmation pass.
   - `no_match` or `ambiguous` → `conflict`. A FIGI this store holds should never vanish.
   - `unreachable` → `unresolved`, no change. The forward pass already showed that S no longer names F, so the grace window does not apply.
3. **Confirmation pass.** Map each T forward.
   - `confirmed(F)` → `renamed` to T.
   - Anything else except `unreachable` → `delisted`: the security was renamed and then delisted.
   - `unreachable` → `unresolved`, no change.
4. **Apply.** One write transaction:
   - Close every `delisted` listing with reason `delisted`, and every `renamed` listing with reason `renamed`.
   - Then open each rename's new ticker.
   - If that ticker is still open on another instrument, the id becomes `conflict`: its old listing stays closed and no new listing opens.
   - Set `verified_at = now` on every conclusive outcome and set it to NULL on every `conflict`.

Closing before opening lets two stored instruments swap tickers in one run. A closed listing can reopen later: typing the ticker again goes through step 2 of `ensureInstrument`.

The passes cost one job per due instrument in the common case and at most three when something changed. With a key, 100 instruments take one request per pass.

Instruments with a NULL FIGI are skipped.

### CLI

- `ingest SYMBOL...` resolves identity as above, then ingests.
- `ingest --verify` verifies every instrument whose `verified_at` is NULL or older than `kVerifyMaxAge`. `--verify --all` ignores the age. It prints one line per instrument: `ok`, `renamed FB -> META`, `delisted`, `conflict: …`, or `unreachable`.
- `ingest --delist SYMBOL` closes an open listing with reason `manual`. It is the manual repair for a conflict.

Revision 1's `--figi` and `--unconfirmed` flags are dropped. There is no data to backfill, and an equity without a FIGI can no longer be stored.

### Terminal

`IngestWorker` builds an `OpenFigiSession` (a bearer-less `CurlClient` carrying the optional `openfigi` key). When the GO path refuses a symbol, the refusal text goes to the status line. A warning from the grace window is shown in the status line, and the ingest still runs.

## Screens and chartbooks

The GO box and MBoum requests stay tickers.

**Symbol lookup:**

- `resolveSymbol` returns at most one instrument, so `ChartLoadStatus::AmbiguousSymbol` is removed along with its cases in `CChartPane.cpp` and `StatusRail.cpp`.
- The `found.size() > 1` branches in `CChartLoad.cpp`, `FinancialsPanel.cpp`, and `OptionsChainPanel.cpp` go away.
- The options panel keeps its retry with a `$` prefix.

**Chartbooks:**

- Chart settings, financials panels, and options panels gain an optional `figi` string.
- Chartbook format stays 1. A missing key stays valid, and writers emit `figi` only when it is set.
- Load order in `loadChartBars` and in the financials and options panels:
  1. A stored FIGI uses `findInstrumentByFigi`. A miss is the unknown-symbol status, and the message includes the FIGI.
  2. Otherwise `resolveSymbol`. A miss keeps today's unknown-symbol status.
- When a symbol resolves to an instrument with a FIGI, the in-memory settings take the FIGI and the instrument's current symbol. The file picks both up on the next save. A saved book then opens the same bars after a rename, and after someone else takes the old ticker.

**Inventory:**

- The `Exchange` column becomes `FIGI`.
- The detail heading shows the FIGI and the closed listings (`formerly FB until 2026-09-23`).
- The selectable row label stays the symbol.

## Security and privacy

Every new ticker and every verification sends a ticker or FIGI to OpenFIGI (Bloomberg). That reveals which securities this user researches, the same way the MBoum requests already do. The payload contains nothing else. The key handling is described under Transport.

## Docs

Update `docs/market-data-store.md`:

- **Instrument SQL section.** Replace it with the listing model: the integer id joins fact rows, the composite FIGI is required for equities, ETFs, and indexes, and the live ticker is the open listing.
- **Exchange coercion.** Remove the coercion rule and Alternative 8 (the expression unique index).
- **Schema file.** Point the schema section at `v4.sql` as the baseline.

In `docs/gen_architecture_diagram.py`:

- The instrument box label becomes: integer primary key, composite FIGI, open symbol listing.
- The store caption moves from `kSchemaUserVersion = 3` to 4.
- The figure gains an OpenFIGI box next to MBoum, feeding ingest.
- Regenerate `docs/architecture.svg` and `docs/architecture.png` when pycairo imports.

## Tests

**`schema_tests.h`:**

- An empty file stamps 4 and lists `instrument_listing` and the `instrument_current` view.
- The embedded `v4.sql` matches the file after stripping CR.
- A file stamped 1, 2, or 3 throws the reset message and keeps its `user_version`.
- `user_version` 99 still throws.
- A file stamped 4 that is missing `instrument_listing` throws.

**`figi_tests.h`:**

- The check digit is pinned to `BBG000BLNNH6`, `BBG000B9XRY4`, `BBG000MM2P62`, `BBG000BLNNV0`, and `BBG000H4FSM0` (SPX), and it is also run over every FIGI in the fixtures.
- Each of these is rejected: a bad check digit, a vowel, a reserved prefix, a missing `G`, lowercase, and 11 characters.

**`store_tests.h`:**

- `insertInstrument` rejects a second row with the same FIGI and a second open listing for the same symbol, with case ignored.
- An equity with a NULL FIGI is rejected by SQL. A crypto row with a NULL FIGI inserts.
- `relinkSymbol` from `FB` to `META` keeps the id:
  - Bars written before the relink read back on that id.
  - `resolveSymbol("FB")` returns it through the closed listing.
  - `findOpenListing("FB")` is empty.
  - `listingHistory` shows `FB` closed as `renamed`, then `META` open.
- After `closeListing(delisted)`, a new instrument opens `FB`:
  - `resolveSymbol("FB")` returns only the new row.
  - The retired row is still reachable by id and by FIGI, and its `delisted_at` is set.
- Two relinks in the same second both succeed.
- `attachFigi` succeeds only on a NULL-FIGI row.
- A timezone change after bars exist still throws.

**`openfigi_tests.h`** (fixtures only):

- **Response shapes.** Each fixture element parses to the expected `hits`, `no_match`, or `unreachable` result. The `BASE_TICKER` element in `index_shapes` is `unreachable` and keeps its message.
- **Batch errors.** A wrong array length and a non-array body are `unreachable`. A 413 throws.
- **Asset classes.** `QQQ` and `FB` → `etf`; `AAPL`, `META`, and `BRK/B` → `equity`; `SPX Index` → `index`.
- **Index reduction.** An index hit with a null `compositeFIGI` reduces to its `figi` (`BBG000H4FSM0`).
- **Ambiguity.** The 60-hit plain `SPX` body reduces to `ambiguous`. The client never builds that job, and the test pins the reason it doesn't.
- **Ticker translation.** `BRK.B` ↔ `BRK/B` and `$SPX` ↔ `SPX Index` round-trip. The job builder never emits `includeUnlistedEquities`.
- **Rate limits.** The batch size is 10 without a key and 100 with one. `ratelimit.headers.txt` parses to a limit of 25, a 60-second window, and a reset of 60.

**`identity_tests.h`** (fake `HttpPost` serving the fixtures; now is fixed):

- **New ticker.** `AAPL` inserts `BBG000B9XRY4` as `equity` with name `APPLE INC`, and marks it verified. `QQQ` inserts as `etf`, and `$SPX` inserts `BBG000H4FSM0` as `index`.
- **Refused tickers.** `APPL`, `TWTR`, a stale index form, an `unreachable` element, and a 429 that survives its retries each leave the database byte-identical.
- **Fresh listing.** An open listing verified within 24 h makes no POST.
- **Stale listing, still valid.** A stale open listing whose forward FIGI matches makes one POST and is `ok`.
- **Real rename with a recycled ticker.**
  - Setup: store `BBG000MM2P62` with an open `FB` listing verified 2 days ago.
  - Forward `FB` returns the ProShares FIGI, reverse returns `META`, and forward `META` confirms, so the row is relinked to `META` with its bars.
  - Ingesting `FB` then inserts the ETF as a new id and carries the `the former FB trades as META` notice.
- **Rename with a now-unused ticker.** The same setup, but forward on the old ticker returns `no_match`. Ingesting the old ticker throws the `now trades as META` message and writes nothing.
- **Real delisting.**
  - Setup: store `BBG000H6HNW3` with an open `TWTR` listing.
  - Forward `TWTR` returns `no_match` and reverse returns `TWTR`, so the listing closes as `delisted` and `delisted_at` is set.
  - Ingesting `TWTR` throws `TWTR is delisted`.
- **Renamed, then delisted.** Reverse returns T, and forward T returns `no_match`: the listing closes as `delisted`, and T is never opened.
- **Vanished FIGI.** Reverse `no_match` for a stored FIGI is a `conflict` that clears `verified_at`, and the next ingest throws.
- **Grace window.** With a stale listing and OpenFIGI unreachable, ingest is accepted with a warning at 6 days since verification and refused at 8.
- **Ticker swap.** Two instruments that swap tickers resolve in one `verifyIdentities` run.
- **Blocked rename.** A rename target that is still open on another instrument after the closes is a `conflict`.

**Chartbook and chart-load tests:**

- A book with no `figi` key loads, and a book with `figi` round-trips.
- A load pinned to a FIGI ignores a recycled ticker.
- A load pinned to a FIGI after a rename shows the new symbol.

Run `market_data_tests` and `terminal_tests`. A running `terminal.exe` blocks the relink; close it first.

## Open questions

Resolved on 2026-09-23 against the live OpenFIGI API (see Fixtures):

- **Delisted securities.** A reverse lookup returns the last ticker, not `no_match`. Delisting is inferred from the forward lookup.
- **Composite FIGI sent as `ID_BB_GLOBAL`.** It returns exactly the composite row. One reverse job shape serves every asset class.
- **Index queries.** They need the `SPX Index` form, and the result has no `compositeFIGI`.
- **OpenFIGI share-class separator.** It is `/`.
- **Futures and crypto.** Deferred by decision.

Resolved against MBoum on 2026-09-23 (`GET /v3/markets/historical?interval=daily`):

- **MBoum share-class form.** MBoum uses `.`: `BRK.B` returns bars, while `BRK-B` and `BRK/B` return HTTP 404 `No historical data found`. The store and MBoum therefore share one spelling, and only OpenFIGI jobs translate the separator.
- **Recycled `FB` on MBoum.** MBoum's `FB` is the ProShares ETF as well: a close near 45.7 and a few hundred shares a day. Both vendors agree, so a stale Meta row must never append `FB` bars.
- **Ticker encoding.** `$SPX` and `%24SPX` both return bars. `mboumV3DailyUrl` appends the ticker unencoded while `mboumV3OptionsUrl` encodes it. Make both encode in slice 3 for consistency; this is not a live bug.

No open questions remain.

## Out of scope

- Migrating v1–v3 data. The file is recreated.
- `mergeInstruments`. A FIGI is required at insert and unique, so duplicate rows for one security cannot form.
- Point-in-time ticker lookup by market date. Listing times record when this store observed a change.
- Portfolio tables. They are not in the tree. A later holding can store a FIGI and still must not foreign-key to `instrument`, so a never-ingested name can be written down.
- CUSIP, ISIN, and CIK columns.
- Per-venue FIGIs and share-class FIGIs.
- A FIGI on each option contract.
- Network calls from `Store`'s constructor or from schema setup.
- Changing the GO box from a ticker to a FIGI.

## Work slices

1. **Schema baseline.** Write `v4.sql` and embed it. Delete v1–v3 and their embeds. Refuse stamps 1–3, bump the version constant, add `Figi`, and write the schema and FIGI tests.
2. **Store identity API.** Replace the instrument methods with the listing API and the `instrument_current` reads. Remove `exchange`. Add `testingInsertInstrument` and move the existing tests onto it. Write the store tests.
3. **OpenFIGI client.** The fixtures are already in the tree. Add `CurlClient::post`, the optional `openfigi` secret, jobs, parsing, ticker translation, classification, the rate limiter, and ticker encoding in both MBoum URL builders, with parser tests.
4. **Identity resolution.** `ensureInstrument`, `verifyIdentities`, the new `OpenFigiClient&` parameter on the ingest functions, `--verify` and `--delist`, and `IngestWorker`. Identity tests with a fake POST.
5. **Readers.** `resolveSymbol` in the chart, financials, and options paths, removal of the ambiguous status, the chartbook `figi` field, and the inventory FIGI column and history. Chartbook and chart-load tests.
6. **Docs and diagram.** Update `market-data-store.md` and the architecture label, then regenerate the figure if pycairo imports.
7. **Reset.** Follow the Fresh start procedure and re-ingest.
