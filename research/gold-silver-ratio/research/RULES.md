# Gold/silver ratio: pre-registered rules

Written 2026-10-02, before any ratio, z-score, return, P&L, forward return, or hit rate was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `research/gold-silver-ratio/research/counts.py` (output `counts.json`), read through `agent-data/mdq.py`. Bar counts, coverage, corporate actions, and one split-continuity ratio. No GLD/SLV ratio, no z-score, no threshold-crossing count, no return, no P&L.
  - GLD and SLV each have 3,959 daily bars, first session 2011-01-04, last session 2026-10-01. No duplicate sessions, no non-positive OHLC, no zero-volume bars. Their session sets are identical. PPLT has 3,960 daily bars, first 2011-01-04, last 2026-10-02, and 3,959 bars through 2026-10-01. Through 2026-10-01 the PPLT session set equals the GLD session set. The 2026-10-02 PPLT bar is not used.
  - `nyse_sessions` from 2011-01-04 through 2026-10-01 has 3,962 sessions and includes 2012-10-29, 2012-10-30, and 2018-12-05. Those three have no bars. Book sessions are the 3,959 dates that remain. That list equals the GLD session list. 2024-06-28 and 2024-07-01 are both GLD sessions.
  - Coverage, daily: GLD and SLV each have 4,108 rows (4,103 complete, 4 missing, 1 partial). PPLT has 4,109 rows (4,104 complete, 4 missing, 1 partial) because of 2026-10-02. `needs_attention` on each name is only the three closures above, each `missing` with note `no bars: re-run ingest for this session`. The other missing row and the partial row are the rows mdq marks `expected`. The known-issues table identifies those as 2025-01-09 (already outside `nyse_sessions`) and 2021-12-31 (a real daily bar). 2021-12-31 stays in the book.
  - Corporate actions: GLD none. SLV none. PPLT one split, ratio 10.0, ex-date 2026-05-18, source `mboum`. No dividend row is stored on any of the three.
  - Split continuity, ex-date close divided by the previous session's close. This is a jump check, not a result. Previous session 2026-05-15. Raw close ratio 0.1005. Adjusted close ratio 1.005. The day before that, both the raw and the adjusted close ratio are 0.959. The adjusted series does not jump by about 10×. The raw series does, down by about 10×, which is the 10-for-1 split. `backtest.py` re-checks raw ratio < 0.2 and adjusted ratio between 0.5 and 1.5, and aborts if that fails. It does not target the residual ratio.
- **Earlier studies on the same instruments, periods, or mechanism.** `commodity-etf-momentum` (Inconclusive, locked 2026-10-02) traded GLD and SLV. It ranked six commodity ETFs on a 12-month return and held the top two long and the bottom two short. It did not trade the gold/silver ratio, and this study does not rank GLD and SLV on past returns. Its evaluation window is 2012-02-01 through 2026-10-01 and its OOS window is 2024-07-01 through 2026-10-01, the same OOS dates used here. That OOS window is **not unseen data**. No study in this repo has used PPLT, and none has traded a gold/silver ratio. `micro-futures-trend` (Rejected) traded gold futures, not GLD. Equity mean-reversion studies in the repo (RSI(2), Bollinger, opening fades) did not use these ETFs.
- **What I already know about the test windows.** From `commodity-etf-momentum`'s report, read before any ratio was computed here. Out of sample that book returned +41.5% after costs (Sharpe 0.574, profit factor 2.26, max drawdown −37.0%, 21 round trips, 566 sessions) and missed its own sample floor of 24, so the status is Inconclusive. The uncosted equal-weight book of its six ETFs returned +47.0% OOS (Sharpe 0.967, max drawdown −15.4%). Full sample, that strategy returned +95.2% (Sharpe 0.310). GLD's own-trade net P&L was +0.744 and SLV's was +0.629, the two large positive names. Leave-one-out gross P&L stayed positive when GLD was removed (+0.290) and when SLV was removed (+0.348). Of the OOS-entry trades, GLD was +0.472 on 1 trade and SLV was +0.488 on 2. SLV was long from 2025-07-01 into the sample flatten (net +0.431). No GLD position was open at that flatten. Calendar-year equal-weight returns in that report: 2022 +11.8%, 2023 −11.6%, 2024 +15.3%, 2025 +23.8%, 2026 through 2026-10-01 +14.8%. The strategy's own year returns were 2022 +26.0%, 2023 −20.7%, 2024 −2.5%, 2025 +43.7%, 2026 +3.4%. That report, citing `micro-futures-trend`, already stated that gold rallied through 2024–2025 and that crude drifted lower over 2023–2025. The futures gold sleeve in that earlier study was negative in the full sample and positive out of sample. Those are not this rule. I know both GLD and SLV were strong enough, on a 12-month rank, to be held long during parts of this OOS window. I do not know which one outperformed the other, and I have not computed the ratio. Equity studies used an OOS start of 2024-07-01. From the commodity study's account of those reports, QQQ buy-and-hold was about +55% in a window that contains the April 2025 tariff crash and rebound. Those are not metal-ETF results.
- **Where the parameters came from.** The entry at two standard deviations and the exit when the spread crosses its mean are the pairs rule in Gatev, Goetzmann, and Rouwenhorst (2006). They are used here on the raw close ratio, not on their normalized price distance, and not with their 12-month formation and 6-month trading period. The 60-session window, the population divisor (60, not 59), the weights ±0.5, the 1 bp cost, the 2024-07-01 split, the OOS floor of 15, and the GLD/PPLT cross-market were fixed in the study request before any ratio on these series was computed. They are not to be changed after the run. The request said not to change the 60-session, 2-standard-deviation, exit-at-zero rule after reading the commodity study. It is not changed.

## Hypothesis

The split-adjusted GLD/SLV close ratio mean-reverts, so a dollar-neutral pair entered when the ratio is more than two standard deviations from its trailing 60-session mean, and exited when the ratio crosses that mean, has a positive return after costs.

**Mechanism.** Gold and silver share a monetary-metal factor. Escribano and Granger (1998) found a long-run relationship that is not stable across the whole sample, and a strong simultaneous link between the two returns. Lucey and Tully (2006) found that the link weakens in places and still prevails over a long sample. The residual between them is temporary hedging and relative-value flow. The other side is a trader who extrapolates a widening gold/silver gap. Gatev, Goetzmann, and Rouwenhorst (2006) describe pairs profits as coming from temporary mispricing of close substitutes that share a common factor. This is not commodity momentum and not a trend rule.

**Known counter-forces.** Escribano and Granger also found that the out-of-sample link was weaker, and that silver's predictive model failed out of sample. A 60-session band can mistake a permanent shift in the ratio for a temporary one. One metal's own drift can be the whole dollar result of a "pair." The ratio can trend for years when one metal's industrial demand dominates. None of these is a reason to drop a leg, change the window, or change the band after the run.

## Predictions beyond P&L

If the mechanism is right, then, on the primary GLD/SLV book:

1. Full-sample gross price P&L, summed over trades, is positive on the `short_ratio` side (the z > 2 entry) and positive on the `long_ratio` side (the z < −2 entry). Gross means price P&L before costs.
2. Full-sample gross price P&L stays positive if the gold leg's dollar P&L is removed and the silver leg is kept, and stays positive if the silver leg's dollar P&L is removed and the gold leg is kept. This is the sum of each leg's price P&L on the trades the primary book actually held. It is not a new backtest with one leg deleted, and it does not change the trades. If one metal is the whole result, the ratio prediction is not consistent.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. Neither prediction can be promoted to the primary.

## Data

- Instruments: GLD and SLV for the primary. GLD and PPLT for the cross-market, under the same rule. PPLT does not replace GLD/SLV. Read with `agent-data/mdq.py`, daily bars, split-adjusted (the default). Dividends are not stored and are not invented. Metal ETFs are the price.
- Prices are adjusted-share prices. Share counts live in that same adjusted space. The engine does not multiply shares by a split ratio on an ex-date. Doing so on an already adjusted series would invent a jump.
- The ratio on a pair session is the adjusted close of the first leg divided by the adjusted close of the second leg. Primary: GLD/SLV. Cross-market: GLD/PPLT. The last session used is 2026-10-01. The PPLT bar on 2026-10-02 is not loaded into the cross-market.
- Calendar: book sessions are `mdq.nyse_sessions` from 2011-01-04 through 2026-10-01, minus 2012-10-29, 2012-10-30, and 2018-12-05. Those three are skipped. They are not zero-return rows. 2025-01-09 is already outside `nyse_sessions`. 2021-12-31 stays, because it has a real daily bar. Early closes have a normal daily bar. The signal uses that bar's close, which is known at 13:00 ET, and needs no separate rule. A daily bar's `ts` is 09:30 ET. Its close is known only at the session close (16:00 ET, or 13:00 ET on an early close).
- A pair session is a book session on which both legs have a bar. The ratio series is those sessions only, in order. A hole is not a ratio point and is not forward-filled.
- A book session that is missing either leg earns 0 for the book that day. Marks stay at the last real close. No fill happens. A pending order waits for the next pair session. The next pair session's gap is from that last real close to the new open, so the held shares earn the reopening jump on the session that has bars. The hole day itself contributes 0.
- Checks the script must pass before it writes results: GLD and SLV each have 3,959 daily bars, first 2011-01-04, last 2026-10-01; PPLT has 3,960 daily bars and its last is 2026-10-02; through 2026-10-01 all three session sets are identical; no duplicate sessions; no non-positive OHLC; GLD and SLV have no corporate actions; PPLT has only the 10.0 split on 2026-05-18; on that ex-date the raw close ratio versus 2026-05-15 is < 0.2 and the adjusted close ratio is between 0.5 and 1.5; the book has 3,959 sessions and does not contain the three skipped closures.

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| Pair | GLD, SLV | Named in the request. Not a sort of a larger universe |
| Ratio | Adjusted GLD close / adjusted SLV close | The price ratio. Not a regression residual and not a 12-month return rank |
| Window | 60 ratio points, ending at t, inclusive | Fixed in the request before any ratio was computed. Not Gatev's 12-month formation. Not to be changed after the commodity study |
| Mean | Arithmetic mean of those 60 | The trailing mean the exit uses |
| Sigma | Population standard deviation, divide by 60, not 59 | Fixed in the request so two implementations match. Not fitted |
| Entry | z > 2 or z < −2 | Gatev, Goetzmann, and Rouwenhorst (2006), strict, on this z |
| Exit | Cross the mean: z ≤ 0 or z ≥ 0, by side, as below | The same paper's revert-to-the-mean exit. The threshold is 0, not a second fitted band |
| Weights | −0.5 and +0.5, or the reverse | Gross long 0.5, gross short 0.5, net 0 at the sizing instant. Fixed in the request |
| Cost | 1 bp per side on GLD, SLV, and PPLT | Skill default for SPY-class liquidity. GLD and SLV are in that class. PPLT uses the same prior so the cross-market is the same rule. Not a measured spread. Not to be changed after P&L |
| Starting equity | 1.0 | Unit book. Fractional shares. No borrow fee and no cash interest. The store has no borrow quotes. Charging zero borrow is a limitation, stated here |

1. **Ratio and z.** On each pair session t that has at least 60 ratio points ending at t, let those points be x_1 … x_60 with x_60 = R_t. mu = (1/60) × Σ x_i. sigma = sqrt( (1/60) × Σ (x_i − mu)^2 ). If sigma = 0, z = 0. Otherwise z = (R_t − mu) / sigma. Sigma = 0 is the exact floating-point zero. There is no epsilon. The first 59 ratio points have no z. They are warm-up.
2. **State.** One pair position at a time: `flat`, `short_ratio`, or `long_ratio`. Do not add to an open trade. Shares are constant between fills.
   - `short_ratio` is the z > 2 side: weight −0.5 GLD and +0.5 SLV. The position is short the rich ratio.
   - `long_ratio` is the z < −2 side: weight +0.5 GLD and −0.5 SLV.
   - `flat` is zero shares in both.
3. **Desired position, evaluated at the pair session's close.** Let the entry threshold be E. The primary uses E = 2. The grid changes E and changes nothing else.
   - From `flat`: if z > E, desired is `short_ratio`. Else if z < −E, desired is `long_ratio`. Else `flat`.
   - From `short_ratio`: if z < −E, desired is `long_ratio` (flip). Else if z ≤ 0, desired is `flat`. Else stay `short_ratio`.
   - From `long_ratio`: if z > E, desired is `short_ratio` (flip). Else if z ≥ 0, desired is `flat`. Else stay `long_ratio`.
   - The flip test is first. A z that satisfies the opposite entry also satisfies the exit inequality. The position flips. It does not go flat and wait.
4. **Orders.** If desired differs from the position already on from today's open, that desired position replaces any pending order. The signal session is this close. If desired equals the current position, any pending order is cancelled. A later close with the same desired side does not reset the signal session. A later close with a different desired side replaces the order and resets the signal session.
5. **Fills.** The primary delay is 1. The order fills at the open of the pair session that is `delay` pair-sessions after the signal session. Delay 1 is the next pair session. A session missing either leg does not count and does not fill. The fill price is that session's open on each leg. There is no stop, no profit target, and no daily rebalance back to the weights. If the signal session is the last book session, or the required later pair session does not exist, the order is not filled.
6. **Accounting on a book session.**
   - A session missing either leg earns 0. Cash, shares, and last closes stay as they are.
   - Otherwise old shares earn the gap: for each leg, `old_shares × (open − last_close)`. A leg with no previous close, or with zero shares, has gap 0.
   - Mark-to-open equity is cash plus old shares marked at today's opens. Every fill on this session is sized off this one equity number, before today's costs.
   - If equity_open is not strictly positive, do not open a new position. Flatten any old shares at the opens, charge cost, and reason the closed trade `flat`. This is the sizing identity, not a stop chosen after a loss. It is reported if it happens.
   - If a pending order fills and equity_open > 0, new shares in a leg are `weight × equity_open / open`. Cash decreases by `delta_shares × open + cost` on each leg. Cost is `abs(delta_shares) × open × rate`. The rate is 1 bp × the cost multiple / 10,000. The primary multiple is 1. Both legs use the same equity_open.
   - New shares then earn the open-to-close: `new_shares × (close − open)`. The mark becomes today's close.
   - A day with no fill and both bars is old shares earning close-to-close, because old and new shares are equal.
7. **Terminal exit.** The last book session is 2026-10-01. After that session's open fill, if any, and after open-to-close, any shares still held are flattened at that session's closes. Cost is charged on the absolute notional. There is no further price P&L, because the exit price is the mark. Exit reason `end`. A signal whose required later open does not exist is not filled. 2026-10-01's own close can still generate a desired position. That order is discarded. The flatten is not a fill of that order. It is the end of the sample, and it is the only close fill in the primary.
8. **What is not charged.** No borrow, no commission beyond the bp rate, no cash interest. A flip pays one cost on each leg's absolute share change. That cost equals the notional closed plus the notional opened, times the rate, when the old and new share signs differ or either is zero. The cost on a leg is split between the trade that is closing and the trade that is opening in proportion to those two notionals. Opening from flat puts the whole cost on the new trade. Closing to flat puts the whole cost on the old trade. The terminal flatten's cost goes to the trade it closes.

## Trade identity

A round trip is one pair, one side, from the fill that opens that side until the fill that sets the side to `flat` or to the opposite side, or until the terminal flatten.

- Exit reasons are `flat`, `flip`, and `end`.
- The gap into an exit open belongs to the trade that is closing. The open-to-close after a fill belongs to the trade that holds the new shares. On the terminal day the open-to-close belongs to the trade held after the open fill, and the flatten cost belongs to that same trade if the flatten closes it.
- Trade gross P&L is the price P&L of both legs attributed to the trade. Trade net P&L is gross minus the costs attributed to it. Each leg's gross P&L is stored on its own.
- Return in bp is `net_pnl / entry_gross_notional × 10,000`. Entry gross notional is `abs(gld_shares) × gld_entry_price + abs(slv_shares) × slv_entry_price`, using the second leg's name on the cross-market.
- Entry prices are the fill opens. Exit prices are the exit opens for `flat` and `flip`, and the exit closes for `end`.
- A trade is in-sample if its entry fill is before 2024-07-01, and out-of-sample if its entry fill is on or after 2024-07-01. The whole trade is in one sample even when its daily P&L crosses the boundary. Daily Sharpe uses the session date, not the trade's entry date. Both facts are reported.
- Holding time, in pair sessions: for `flat` and `flip`, exit index minus entry index. For `end`, the exit session is included, so the count is last index minus entry index plus 1. A same-session enter-and-end trade has holding time 1.
- Line 6 uses a narrower count than the trade list. It counts out-of-sample trades whose exit reason is `flat` or `flip` only. A trade that is still on at the sample end has not finished the round trip the request defined ("one pair entry until flat or flip"). `end` trades stay in the trade list, in profit factor, in win rate, and in the daily path.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| R_t and the 60 closes in the window | The session-t close (16:00 ET, or 13:00 ET on an early close). Every close in the window is on or before t | z at that close. The fill is a later open |
| mu and sigma | That same close. The window is the last 60 ratio points, not the full sample | The same z |
| Mark-to-open equity | The fill session's open | Sizing of fills at that open, before the trade |
| Fill price | That session's open on each leg | The fill |
| Terminal exit price | The last session's close, after open-to-close has been earned | Flatten after that close, because no later open exists |
| Costs, window, E, weights, universe | Constants, fixed in this file | Everywhere |

No statistic is estimated on the full sample. The z-score uses only the trailing 60 ratio points. A primary fill never uses the signal session's close as the execution price. The same-bar close fill below is a labelled upper bound and is not the primary.

## Samples

- **Warm-up.** The first 60 ratio points. No z exists before the 60th. No P&L is recorded before the first fill.
- **In-sample.** The first fill through 2024-06-28, the last book session before 2024-07-01.
- **Out-of-sample.** 2024-07-01 through 2026-10-01.
- **Which days enter the Sharpe.** Evaluation sessions only: the first fill through 2026-10-01. A day inside that window with a flat position has return 0 and stays in the mean and the standard deviation. Warm-up days, and pair sessions after warm-up but before the first fill, do not. The three skipped closures are not in the window.
- **Why this split.** 2024-07-01 is the repo's existing OOS start. It was fixed in the request before any ratio was computed. The window is the one `commodity-etf-momentum` already reported. It is not unseen. Grid cells use the same boundary. A cell whose own first fill is after 2024-06-28 has an empty in-sample. A cell whose own first fill is after 2024-07-01 starts its out-of-sample at that fill.

## Benchmarks

Two uncosted buy-and-hold books, GLD and SLV, on the primary's evaluation sessions. Each starts at equity 1, buys weight +1 at the first evaluation session's open, and holds through the last close. The first day's return is open to close. Later days are close to close, using the previous book session's close. No cost and no terminal flatten cost. They are not a second primary, and the verdict does not require beating them. The cross-market does not use a separate benchmark for acceptance.

## Secondary candidates

None. There is no second primary.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, IS, and OOS at base cost, for the primary and both benchmarks: total return; CAGR on a 252-session year; annualized volatility (sample SD × √252); Sharpe (mean ÷ sample SD × √252, sample SD with delta degrees of freedom 1, zero risk-free rate); max drawdown of equity compounded from 1 over that window's daily returns only, with the peak seeded at 1; t-stat of the mean daily return; trades; win rate; profit factor (sum of positive net trade P&L ÷ absolute sum of negative net trade P&L); average net trade in bp; exposure. Exposure is the fraction of window sessions on which the side after the open's fills, and before a terminal flatten, is not `flat`. A trade with net P&L of 0 is not a win and not a loss. Win rate is the count of net P&L > 0 divided by the count of trades, so a zero stays in the denominator. If there are no losing trades, the profit factor is null and line 1 fails. If the sample standard deviation is 0 or the window has fewer than 2 sessions, Sharpe is null. A null OOS Sharpe fails line 1. A null IS Sharpe fails line 3. A null cross-market OOS Sharpe fails line 5.
- Gross Sharpe, full, IS, and OOS: that session's price P&L, before costs, divided by the previous close's net equity of the costed book. The first evaluation session uses previous equity 1. Costs stay in the denominator. They are not in the numerator.
- Breakdowns: calendar year (compounded return, Sharpe, max drawdown, both benchmark returns); `long_ratio` versus `short_ratio`; exit reason (`flat`, `flip`, `end`); quintile of the GLD close-to-close simple return on evaluation sessions. The first evaluation session's GLD move uses the previous book session's GLD close. Sessions are sorted by that GLD return ascending, ties by date ascending. Session i in that list, from 0, is in quintile `(i × 5) // n + 1`. Quintile 1 is the worst GLD sessions. The table reports the mean strategy net return and the mean GLD return in each quintile.
- Costs: multiples 0, 0.5, 1, 2, and 3 of the 1 bp rate. Each multiple is a full resimulation. Report full-sample Sharpe, OOS Sharpe, and full-sample total return.
- Fill delay: delay = 2, so the fill is the second pair session after the signal. Same sizing at that later open. Report full and OOS Sharpe and full return.
- Same-bar close fill, labelled upper bound, not a verdict input: after old shares have earned that session's open-to-close, rebalance at the close off mark-to-close equity before cost. New shares earn nothing more that day. The last session still discards nothing from this upper bound except the rule below. After the last session's close rebalance, any shares still held are flattened at that same close, with cost, and that closing trade's reason is `end`. A same-price enter-and-flatten has no price P&L and pays both costs. This path is not the primary.
- Direction placebo: each completed trade keeps its timing. Its daily gross price P&L is multiplied by an independent ±1, one sign per trade per draw. 2,000 draws, seed 20261081. Daily gross return uses the actual primary book's previous net equity as the denominator. Compare the draw's gross Sharpe with the actual full-sample gross Sharpe. p = (1 + number of draws with Sharpe ≥ actual) / 2001.
- Timing placebo: 500 accepted draws, seed 20261083. Pair sessions are indexed in order. Each trade has entry index i and an exclusive end index j: the exit-open index for `flat` and `flip`, and one past the last holding session for `end`. Holding length is j − i. A draw adds an independent integer shift, uniform on {1, 2, …, 60}, to every trade's i and j. The draw is rejected, and not counted among the 500, if any shifted end index runs past the last pair session, if any two shifted half-open intervals [i, j) overlap, or if the shifted entry order reverses. Rejected draws are redrawn until 500 are accepted or 200,000 attempts have been made, whichever comes first. An accepted draw resimulates those holdings at cost 0, sizing ±0.5 off that draw's own mark-to-open equity, with the same gap and open-to-close accounting, and with an `end` exit still filled at the close. The score is that path's Sharpe on the primary evaluation sessions (a session before the shifted book is in the market, but inside the window, is 0). p = (1 + number of accepted draws with Sharpe ≥ the actual full-sample gross Sharpe) / 501. If 500 accepted draws are not reached, the check is not testable, p is null, and the attempt count is reported. This check is not an acceptance line. Trades with reason `end` are shifted with the others. They are not dropped if that makes the check not testable.
- Block bootstrap: circular blocks of 20 evaluation sessions, 2,000 draws, seed 20261082, of the full-sample net daily returns. If the length is not a multiple of 20, draw enough blocks to cover it and truncate to the original length. A block that passes the end wraps to the start. Report the 2.5 and 97.5 percentiles of the draw Sharpes, and the median.
- Plateau grid, IS only for the acceptance line: entry threshold E ∈ {1, 1.5, 2, 2.5, 3}. Window fixed at 60. Exit remains 0. Five cells. Each cell is a full resimulation. Its IS window is that cell's first fill through 2024-06-28. OOS is shown for selection bias only. Nothing is selected from the grid. The E = 2 cell's IS Sharpe must equal the primary IS Sharpe, or the script aborts as a bug.
- Cross-market: the identical rule on GLD and PPLT, ratio GLD/PPLT, E = 2, window 60, exit 0, weights ±0.5, 1 bp, same dates, same terminal rule. PPLT does not replace the primary. Report IS Sharpe, OOS Sharpe, full total return, and full profit factor.
- Verification: a self-test on synthetic sessions before the store is opened. It covers an entry on z > 2, an exit at z ≤ 0, an entry on z < −2, a flip, a zero-sigma window, a missing bar, and the last session. The analytic check is a window of 59 ones and one 2: z at the spike equals sqrt(59), and z on the following 1, in a window that still contains that single 2 and 59 ones, equals −1/sqrt(59). `backtest.py` runs this self-test before it constructs `MarketData`. A failure exits before any store read and writes no `RUNLOG` line. `verify.py` shares no signal code with `backtest.py`. It recomputes the primary book and must match every round trip on side, entry date, exit date, both entry prices, both exit prices, and exit reason. Prices and gross and net P&L match to 1e-6 absolute. Seed 20261084 draws 40 trades when the list is longer than 40. The pass condition is the full trade list, which contains any such draw. It also recomputes the cross-market trade list and matches it to the stored cross-market trades on the same fields.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at base cost (1 bp per side):

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. IS Sharpe > 0, and at least 60% of the IS grid cells have Sharpe > 0. Five cells, so at least 3. A null IS Sharpe is not > 0.
4. Full-sample total return > 0 at 2× base cost.
5. OOS Sharpe > 0 on the GLD/PPLT cross-market under the identical rule.
6. At least 15 OOS round trips, counted as defined above: entry fill on or after 2024-07-01, and exit reason `flat` or `flip`. Below that, the verdict is **Inconclusive**, and the other lines are still reported.

Reason for lowering the protocol's 100-trade floor, and only this line: a 2-standard-deviation band on a 60-session ratio is a low-frequency rule, and a 100-trip bar would force a tighter band than the one fixed here. The floor is not to be lowered further after seeing the count. No other line is lowered.

**Verdict map, fixed here.** If line 6 is not met, the status is **Inconclusive**. If line 6 is met and any of lines 1–5 fails, the status is **Rejected**. If lines 1, 2, 3, 4, 5, and 6 all pass, the status is **Paper-trading candidate**.

A failed line fails the strategy. A grid cell, one side, one metal, or the cross-market does not replace the primary.

## Not done in this study

The report will not promote any of these in place of the primary:

- trading USO, or the six-ETF momentum book;
- ranking GLD and SLV on past returns, or adding a trend overlay;
- a different exit, a different window, or a sample divisor of 59;
- dropping a metal, or keeping only the z > 2 side or only the z < −2 side;
- using PPLT as the primary;
- moving the sample split;
- a grid cell other than E = 2;
- a different cost, a different fill, or the same-bar close upper bound;
- daily rebalance back to the 0.5 weights, a stop, or volatility scaling.
