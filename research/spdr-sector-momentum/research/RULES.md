# SPDR sector momentum: pre-registered rules

Written 2026-10-02, before any strategy return, forward return, hit rate, or P&L was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `research/spdr-sector-momentum/research/counts.py` (output `counts.json`), read-only through `agent-data/mdq.py`. No formation return, forward return, hit rate, or strategy P&L. It recorded:
  - SPY and the nine original sectors (XLK, XLF, XLE, XLV, XLI, XLY, XLP, XLU, XLB): 3,959 daily bars each, 2011-01-04 through 2026-10-01. No 1-minute bars on any sector ETF.
  - XLRE: 2,759 daily bars, 2015-10-08 through 2026-10-01. XLC: 2,083 daily bars, 2018-06-19 through 2026-10-01.
  - Against SPY's sessions, the only missing bars on or after a name's first bar are XLRE on 2015-10-14 and 2015-11-27. XLK, XLF, XLE, XLV, XLI, XLY, XLP, XLU, XLB, and XLC miss none.
  - mdq's calendar from 2011-01-04 through 2026-10-01 has 3,962 sessions. The only three with no SPY daily bar are 2012-10-29, 2012-10-30, and 2018-12-05. Coverage marks those `missing`. 2025-01-09 is a special closure in mdq and is not a session. 2021-12-31 is a session; the daily bar is present and coverage marks it `partial` with the note that the bar is good (the terminal's calendar wrongly calls that Friday a holiday). It is included.
  - Corporate actions: a 2-for-1 split (`split_ratio` 2.0, source `mboum`, ex-date 2025-12-05) on XLK, XLE, XLY, XLU, and XLB. No other splits. No dividend rows on any of the eleven names or on SPY.
  - Split-adjustment continuity, adjusted close on 2025-12-05 over the 2025-12-04 adjusted close: XLK 1.0055, XLE 1.0009, XLY 1.0056, XLU 0.9910, XLB 0.9936. None is near 2 or 0.5. Raw close ratios on the five split names are 0.503, 0.500, 0.503, 0.495, and 0.497. The adjusted close series does not jump by 2×. The stored ex-date open and high are not continuous on three names: XLK open = high = 174.74 against a prior adjusted close of 145.625 and a close of 146.43; XLE open 49.34 and high 55.43 against a prior close of 46.08 and a close of 46.12; XLB open = high = 47.34 against a prior close of 44.38185 and a close of 44.10. Lows and closes sit on the prior adjusted close. The rule uses the stored open and the stored close anyway. 2025-12-05 is not a fill date: the November 2025 signal is the last NYSE session of that month (2025-11-28) and the fill is the next SPY session, 2025-12-01.
  - Completed month-end signals (definition below): 189. The first with at least six names that have a bar 273 own sessions earlier is 2012-02-29. Counts of eligible names on those 189 dates: 0 on 13 dates, 9 on 57, 10 on 32, 11 on 87. Dates with at least six: 176. XLRE's first such date is 2016-11-30. XLC's is 2019-07-31. Out-of-sample month-ends, 2024-07-31 through 2026-09-30: 27. 2026-10-01 is the last stored session and is not a month-end, because October 2026's last NYSE session is after the store.
  - SPY daily close versus the last regular-hours 1-minute close, on the 1,254 overlapping sessions 2021-09-27 through 2026-09-25: 111 sessions differ by more than 20 bp, all on or after 2024-11-01. The largest is 2025-04-02, stored daily close 544.83 versus last 1-minute close 564.52. The same comparison cannot be run on the sector ETFs. They have no 1-minute bars. This study uses the stored daily open and close anyway.
- **Earlier studies.** Every `research/*/research/RULES.md` and `research/*/report/REPORT.md` on disk as of this writing was read. None used XLK, XLF, XLE, XLV, XLI, XLY, XLP, XLU, XLB, XLRE, or XLC. IGV appears in intraday studies as a single software ETF, not as one name in a sector cross-section. Closest priors, and they are not this test:
  - Equity dip, gap, and intraday studies (qqq-intraday-trend, intraday-channel-trend, qqq-15m-turtle-overnight, qqq-atr-scale-in, qqq-atr-martingale, qqq-atr-band-dip-eod, qqq-bollinger-adding, spy-rsi2-dip-buy, index-opening-pop-fade, igv-small-account-fade, small-cap-gap-up-fade, finviz-gap-up-fade, low-liq-high-vol-mean-reversion) and the two portfolio combinations. Those are single-name intraday rules, an index dip-buy, or small-cap gap and reversal books.
  - `micro-futures-trend` is a rejected time-series book (Hurst, Ooi, and Pedersen signals on futures). This study is cross-sectional sector momentum. It is not that book.
  - `low-liq-high-vol-mean-reversion` is cross-sectional, but it is a one-week reversal on a 2026 small-cap snapshot, and it lost money. The sign, the horizon, and the universe differ.
- **What I already know about the test windows.** The out-of-sample window 2024-07-01 through 2026-10-01 is not unseen. Through 2026-09-25, earlier studies in this repo already reported a strong equity bull market: SPY buy-and-hold about +42% (Sharpe near 1.0) and QQQ about +55% (Sharpe 1.01), with calendar-year SPY gains on the order of +23% in 2024, +16% in 2025, and +13% in 2026 to 25 September. April 2025 contained a sharp selloff and a rebound of about +10% in SPY on 9 April (the tariff pause). Several intraday books' out-of-sample results were dominated by those days. From May 2025, QQQ kept rising while at least one intraday edge went flat. I do not know the sector cross-section, and I have not computed a sector return. Sessions 2026-09-26 through 2026-10-01 were not in those reports. The in-sample window includes the 2022 bear market, already visible in the QQQ studies. `spy-rsi2-dip-buy` already reported the SPY daily-bar defect above; this file re-verified it.
- **Where the parameters came from.** The user's study design, fixed before any return on these series. The 12-month formation with a one-month skip is Jegadeesh and Titman (1993), not a fit to this store. The 1 bp cost is the protocol default for SPY-class liquidity. The 2024-07-01 split is the repo's existing out-of-sample start. The top-three / bottom-three book and the 24-trip floor are part of that design. See the reason on line 6.

## Hypothesis

The three SPDR sector ETFs with the highest 12-month return, skipping the most recent month, outperform the three with the lowest such return over the next month.

**Mechanism.** Capital and attention move slowly across industries, so the recent cross-section of sector returns predicts the next month. The other side is an investor who buys recent losers, or who is tied to a sector benchmark and must rebalance toward losers and away from winners (Jegadeesh and Titman 1993; Moskowitz and Grinblatt 1999).

**Known counter-forces.** Momentum crashes when the market rebounds and recent losers rise together. The long book can be a high-beta book in disguise. Dividends are not in the store, so high-distribution sectors look worse when held long, and better when held short, than they would in a total-return test. A stock-borrow fee is not charged. From November 2024 the stored daily closes may include extended-hours prints, as the SPY check shows, and that noise is inside the out-of-sample window. The report must show long-book and short-book P&L separately, and the result by quintile of the SPY day.

## Predictions beyond P&L

If the mechanism is right, then:

1. Full-sample gross price P&L, summed in account dollars and not compounded, is positive on the long book and positive on the short book.
2. Rebalances whose formation gap (mean formation return of the three longs minus mean formation return of the three shorts) is in the top half have a higher mean strategy holding-period return than rebalances in the bottom half.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. Neither one changes the verdict.

## Data

- Instruments: XLK, XLF, XLE, XLV, XLI, XLY, XLP, XLU, XLB, XLRE, XLC, plus SPY as the calendar and the buy-and-hold benchmark. Read with `agent-data/mdq.py`, daily bars, split-adjusted (the default). Dividends are not in the store and are not invented. Price returns omit distributions. High-distribution sectors (utilities, real estate, energy, financials) therefore look worse when held long and better when held short than a total-return test.
- Bars: stored 1d bars. A daily bar's `ts` is 09:30 ET; its close is not known until 16:00 ET and is not used before then. No coarser bar is built.
- Calendar: evaluation sessions are the SPY daily-bar dates. mdq's `nyse_sessions` supplies the month-end rule. 2012-10-29, 2012-10-30, and 2018-12-05 appear on that calendar and have no bars; they are skipped, not booked as zero days. 2025-01-09 is not a session. Early closes are ordinary sessions when a daily bar exists. 2021-12-31 is included.
- Missing bars: not forward-filled. A name with no bar on the signal date is ineligible. Formation lags count that name's own daily bars in order, not calendar days, so a missing session is skipped in the count rather than filled. While a name is held, a session with no bar earns 0 that day, does not force an exit, and does not update the marked price. The next bar's gap is taken from the most recent observed close. That is the only use of the most recent available close.
- Data checks the script must pass before it writes results: the bar counts, ranges, split set, and adjusted-close ratios above; the first completed month-end with six eligible names is 2012-02-29; there are 27 out-of-sample month-ends; every used open and close is positive. A failure here aborts before any result file is written.

## Primary rule

Parameters, all fixed:

- `SKIP` = 21 own sessions. Jegadeesh-Titman one-month skip. Not tuned.
- `LOOKBACK` = 252 own sessions. Twelve months. The far lag is `LOOKBACK + SKIP` = 273. The formation is `close[t-21] / close[t-273] - 1`.
- `N_LONG` = 3, `N_SHORT` = 3, `MIN_NAMES` = 6.
- `COST` = 1 bp of notional per side (0.0001). Protocol default for SPY-class liquidity. These ETFs are in that class. Not changed after seeing P&L.
- `OOS_START` = 2024-07-01. `SAMPLE_END` = 2026-10-01, the last stored session.
- Grid lookbacks `{126, 189, 252, 315, 378}`, skip fixed at 21. The primary lookback is one of the five cells.
- Starting equity = 1. Cash earns zero. No stock-borrow fee. No margin finance. Short-sale proceeds stay in cash.

1. **Formation.** On a signal date, a name is eligible only if it has a split-adjusted close on that date and at least 273 earlier closes in its own daily-bar series. Let the own closes ending at the signal be `c[0..k]` with `c[k]` the signal close. The formation return is `c[k-21] / c[k-273] - 1`. No other price enters the signal. No full-sample mean, volatility, or quantile is used.
2. **Signal dates.** The last NYSE session of each calendar month, according to mdq's calendar, that has a SPY daily bar, and only when that month's true last NYSE session is on or before the last SPY bar in the store. A month that is still open at the sample end contributes no signal. 2026-10-01 is not a signal. The signal is computed from that session's closes and is known at 16:00 ET.
3. **Rank and target weights.** Sort eligible names by formation return descending, and break ties by symbol ascending (alphabetical, so the earlier symbol ranks higher and is more likely to be long). This tie-break is deterministic, not tuned. If fewer than six names are eligible, every target weight is 0 and the book is flat that month. Otherwise the first three get weight `+1/3`, the last three get weight `−1/3`, and the middle names get 0. Ineligible names get 0. Gross long 1 and gross short 1 when the book is on. No stop, no volatility target, no overlay.
4. **Position.** One book. Shares are constant between fills. A later signal replaces the target weights entirely. There is no second position in the same name.
5. **Exits.** A name exits when a fill sets its target to 0, or when a fill flips its sign. There is no stop, target, or time stop. A position still open on the last evaluation session is marked to that session's close (or, if the name has no bar that day, to its most recent observed close) with exit reason `sample_end` and with no exit trade and no exit cost. That mark is how the trade list and the profit factor see the last book. It is not a live order.
6. **Fills.** The scheduled fill for a signal is the next SPY-bar session after the signal date, at that session's open. A name with a bar that day is filled then. A name with no bar that day keeps its old shares and is filled at the open of the next session on which it does have a bar, to whatever the target map is at that open (a newer signal, if one has already been computed at a close, has replaced the target). Names that have already filled the current target are not resized on the deferred day. If mark-to-open equity before the trade is less than or equal to 0, every target is set to 0, names with bars are flattened, and the book takes no new risk.
7. **Sizing and accounting.** On a fill session, at the open, before any share change: old shares earn the gap from the previous observed close to today's open (a missing bar earns 0 and does not change the marked price). Equity for sizing is that mark-to-open equity, before cost. For every name filled at this open, set signed shares so that notional at the open equals `target weight × that equity`. Charge cost on the absolute notional of the share change. New shares then earn the open-to-close. A session that is not a fill marks the book the same way with the share change equal to 0. Daily strategy return = close equity / prior close equity − 1. The prior close equity before the first fill is 1.
8. **Costs.** 1 bp of notional per side, paid on the absolute notional of each share change, including a same-side resize. A flip from old shares to new shares of the opposite sign is one share change; its cost equals the cost of closing the old notional plus the cost of opening the new notional, and the trade list splits it that way. No cost is charged on the `sample_end` mark. Cash interest and borrow are zero, as above.
9. **Round trips.** One trip is one name, from the fill where its share sign leaves 0 or flips, until the fill where that sign goes flat or flips again. A same-side resize is the same trip; its cost is part of that trip. A trip still open at the sample end ends with reason `sample_end`, as above. `gross_pnl` is the sum of that trip's gap and open-to-close price P&L in account dollars. `net_pnl` is `gross_pnl` minus the costs assigned to the trip. A trip counts as out-of-sample when its entry fill is on or after 2024-07-01, whether it ends by a fill or by `sample_end`.

Holding-period return of a non-flat rebalance, used only for prediction 2: equity at the next scheduled fill's open, before that next trade, divided by equity at this scheduled fill's open, before this trade, minus 1. The last rebalance uses the final close equity in the numerator. The formation gap is the mean of the three long formation returns minus the mean of the three short formation returns, from the signal that set the weights. Flat signals are excluded. The top half is gap greater than or equal to the median of those gaps over the full evaluation sample; the bottom half is gap strictly below the median. An odd count puts the median month in the top half.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Close on the signal date, and the name's earlier own closes | 16:00 ET on the signal date | The signal at that close. The fill is no earlier than the next session's open |
| Daily bar `ts` of 09:30 ET | Bar open | Not treated as the time the close is known |
| Formation lags | Those earlier sessions' closes | The signal. Never a later close |
| Rank, tie-break, eligibility | The signal close | Target weights for the next fill only |
| Open of the fill session | That open | Sizing and the fill price. The fill session's close is not used to size |
| Marked price of a name with no bar | The most recent observed close, already in the past | That session's mark. It does not enter the signal |
| SPY close-to-close benchmark | Prior close and that session's close | That session's benchmark return only |
| Grid, placebos, quintile bins | The same sessions as the primary | Reported checks. Quintile edges use the full-sample SPY-day distribution and are descriptive. They are not a trading filter |
| Same-bar close fill | The signal close | A labelled upper bound only. Not the primary |

Point-in-time universe: XLRE and XLC cannot be eligible before they have a bar on the signal date and 273 own bars before it. Nothing is normalized on the full sample.

## Samples

- Warm-up: daily history before the first fill. The first signal with six eligible names is 2012-02-29. The first fill is the next SPY session, 2012-03-01. Sessions before that fill are not in the evaluation window and are not in `daily.csv`.
- **In-sample:** the first fill (2012-03-01) through the last SPY session strictly before 2024-07-01 (2024-06-28).
- **Out-of-sample:** 2024-07-01 through 2026-10-01, inclusive.
- Sharpe and total return use that evaluation window only. Inside it, a session on which the book is flat counts as 0. The split is the repo's existing out-of-sample start. It was chosen before any return on these series. The out-of-sample calendar overlaps many equity studies; what those studies already showed is listed under Prior exposure. The split is not moved.
- A signal on 2024-06-28 fills on 2024-07-01. That entry is out-of-sample. The in-sample daily returns stop at the 2024-06-28 close.

## Benchmarks

- SPY close-to-close buy and hold, uncosted, on the same evaluation sessions. The first day's return uses the SPY close of the previous session.
- An equal-weight long-only book of the names eligible on that signal, including a month with fewer than six eligible names. Weight `1/n` each, gross 1. Same signal dates, same next-open fills, same deferred-bar rule, same share accounting, cost 0. If no name is eligible, that benchmark is flat for the month. The strategy's six-name flat rule does not apply to this benchmark.

## Secondary candidates

None. Acceptance line 5 is not applicable: no second sector family is in the store, and a new market is not fetched. Line 5 is recorded here, before the lock, as not applicable. It is not a pass and not a fail. The verdict uses lines 1, 2, 3, 4, and 6.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at base cost, for the strategy and both benchmarks: total return, CAGR on a 252-session year, annualized volatility, Sharpe (mean / sample standard deviation of daily simple returns × √252, zero risk-free rate; 0 if the mean and the standard deviation are both 0), max drawdown of compounded equity, t-stat of the mean daily return, trades, win rate, profit factor (sum of positive trip `net_pnl` divided by the absolute sum of negative trip `net_pnl`; infinite if there are winners and no losers), average net trade in basis points of equity at that trip's entry fill (`net_pnl / entry_equity × 10,000`), time in market, average gross exposure, holding time.
- A trip is assigned to the sample that contains its entry fill. Daily Sharpe uses the daily returns inside the window, which include marks of positions that were entered earlier.
- Breakdowns: calendar year; long versus short; exit reason (`flip`, `flat`, `sample_end`); quintile of the SPY close-to-close move (five rank buckets, quintile 1 = worst SPY days, full evaluation sample).
- Costs: 0, 0.5×, 1×, 2×, and 3× the base 1 bp, by resimulating. Fill delay: the scheduled fill one SPY session later than the primary (signal close, skip one session, fill at the open after that). Upper bound: fill at the signal close. Old shares earn that day's full move; the trade is done at the close; new shares earn from the next session; cost is charged at the close. Labelled upper bound, not the primary.
- Direction placebo: keep each trip's timing and its daily gross dollar price P&L, multiply each trip by an independent ±1, divide the summed dollars by the primary path's prior close equity, and compute the Sharpe of that gross daily return. 2,000 draws, seed `20261011`. Actual gross Sharpe uses the same definition with every sign at +1 (price P&L over prior net equity; the share path is the costed primary path). p = (1 + number of draws with Sharpe ≥ actual) / 2001.
- Timing placebo: 500 draws, seed `20261013`. At each rebalance, permute formation returns across the eligible names (eligible names sorted by symbol, then shuffled) and rebuild the book with the same holding, sizing, and cost rules. Compare gross Sharpe, same definition and same p formula with denominator 501.
- Block bootstrap: circular blocks of 20 sessions on the full-sample net daily strategy returns, 2,000 draws, seed `20261012`. Report the 2.5th and 97.5th percentiles of the Sharpe. Not an acceptance line.
- Plateau grid on the in-sample window: formation lookback in `{126, 189, 252, 315, 378}` sessions, skip 21, far lag = lookback + 21, book fixed at top 3 / bottom 3. Five cells. Out-of-sample Sharpe for each cell is shown for selection bias only, with the rank correlation of the five in-sample Sharpes against the five out-of-sample Sharpes. Nothing is selected from the grid.
- Cross-market: not applicable. No second sector family is in the store.
- Verification: a self-test on synthetic sessions before the store is opened, covering entry, hold, same-side resize, flip, a month with fewer than six names, a missing bar while held, a missing bar on the scheduled fill (deferred fill), the tie-break, and the sample-end mark. `verify.py` is an independent reimplementation, shares no signal code with `backtest.py`, and must match every trade. Seed `20261014` draws at least 40 rebalance dates; if replaying all rebalances is fast, all trades are matched, and that draw is still reported.

Gross price P&L for prediction 1 is the sum of daily long-share price P&L and the sum of daily short-share price P&L over the full evaluation sample, costs excluded. The sums are in account-dollar units starting from equity 1. They are not compounded returns.

## Acceptance

The primary is a **paper-trading candidate** only if every applicable line holds at 1 bp per side:

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0. With five cells, 60% means at least three cells strictly above zero. The primary cell counts as one of the five.
4. Full-sample total return > 0 at 2× base cost (2 bp per side).
5. Not applicable. No second sector family is in the store. This line does not pass or fail the strategy.
6. At least 24 out-of-sample round trips, counting a trip when its entry fill is on or after 2024-07-01, and counting a `sample_end` trip. Below that, the verdict is **Inconclusive**, even if another line also fails.

Line 6 is lower than the protocol's 100 because the rule rebalances monthly, the out-of-sample window has 27 month-ends, and a 100-trip bar would force a higher-frequency rule than the published monthly hold. No other threshold is lowered.

If line 6 is met and any of lines 1, 2, 3, or 4 fails, the verdict is **Rejected**. If lines 1, 2, 3, 4, and 6 all hold, the verdict is **Paper-trading candidate**. Never "ready for live capital."

A failed line fails the strategy. A grid cell, one side, or one sector is not promoted in its place.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone; one sector; a yield filter; a dividend adjustment; a different skip; a different cost; a different sample split; repairing or dropping the 2025-12-05 opens; replacing stored daily closes with 1-minute closes; a volatility target; dropping April 2025 or any other regime.
