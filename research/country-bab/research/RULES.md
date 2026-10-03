# Country betting-against-beta: pre-registered rules

Written 2026-10-02, before any beta rank was turned into a return, and before any forward return, hit rate, or P&L was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `research/country-bab/research/counts.py` (output `counts.json`), read-only through `agent-data/mdq.py`. No beta, no forward return, no hit rate, no P&L. It recorded:
  - SPY: 3,959 daily bars, 2011-01-04 through 2026-10-01. No bar after 2026-10-01. No duplicate dates. Every open, high, low, and close is positive, and high/low brackets the open and the close. No corporate action. Coverage: 4,103 complete, 4 missing, 1 partial, 3,959 sessions with bars. `needs_attention` is exactly three rows, all `missing` with note `no bars`: 2012-10-29, 2012-10-30, and 2018-12-05. The other non-complete rows are labelled expected and are not in `needs_attention`: 2021-12-31 `partial` with 1 bar (`expected: real session the terminal's calendar calls a holiday; the bar is good`) and 2025-01-09 `missing` with 0 bars (`expected: market closed`). mdq's calendar from 2011-01-04 through 2026-10-01 has 3,962 sessions. The only three without a SPY bar are those three closures. No SPY bar falls off that calendar.
  - Each of EWA, EWC, EWG, EWH, EWJ, EWS, EWU, EWW, EWZ, EWL, EWT, and EWY: 3,960 daily bars, 2011-01-04 through 2026-10-02. No duplicate dates, no non-positive OHLC, no high/low violation, no zero-volume bar. Coverage: 4,104 complete, 4 missing, 1 partial. `needs_attention` is the same three closure dates. 2021-12-31 and 2025-01-09 match SPY's expected notes. Each name has exactly one bar after the sample end, 2026-10-02, and no other bar that SPY does not have. Through 2026-10-01 each name's sessions match SPY's 3,959 sessions exactly (0 missing, 0 extra). Paired closes through 2026-10-01: 3,959 on every name.
  - Corporate actions, and no others: EWJ split_ratio 0.25 on 2016-11-07; EWS, EWU, and EWT split_ratio 0.5 on 2016-11-07. Source `mboum`. No dividend row on any of the twelve or on SPY. These are reverse splits in the stored ratio (mdq divides earlier prices by the ratio). Split-adjustment continuity, ex-date close over the 2016-11-04 close: EWJ adjusted 1.0071 versus raw 4.028; EWS adjusted 1.0175 versus raw 2.035; EWU adjusted 1.0149 versus raw 2.030; EWT adjusted 1.0336 versus raw 2.067. Adjusted opens sit on the same side of 1. None of the adjusted ratios is near 4 or near 2. The raw ratios are near 1/split_ratio. The adjusted series does not jump by the split factor. The rule uses adjusted prices.
  - Completed month-end signals, definition below: 189. First 2011-01-31, last 2026-09-30. 2026-10-01 is not a signal. A name with 253 paired closes through the signal (252 paired returns) is first eligible on 2012-01-31, and all twelve are eligible together. Histogram of eligible names on the 189 dates: 0 on 12 dates, 12 on 177 dates. Dates with at least six: 177. First is 2012-01-31. Out-of-sample month-ends, signal on or after 2024-07-01: 27, from 2024-07-31 through 2026-09-30. 2024-07-01 is a SPY session. The last SPY session before it is 2024-06-28.
- **Earlier studies.** Every study row in `research/README.md` was read, and the rules and reports of the studies that share this out-of-sample calendar were read. `trades.csv` and `daily.csv` were not. None of those studies used EWA, EWC, EWG, EWH, EWJ, EWS, EWU, EWW, EWZ, EWL, EWT, or EWY. None ranked a beta. The closest books are not this test:
  - `spdr-sector-momentum` (Rejected) is 12-1 month cross-sectional momentum on SPDR sector ETFs, long the top 3 and short the bottom 3. Out of sample it returned +6.2% (Sharpe 0.253, profit factor 0.935, 35 round trips). The signal there is past return. This study does not use past return as the signal.
  - `commodity-etf-momentum` (Inconclusive) is 12-month cross-sectional momentum on six commodity ETFs. Out of sample +41.5% (Sharpe 0.574, 21 round trips). Different instruments, and the signal is past return.
  - `fx-etf-momentum` (Rejected) is 63-session time-series momentum on six CurrencyShares ETFs. Out of sample +6.36% (Sharpe 0.4908). Different instruments.
  - `treasury-etf-trend` (Inconclusive) is 12-month time-series momentum on TLT and IEF. Out of sample −10.52% (Sharpe −0.65, 16 round trips).
  - `spy-overnight-premium` (Rejected) is long every SPY night. Out of sample +0.27% (Sharpe 0.053). Over the same sessions, uncosted SPY close-to-close returned +40.5% (Sharpe 0.985, max drawdown −19.9%).
  - `micro-futures-trend` (Rejected) is a 1/3/12 time-series book on micro futures. It is not a country-ETF beta rank. Its rules and report, as already summarized in the FX study's rules, recorded that US equities rose across 2023–2025 and that long ES was strongly positive in that study's window.
  - The QQQ and SPY intraday studies, the RSI(2) dip-buy, and the gap fades are single-name or small-cap rules. They are not this cross-section.
- **What I already know about the test windows.** The out-of-sample window 2024-07-01 through 2026-10-01 is **not unseen data**. It is a rising equity market in the reports above. SPY close-to-close gained about +40.5% to +41.7% with a Sharpe near 1.0 and a max drawdown near −19% to −20%. QQQ buy-and-hold over the overlapping window through 2026-09-25 was about +55% with a Sharpe near 1.0. April 2025 was a crash and a rebound; SPY rose about 10.5% on 2025-04-09. Calendar 2022, inside the in-sample window, was a down year for the equity indexes in those reports. From November 2024, stored SPY daily closes can differ from the 15:59 regular-hours print (`spy-rsi2-dip-buy`, re-checked in `spdr-sector-momentum`). This study uses the stored daily open and close anyway. It does not recompute that minute comparison before the lock. I do not know the country-ETF beta ranks, and no return on these twelve series has been computed.
- **Where the parameters came from.** The user's study design, fixed before any return on these series. The signal is trailing beta, not past return, because the cited mechanism is leverage aversion (Frazzini and Pedersen 2014), not momentum. The 252-session window is one trading year, chosen as the primary before the run. The grid is the protocol plateau around that window. The book is three and three because that is what the request specified. The 5 bp cost is a round prior for country ETFs that are thinner than SPY; the store has no quotes. The 2024-07-01 split is the repo's existing out-of-sample start. The 24-trip floor is the same monthly-rebalance floor the recent ETF studies locked, and the reason is below. None of these is fit to a return on these series.

## Hypothesis

The three country ETFs with the lowest trailing beta to SPY outperform the three with the highest trailing beta over the next month.

**Mechanism.** Investors who cannot use leverage bid up high-beta assets, so low beta earns more per unit of risk than the CAPM allows. The other side of this trade is a leverage-constrained buyer of high-beta countries. Frazzini and Pedersen (2014) build a beta-neutral betting-against-beta factor. This study does not. It is a dollar-neutral long-short of ranked betas: long the three lowest, short the three highest, equal weight. The signal is beta, not past return.

**Known counter-forces.** Betting against beta loses when high-beta assets rebound together. These ETFs are already diversified country portfolios, so the beta spread can be small. The price is a mix of the local equity market and the currency against the dollar. No dividend is stored. If low-beta countries have higher yields, this accounting understates the long side. No borrow fee is charged. The result was published in 2014, so post-publication decay is a real possibility. The out-of-sample window is already known to be a rising US equity market, which is a headwind for a book that is long low beta and short high beta if that beta spread is real. The report must show long-book and short-book price P&L separately, the correlation with SPY, and the result by quintile of the SPY day. None of that changes the rule.

## Predictions beyond P&L

If the mechanism is right, then, on the full evaluation sample:

1. Gross price P&L, summed in account dollars and not compounded, is positive on the long book and positive on the short book.
2. The strategy's correlation with SPY close-to-close is lower than the equal-weight country book's correlation with SPY. Both correlations are Pearson correlations of daily simple returns over the same full evaluation sessions. A series with sample standard deviation 0 makes this prediction not testable.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. Neither one changes the verdict. A pass on P&L with a failed prediction does not confirm the mechanism.

## Data

- Instruments: EWA (Australia), EWC (Canada), EWG (Germany), EWH (Hong Kong), EWJ (Japan), EWS (Singapore), EWU (United Kingdom), EWW (Mexico), EWZ (Brazil), EWL (Switzerland), EWT (Taiwan), EWY (South Korea). All twelve are in the universe on every date. None is dropped. Read with `agent-data/mdq.py`, daily bars, split-adjusted (the default). SPY daily bars date the book and enter the beta. SPY is not a traded leg. Dividends are not stored and are not invented. Price returns omit distributions. If low-beta countries have higher yields, the long side is understated.
- Bars: stored 1d bars. A daily bar's `ts` is 09:30 ET. Its close is not known until 16:00 ET and is not used before then. No coarser bar is built. Country bars on 2026-10-02 are ignored. The shared SPY series ends 2026-10-01, and that is the sample end.
- Calendar: book sessions are the SPY daily-bar dates from 2011-01-04 through 2026-10-01. That set skips 2012-10-29, 2012-10-30, and 2018-12-05. Those dates are not booked as zero days. 2025-01-09 is not a session. 2021-12-31 is a session and is included. Early closes with a daily bar are ordinary sessions.
- Missing bars: not forward-filled. A book session with no bar for a name is dropped from that name's pair. It is not a zero return inside the beta. While a name is held, a session with no bar earns 0 that day, does not force an exit, and does not update the marked price. The next bar's gap is from the most recent observed close to that bar's open.
- The universe was named in advance. It is hand-picked among iShares country ETFs that listed before 2011. Countries that were not named are not added. That is selection, and it is disclosed. It is not repaired after the run.
- Data checks the script must pass before it writes results: the bar counts, ranges, extra 2026-10-02 bar, split set, and adjusted-close ratios above (each adjusted ex-date close ratio strictly between 0.90 and 1.10, and each raw close ratio within 5% of `1/split_ratio`); 189 signals, first 2011-01-31, last 2026-09-30, 2026-10-01 not among them; the first signal with at least six names that have 252 paired returns is 2012-01-31; the first fill is 2012-02-01; 27 out-of-sample signals; every used open and close is positive. A failure aborts before any result file is written and does not append a run-log entry.

## Primary rule

Parameters, all fixed:

- `BETA_WINDOW` = 252 paired returns. One trading year. Not the Frazzini-Pedersen shrunk beta. Not tuned.
- `N_LONG` = 3, `N_SHORT` = 3, `MIN_NAMES` = 6.
- `COST` = 5 bp of notional per side (0.0005) on every name. Country ETFs are thinner than SPY. The store has no quotes. This is a round prior, not a measured spread, and it is not changed after seeing P&L.
- `OOS_START` = 2024-07-01. `SAMPLE_END` = 2026-10-01.
- Grid windows `{126, 189, 252, 315, 378}` paired returns. Book size fixed at 3. The primary window is one of the five cells.
- Starting equity = 1. Cash earns zero. No stock-borrow fee. No margin finance. Short-sale proceeds stay in cash.

1. **Beta.** On a signal date, a paired session for a name is a book session on which that name has a split-adjusted close. Let those paired sessions on or before the signal be `p[0] … p[k]`, and require `p[k]` to be the signal date. Otherwise the name is ineligible. The paired return ending at `p[j]` (`j ≥ 1`) is `close_i(p[j]) / close_i(p[j-1]) − 1` for the name and `close_SPY(p[j]) / close_SPY(p[j-1]) − 1` for SPY. Both legs use those two dates. Sessions that are not paired are not inserted. The name is eligible when `k ≥ 252`, which is 252 paired returns ending at the signal. Beta is the OLS slope with an intercept,

   `Σ (r_i − mean_i)(r_s − mean_s) / Σ (r_s − mean_s)²`,

   over those 252 returns. Means are the arithmetic means of the 252 returns. If the denominator is 0, the name is ineligible. No full-sample mean, volatility, or quantile enters the signal. The signal is not a past-return rank.

2. **Signal dates.** For each calendar month from 2011-01 through 2026-10, take mdq's `nyse_sessions` from the first through the last calendar day of that month. The month has a signal only when that month's true last NYSE session is on or before 2026-10-01 and that date has a SPY daily bar. The signal date is that last session. 2026-10-01 is not a signal: October 2026's last NYSE session is after the store. The signal uses that session's closes and is known at 16:00 ET. It is not acted on until a later open.

3. **Rank and target weights.** Sort eligible names by beta ascending, and break ties by symbol ascending (ordinary string order: EWL before EWS, EWZ last). The tie-break is deterministic, not tuned. If fewer than six names are eligible, every target weight is 0. Otherwise the first three get `+1/3`, the last three get `−1/3`, and every other name gets 0. Gross long 1 and gross short 1 when the book is on. No stop, no volatility target, no momentum overlay, no shrinkage toward 1, no scaling to beta 1.

4. **Position.** One book. Shares are constant between fills. A later signal replaces the target weights entirely. There is no second position in the same name.

5. **Exits.** A name exits when a fill sets its target to 0, or when a fill flips its sign. There is no stop, target, or time stop. A position still open on the last evaluation session is marked to that session's close (or, if the name has no bar that day, to its most recent observed close) with exit reason `sample_end` and with no exit trade and no exit cost. That mark closes the trade list. It is not a live order.

6. **Fills.** The scheduled fill is the next book session after the signal, at that session's open. A name with a bar that day is filled then. A name with no bar that day keeps its old shares and stays unfilled until the open of the next session on which it has a bar, at whatever the target map is then. A newer signal, already computed at a close, replaces the target before that deferred open. Names that have already filled the current target are not resized on the deferred day. If mark-to-open equity before the trade is less than or equal to 0, every target is set to 0, names with bars are flattened, and the book takes no new risk. A signal with no later book session does not fill. The last signal, 2026-09-30, has later sessions and can fill.

7. **Sizing and accounting.** On a fill session, at the open, before any share change: old shares earn the gap from the previous observed close to today's open. A missing bar earns 0 and does not change the marked price. Equity for sizing is that mark-to-open equity, before cost, and the same equity is used for every name filled at that open. For each name filled, set signed shares so that notional at the open equals `target weight × that equity`. Charge cost on the absolute notional of the share change. New shares then earn the open-to-close. A session that is not a fill marks the book the same way with the share change equal to 0. Daily strategy return is close equity divided by prior close equity, minus 1. Prior close equity before the first fill is 1.

8. **Costs.** 5 bp of notional per side, on the absolute notional of each share change, including a same-side resize. A flip is one share change. Its cost equals the cost of closing the old notional plus the cost of opening the new notional, and the trade list splits it that way. No cost is charged on the `sample_end` mark. Cash interest and borrow are zero.

9. **Round trips.** One trip is one name, from the fill where its share sign leaves 0 or flips, until the fill where that sign goes flat or flips again. A same-side resize is the same trip. Its cost is part of that trip. A trip still open at the sample end ends with reason `sample_end`. `gross_pnl` is the sum of that trip's gap and open-to-close price P&L in account dollars. `net_pnl` is `gross_pnl` minus the costs assigned to the trip. A trip is out-of-sample when its entry fill is on or after 2024-07-01. The whole trip stays in that sample even if daily P&L crosses the boundary. Daily Sharpe uses the session date, not the trip's entry date.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Name close and SPY close on the signal date, and earlier paired closes | 16:00 ET on the signal date | The beta at that close. The fill is no earlier than the next session's open |
| Daily bar `ts` of 09:30 ET | Bar open | Not treated as the time the close is known |
| Paired-return window | Those earlier paired closes only | The signal. A later close is never inside the window. A missing session is dropped, not filled |
| Rank, tie-break, eligibility | The signal close | Target weights for the next fill only |
| Open of the fill session | That open | Sizing and the fill price. The fill session's close is not used to size |
| Marked price of a name with no bar | The most recent observed close | That session's mark. It does not enter the beta |
| SPY close-to-close series | Prior close and that session's close | That session's reported SPY return only. SPY's return inside a pair uses only the two paired dates |
| Grid, placebos, quintile bins | The same sessions as the primary | Reported checks. Quintile edges are descriptive. They are not a trading filter |
| Same-bar close fill | The signal close | A labelled upper bound only. Not the primary |

The universe on a date is the twelve names, restricted to those with 252 paired returns ending that date. Nothing is normalized on the full sample.

## Samples

- Warm-up: history before the first fill. The first signal with six eligible names is 2012-01-31. The first fill is the next book session, 2012-02-01. Sessions before that fill are not in the evaluation window and are not in `daily.csv`.
- **In-sample:** 2012-02-01 through the last book session strictly before 2024-07-01, which is 2024-06-28.
- **Out-of-sample:** 2024-07-01 through 2026-10-01, inclusive.
- The split is the repo's existing out-of-sample start. It was chosen before any return on these series. The out-of-sample calendar is not unseen. What the earlier reports already showed is listed under Prior exposure. The split is not moved.
- A signal on 2024-06-28 fills on 2024-07-01. That entry is out-of-sample. In-sample daily returns stop at the 2024-06-28 close.
- Sharpe, total return, CAGR, volatility, drawdown, and the t-stat use evaluation sessions only. A flat day inside the window counts as 0. Warm-up days do not.

## Benchmarks

- Uncosted equal-weight long-only of the names eligible on that signal, including a signal with fewer than six eligible names. Weight `1/n` each, gross 1. Same signal dates, same next-open fills, same deferred-bar rule, same share accounting, cost 0. If no name is eligible, that benchmark is flat. The strategy's six-name flat rule does not apply to this benchmark. This is the hurdle the report plots against the strategy. It is not an acceptance line.
- SPY close-to-close, uncosted, on the same evaluation sessions. The first day's return uses the SPY close of the previous book session. SPY is reported. SPY is not the hurdle.

## Secondary candidates

None. There is no second country-ETF family in the store. Line 5 is not a secondary and is not scored. It is not a pass and not a fail.

## Definitions

- **Sharpe.** Mean of the daily simple returns in the window, divided by their sample standard deviation (`ddof=1`), times √252. The risk-free rate is 0. If the window has fewer than 2 sessions, or the sample standard deviation is 0, Sharpe is undefined. A test that needs an undefined Sharpe fails.
- **Total return.** Compound of the daily simple returns in the window, minus 1. Each window compounds from 1. It does not carry equity in from the other window.
- **CAGR.** On a 252-session year: `(1 + total return) ** (252 / n_sessions) − 1`.
- **Annualized volatility.** Sample standard deviation of daily simple returns times √252.
- **Max drawdown.** Minimum of compounded equity over its running peak, minus 1, on that window's daily returns only. The path starts at 1.
- **t-stat.** Mean daily return divided by its standard error. The standard error uses the sample standard deviation and the session count.
- **Profit factor.** Sum of positive trip `net_pnl` divided by the absolute sum of negative trip `net_pnl`. A trip with net P&L of 0 is neither. If the window has no losing trip, or no trip, the profit factor is null and line 1 fails.
- **Average net trade, bp.** `net_pnl / entry_equity × 10,000`. `entry_equity` is mark-to-open equity before cost on the entry fill.
- **Gross Sharpe.** Daily price P&L divided by the previous close's net equity, then the Sharpe definition above. Costs stay in the equity denominator. They are not subtracted from the numerator. The share path is the path of the book being scored.
- **Correlation.** Pearson correlation of two daily simple-return series on the full evaluation window.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at 5 bp per side, for the strategy, and the same return metrics for both benchmarks: the definitions above, plus trades, win rate, average winner, average loser, holding time, time in market, and average gross exposure. Time in market is the fraction of evaluation sessions with any nonzero shares after that session's fills. Average gross exposure is the mean of gross notional at the close divided by close equity.
- A trip is assigned to the sample that contains its entry fill. Daily Sharpe uses the daily returns inside the window, including marks of positions that were entered earlier.
- Breakdowns: calendar year (strategy return, Sharpe, max drawdown, equal-weight return, SPY return); long versus short trips (count, net dollars, gross dollars, profit factor, win rate); exit reason (`flip`, `flat`, `sample_end`); per name (gross P&L, net P&L, trades). Quintile of the SPY close-to-close move on the full evaluation sample: sort by SPY return ascending, break ties by date ascending, five buckets, quintile 1 = worst SPY days. Sizes are as equal as the session count allows, and any remainder goes to the earliest quintiles. This is not a filter.
- Costs: resimulations at 0, 0.5×, 1×, 2×, and 3× the 5 bp rate, that is 0, 2.5, 5, 10, and 15 bp per side. Each multiple resizes shares, because equity compounds net of the cost being tested. Report full-sample Sharpe, out-of-sample Sharpe, and full-sample total return.
- Fill delay: the scheduled fill is two book sessions after the signal, at that open. Same sizing. Report full and out-of-sample Sharpe and full return.
- Upper bound: fill at the signal close. Old shares earn that day's full move. The trade is done at the close. New shares earn from the next session. Cost is charged at the close. Labelled upper bound, not the primary.
- Direction placebo: keep each trip's dates and its daily gross dollar price P&L, and multiply each trip by an independent ±1. 2,000 draws, seed `20261061`. The daily gross return divides the summed dollars by the primary path's prior close equity. Compare the draw's gross Sharpe with the actual gross Sharpe. `p = (1 + count of draws whose gross Sharpe is greater than or equal to the actual) / 2001`. An undefined draw Sharpe does not count as greater than or equal to a defined actual. If the actual gross Sharpe is undefined, line 2 fails.
- Timing placebo: 500 draws, seed `20261063`. At each signal, take the eligible names in symbol order, permute their betas, and re-rank with the same tie-break. One generator, draws in order. Resimulate at the 5 bp cost. The statistic is that path's gross Sharpe over the draw's own evaluation dates. A draw with fewer than 2 evaluation sessions scores 0 for this comparison. Compare with the primary gross Sharpe. `p = (1 + count of draws ≥ actual) / 501`. This p is reported and is not an acceptance line.
- Block bootstrap: circular blocks of 20 evaluation sessions, 2,000 draws, seed `20261062`, of the primary's full-sample net daily returns. Draw enough blocks to cover the length and truncate. Report the 2.5th and 97.5th percentiles of the Sharpe, and the fraction of draws with Sharpe less than or equal to 0. Not an acceptance line.
- Plateau grid, in-sample only for the verdict: beta window in `{126, 189, 252, 315, 378}`, book size fixed at 3, cost 5 bp. Five cells. Each cell is scored on the primary's evaluation dates. A primary-window session before that cell's first fill counts as 0. A cell that filled before the primary window contributes its carried-position return on the primary dates, not a cold start. The 252 cell's in-sample Sharpe must equal the primary in-sample Sharpe. Out-of-sample Sharpe of each cell is shown for selection bias only, with the Spearman rank correlation of the five in-sample Sharpes against the five out-of-sample Sharpes. Nothing is selected from the grid.
- Cross-market: not applicable. No second country-ETF family is in the store.
- Verification: a self-test on synthetic sessions before the store is opened. It covers a hand-computed beta rank, the next-open fill, a hold-day gap, a flip, a same-side resize, a month with fewer than six names, a missing bar while held, a missing bar dropped from the pair, a deferred fill, the symbol tie-break, a zero SPY-variance window, the sample-end mark, a one-session delay, and the signal-close upper bound. `verify.py` is an independent reimplementation, shares no signal code with `backtest.py`, and matches every trade on symbol, side, entry date, entry price, exit date, and exit price. Seed `20261064` also draws 40 signal dates; those dates' long and short sets are matched as well. If replaying every date is fast, all dates are matched.

Gross price P&L for prediction 1 is the sum of daily long-share price P&L and the sum of daily short-share price P&L over the full evaluation sample. Costs are excluded. The sums are in account-dollar units starting from equity 1. They are not compounded returns.

## Acceptance

The primary is a **paper-trading candidate** only if every applicable line holds at 5 bp per side:

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0. With five cells, 60% means at least three cells strictly above zero. The primary cell counts as one of the five.
4. Full-sample total return > 0 at 2× base cost (10 bp per side).
5. Not applicable. No second country-ETF family is in the store. This line does not pass or fail the strategy.
6. At least 24 out-of-sample round trips. A round trip is the trip defined above. It is out-of-sample when the entry fill is on or after 2024-07-01. A `sample_end` trip counts. Reason for lowering the protocol's 100: the rule rebalances monthly, and the out-of-sample window has 27 month-ends. A 100-trip bar would force a higher-frequency rule than the one that was pre-registered. No other threshold is lowered.

Status, applied in this order:

- If line 6 fails, the verdict is **Inconclusive**, even if another scored line also fails.
- If line 6 holds and any of lines 1, 2, 3, or 4 fails, the verdict is **Rejected**.
- If lines 1, 2, 3, 4, and 6 all hold, the verdict is **Paper-trading candidate**.

Never "ready for live capital." A failed line fails the strategy. A grid cell, one side, or one country is not promoted in its place.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone; one country; ranking on past return; dropping a country; a dividend adjustment; the published beta-neutral BAB construction; shrinking beta toward 1; a volatility target; a momentum overlay; a different cost; a different fill; moving the sample split; treating 2026-10-01 or 2026-10-02 as a month-end; replacing stored daily closes with 1-minute closes. A variant suggested by the results goes under *Ideas for a new study* and needs its own rules and data this study did not use.

## Seeds

- Direction placebo: `20261061`
- Block bootstrap: `20261062`
- Timing placebo: `20261063`
- Verify sample: `20261064`

## References

- Frazzini, A., and Pedersen, L. H. (2014). Betting against beta. *Journal of Financial Economics*, 111(1), 1–25.
- Black, F. (1972). Capital market equilibrium with restricted borrowing. *Journal of Business*, 45(3), 444–455.
- McLean, R. D., and Pontiff, J. (2016). Does academic research destroy stock return predictability? *Journal of Finance*, 71(1), 5–32.
