# SSR next-session long: pre-registered rules

Written 2026-10-14, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `research/ssr-next-day-long/research/counts.py` and `health_audit.py`. No price was read for a return, a chart, or a hit rate. Counts, from the current symbol and not from `instrument_listing.opened_at`: 285 instruments, 283 with at least one daily bar, 283 listing rows, 1,476,883 short-sale rows scanned, 11,602 of those rows on a stored symbol, 269 stored symbols with at least one row. NYSE sessions from 2011-02-25 through 2026-10-14: 3,935. Episode starts 5,349. Tradeable episodes (next session has a daily bar) 4,276, of which 1,365 have an entry on or after 2024-07-01 and 2,911 are earlier. Distinct entry sessions 1,714, of which 430 are on or after 2024-07-01. Sessions with two or more names: 893. Most names on one entry session: 80. Most episodes for one symbol: 130. Sources: 2,986 Nasdaq and 1,290 NYSE Group (out of sample 1,144 and 221). Asset class of tradeable episodes: 4,030 equity, 246 ETF. Symbols: 258. All 4,276 tradeable episodes also have a bar on the signal session. 4,265 have bars on the entry session and the next four sessions. First tradeable entry 2015-05-11, last 2026-09-29. SPY daily bars run 2011-01-04 through 2026-10-01 (3,959 sessions). Session 2026-10-14 is an NYSE session and has no SPY daily bar. `corporate_action` has 0 dividend rows and 107 split rows. Filtering episodes by `instrument_listing.opened_at` left 26 restricted sessions and 0 tradeable entries, because those stamps run from unix 1790217592 to 1791038816 and are when this store bound the ticker. The 4,276 count includes VIXY and UVXY. This rule drops both. Each symbol has at most 130 tradeable episodes, so dropping two symbols removes at most 260 of the 1,365 out-of-sample episodes.
- **Health, same pass:** `health_audit.py` ran `DataHealthAuditor.audit_universe` on the 258 symbols. Result: not passed. 89 critical `unadjusted_jump` rows on 34 symbols, and 32 critical `zero_dividend_drag` rows. Warnings: 0. No extended-hours contamination flag. The jump list is kept in `health.json`. The largest jump counts are PRPO 10, UVXY 10, GYRE 7, KRRO 7, BNED 5, OPI 5. A jump of 50% or more is what a crash looks like, and it is also what a missing split looks like. This study does not drop a name for that flag. VIXY and UVXY are dropped because `vixy-variance-premium` already showed the stored series is missing reverse splits (VIXY on 2013-06-10 and 2016-07-25, UVXY on 2012-03-08 and 2013-06-10, plus later UVXY jumps). The zero-dividend flags fire because the yield check was pointed at the ETFs in the snapshot and the dividend table is empty, not because those ETFs were inspected one by one after a return.
- **Earlier studies on the same instruments, periods, or mechanism:** No earlier study used the short-sale files. The same stored names and the same out-of-sample window were used by other rules. `spy-rsi2-dip-buy` is a paper-trading candidate: buy SPY on a low RSI(2), out of sample from 2024-07-01 through 2026-09-25, Sharpe 1.29, and the timing-placebo p-value is 0.046 and seed-sensitive. `qqq-holdings-ma-bounce` and `qqq-holdings-relative-strength` passed their own six lines (out-of-sample Sharpe 1.017 and 1.516). `research/README.md` later marks both Diagnostic (Look-Ahead Universe). Their report headers still say paper-trading candidate. This file follows the index. `qqq-atr-band-dip-eod` was rejected: buying a one-ATR dip in QQQ lost money out of sample (Sharpe −0.40) while QQQ buy-and-hold over that window was about +55%. `qqq-intraday-trend` is a paper-trading candidate and its out-of-sample Sharpe fell from 0.82 to 0.45 when April 2025 was removed. `small-cap-gap-up-fade` and `finviz-gap-up-fade` are short sales of opening gaps on screens that overlap this snapshot, including PRPO. The index marks them rejected. `low-liq-high-vol-mean-reversion` was rejected. None of those rules is the rule below.
- **What I already know about the test windows:** Out of sample starts 2024-07-01. That window has been read in the studies above. SPY and QQQ rose over it, and it contains the April 2025 decline and rebound. It is not a blind holdout for the market. It has not been used to score this SSR rule. In sample ends 2024-06-28.
- **Where the parameters came from:** The user's question, asked before this file: is there a long after a name was recently short-sale restricted, and what is the average performance. Hold of 1 session is that question. The grid 1, 2, 3, 5 is the decay check around it. The cost of 10 bp per side is a round prior for a name on the day after a 10% decline, with no quote in the store. It is wider than the 1 bp QQQ/SPY default and narrower than the 20 bp used on the small-cap gap books. The out-of-sample date is the house date already used on this store.

## Hypothesis

The next NYSE session after a stored name's short-sale restriction begins has a positive open-to-close return, large enough that an equal-weight long book of those names clears the acceptance table after 10 bp per side.

**Mechanism.** SEC Rule 201 (17 CFR § 242.201) makes the listing market flag a covered security after a decline of 10% or more from the prior regular-hours close. For the rest of that day and the next day a national best bid is disseminated, a short sale generally cannot execute at or below the national best bid. Barardehi, Bird, Karolyi, and Ruchti (Office of Financial Research, 2023) compare names that cross −10% with names that stop just short of it and estimate that the restriction raises the trigger day's return by about 35 bp, with no reversal after the restriction lifts. This study does not replicate that comparison. It does not have the intraday path of names that fell 9% and were not flagged. The flag is the operational definition of the episode. A positive result is a long after a flagged 10% decline. It is not, by itself, evidence that the uptick constraint caused the drift. Switzer, Duangnate, and Torsu (2019) report that the same circuit breaker does not reliably stop the intraday decline. The other side of the trade, if the OFR result is the mechanism, is a short seller who cannot hit the bid and a buyer who is paid to provide the share.

**Known counter-forces.** The news that produced the decline can continue the next morning. The entry day is often still a restricted day, so a short squeeze is not required for the position to be open. The book can only price names already in this store. Those names are a static snapshot of past research universes, not the names that were restricted on the day.

## Predictions beyond P&L

If the mechanism is right, then:

1. The average open-to-close of a traded name on its entry session is higher than the average open-to-close of those same names on other sessions in the book.
2. A larger decline on the signal session, measured by that session's close over the previous session's close and known before the entry open, lines up with a larger average net trade. Scored by full-sample equal-count quintiles of that prior return. Consistent if quintile 1 (the lowest prior return) has a higher mean net trade than quintile 5. The quintile is not an input to the trade.
3. The full-sample average net trade falls as the hold lengthens through 1, 2, 3, and 5 sessions. Consistent if that average is strictly decreasing in that order.
4. The Nasdaq-only book and the NYSE Group-only book have full-sample total returns of the same sign. Consistent if both are positive or both are negative. Not testable if either book has no closed trade.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- Instruments: every instrument in the store with at least one daily bar, except VIXY and UVXY. The symbol is `instrument_current.symbol`, stripped and upper-cased. Read with `agent-data/mdq.py`. Prices are split-adjusted. Dividends are not in the store (0 rows), so an ex-date inside a hold looks like a price loss and is not repaired.
- Bars: stored daily bars. `ts` is 09:30 America/New_York. The close is the session close and is not known at 09:30. No resample.
- Calendar: NYSE sessions from `mdq.nyse_sessions`. The book is the sessions from 2015-03-25 through 2026-10-01 on which SPY has a daily bar with a positive open and close. 2015-03-25 is the first stored NYSE Group file. 2026-10-01 is SPY's last daily bar. A session in that span with no SPY bar is omitted and the omission is counted. 2021-12-31 is a session. Early closes use the stored daily open and close. The exit timestamp is 13:00 on `mdq.EARLY_CLOSES` and 16:00 otherwise. The open timestamp is 09:30.
- Missing bars: a name with no daily bar at the fill does not enter and is not deferred. The other names in that cohort are weighted to sum to 1. A missing bar on a later session of the hold is not marked. That name is sold at the open of the next session that has a daily bar. If the sample ends first, the name is sold at its last marked close and the exit reason is `sample_end`.
- Short-sale rows: a session is restricted when a Nasdaq row has that `file_session`, or an NYSE Group row has `trigger_session` <= session < `deactivate_session`. If both say so, the source is `nasdaq`. A missing Nasdaq file is not filled in. Rows are matched to the current symbol only. `instrument_listing.opened_at` is not a market date and is not a filter. One listing row per priced instrument, so a former ticker is not in the test.
- Data checks the script must pass before it writes results: the synthetic self-test. It does not drop health-flagged bars.

## Primary rule

Parameters, all fixed: HOLD = 1 (the question asked). GRID = {1, 2, 3, 5}. COST_BPS = 10 per side. SAMPLE_START = 2015-03-25. SAMPLE_END = 2026-10-01. IS_END = 2024-06-28. OOS_START = 2024-07-01. EXCLUDE = {VIXY, UVXY}. Seeds 20261014, 20261015, 20261016.

1. **Episode.** On the NYSE calendar, a session D is an episode start for a symbol when D is restricted and the previous NYSE session is not. The entry session E is the next NYSE session. There is no warm-up.
2. **Signal.** The decision uses only the restriction state through D. The fill is the open of E. The file time is not in the signal. A same-day trigger is not a same-day fill.
3. **Cohort.** Every eligible name with the same E is one cohort. A name is eligible when its fill bar exists in the book. While any share from the current cohort is still open at the moment of a new fill, that new entry session is skipped and not deferred. Hold 1 exits at E's close, so the next session is free.
4. **Exit.** Sell at the close of session E+HOLD−1 on the book calendar. That is E's close when HOLD is 1. Exit reason `hold`. If that session is past the last book session, sell at the last book session's close and the reason is `sample_end`. If a bar is missing before that close, sell at the next available open and the reason is `missing_bar`.
5. **Fills.** Next open after the episode, as above. No bar, no trade. The delay check enters one book session later than E and then holds HOLD sessions. The upper bound buys the close of D and sells the close of E+HOLD−1. It is not a legal fill: the published file is not known at 16:00. It is not combined with the delay.
6. **Sizing.** At the fill, each eligible name receives an equal share of current equity. The weights sum to 1. That is gross exposure 1 at the fill price, before the side cost. Equity compounds. A sale's cash is not reallocated to the names that remain. Flat sessions earn 0. The account starts at 1.
7. **Costs.** 10 bp of the dollars bought, charged on the fill, and 10 bp of the dollars sold, charged on the exit. No borrow and no interest. The sweep multiplies 10 bp by 0, 0.5, 1, 2, and 3.

A trade row's gross is `weight * (exit_price / entry_price − 1)`. Its net subtracts `weight * cost` and `weight * (exit_price / entry_price) * cost`. The daily path is the equity change, not the sum of those nets across cohorts.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Nasdaq file for session D | After the close. A sampled trailer was 16:30 New York. The study does not assume an earlier print. | The open of E, the next session |
| NYSE Group row for a trigger on D | The file is that day's triggers. Treated as known after D's close, same as Nasdaq. | The open of E |
| Restriction on the previous session | That previous close | Deciding whether D starts an episode, at D's close |
| Daily close of D | 16:00, or 13:00 on an early close | Not used in the primary fill. Used in the labelled upper bound and in prediction 2 |
| Daily open of E | 09:30 of E | Primary fill |
| A later missing bar | When that session has no bar | The next open that does |
| SPY open and close | The same session's open and close | Benchmark and placebos, not the signal |
| Quintile edges of the prior decline | The full sample | The report only. Not a filter |
| Which names are in the store | The day the store was built, after the sample | The universe. This is a static snapshot |

## Samples

- Warm-up: none.
- **In-sample:** first book session on or after 2015-03-25 through 2024-06-28.
- **Out-of-sample:** 2024-07-01 through the last book session, 2026-10-01 if SPY's bar is there.
- The split is the house split. The window is already exposed for SPY and QQQ, as written under Prior exposure. A trade counts in a window by its fill session.

## Benchmarks

- SPY open-to-close on every book session, uncosted. The strategy is 0 when it is flat. The benchmark is not flat on those sessions.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at 10 bp per side: the kit defaults, compound equity.
- Breakdowns: calendar year; exit reason; quintile of SPY's open-to-close; the four predictions.
- Costs: 0, 0.5, 1, 2, and 3 times 10 bp. Fill delay: one book session. Upper bound: buy the close of D.
- Direction placebo: asset-adjusted. One draw flips one cohort. The pieces are the book's gross daily returns on that cohort's fill-through-exit sessions. Benchmark is SPY open-to-close. 2,000 draws, seed 20261014.
- Timing placebo: 500 draws, seed 20261015. Durations are the cohort lengths. The asset path is SPY open-to-close. The actual Sharpe is the Sharpe of that path masked to sessions the primary book held. Not an acceptance line. If the durations sum to more than the book, the result is null and the report says so.
- Block bootstrap: 20-session blocks, 2,000 draws, seed 20261016. Not an acceptance line.
- Concentration: drop the best 1% of sessions and the jackknife. Stored, not an acceptance line.
- Deflated Sharpe uses 4 trials, the four holds. Stored, not an acceptance line.
- Plateau grid on the in-sample Sharpe: hold in {1, 2, 3, 5}. One primary, hold 1. Out-of-sample Sharpe is shown and not used to choose.
- Cross-market: the same hold, cost, and sizing on Nasdaq-sourced episodes only, and on NYSE Group-sourced episodes only. These are publisher subsets, not a second market.
- Verification: the self-test, then `verify.py` on every primary trade.

## Acceptance

The kit's `status_from` is the verdict. The universe type is `static_snapshot`. Performance failure, including the four extra lines, becomes **Rejected**. A missed out-of-sample count, with every other line passing, becomes **Inconclusive**. If every line passes, the snapshot forces **Rejected (Survivorship Contaminated)**. It does not become a paper-trading candidate.

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. IS Sharpe > 0, and at least 60% of the IS grid cells have Sharpe > 0. Four cells, so at least three.
4. Full-sample total return > 0 at 2× base cost per side.
5. OOS Sharpe > 0 on at least one of NASDAQ and NYSE_GROUP.
6. At least 100 OOS trades. The counted sample is large enough that the floor stays at the default. A trade is one name's round trip.
7. Full-sample max drawdown ≥ −0.35.
8. Minimum mark-to-market equity > 0.
9. Max intraday gross leverage ≤ 4.
10. Max overnight gross leverage ≤ 2. Overnight is the position still open after the close. Hold 1 is flat overnight.

A failed line fails the strategy.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; a single symbol; a day-of-week or size filter; names that passed the health check only; a 9% decline that was not flagged; the trigger day's close as a fill; VIXY or UVXY; a different cost or a different sample split.
