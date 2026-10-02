# FX ETF time-series momentum: pre-registered rules

Written 2026-10-02, before any return, P&L, forward return, or signal sign was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `research/fx-etf-momentum/research/counts.py` only. No price was printed and no return, P&L, forward return, or signal sign was computed. It returned:
  - SPY and each of FXE, FXB, FXA, FXC, FXF, FXY: 3,959 daily bars, 2011-01-04 through 2026-10-01, no duplicate dates, no non-positive OHLC, and no high/low violation. Each ETF's dates match SPY's dates exactly (0 missing, 0 extra).
  - `corporate_actions` is empty for all six. Splits: 0. Dividends stored: 0. Split-adjusted and raw opens and closes do not differ (`adjust_differs = 0`).
  - Coverage rows per name: 4,108 recorded, 3,959 with bars. Status counts: complete 4,103, missing 4, partial 1. The partial row is 2021-12-31 (1 bar, note `expected: real session the terminal's calendar calls a holiday; the bar is good`). One missing row is 2025-01-09 (`expected: market closed`, 0 bars). The other three missing rows are 2012-10-29, 2012-10-30, and 2018-12-05 (`no bars`, not labelled expected). `nyse_sessions` over the SPY span has 3,962 dates; the only ones without a SPY bar are those three closures. SPY has no bar that is not an `nyse_sessions` date.
  - Close decimal-place counts (trailing zeros stripped), not a return: FXE 0/1/2/3/4 decimals = 37/348/3388/71/115; FXB 31/324/2990/112/502; FXA 45/298/2811/131/674; FXC 35/290/3184/119/331; FXF 42/322/2766/145/684; FXY 37/379/3366/42/135. These series are not collapsed to a handful of two-decimal prints.
  - A last-stored-bar-of-each-month count was 190 dates, first 2011-01-31, last 2026-10-01. That count treats the sample endpoint as a month-end. This file does not. 2026-10-01 is the first stored session of October, not the last NYSE session of October, and using it as a signal would require knowing that later October bars are absent.
- **Earlier studies on the same instruments, periods, or mechanism.** No study in this repo has read FXE, FXB, FXA, FXC, FXF, or FXY. The mechanism is the same family as `micro-futures-trend` (time-series momentum, monthly rebalance), which did not trade these ETFs.
  - **FX exclusion in `micro-futures-trend`.** That study's `RULES.md` excluded CME FX futures 6E, 6J, 6B, 6A, 6C, and 6S before any return, because the vendor rounds every price to 2 decimals. `6E=F` had 26 distinct closes in 1,260 bars and `6J=F` had 2. The report repeats the exclusion. Silver was excluded separately (thin serial months). This study does not open those futures files and does not use them as a cross-market test. There is no second CurrencyShares set in the store. Line 5 below is therefore not applicable.
  - **What that study found, which makes its 2024–2026 window not unseen.** OOS there was 2024-10-01 → 2026-09-25. The 1/3/12-month futures trend book (equities, rates, and commodities; no FX) returned −7.8% after costs, Sharpe −0.19, profit factor 0.90, 81 trades. In sample −14.8% (Sharpe −0.46). Full sample −21.5% (Sharpe −0.33, max drawdown −33.6%). By calendar year the book was −10.7% in 2024 (Sharpe −0.75), +3.4% in 2025 (0.28), and +1.1% in 2026 through 9/25 (0.18). Long the same volatility-scaled markets returned +27.2%; long ES returned +93.1%. Their rules, written before their run, already recorded general knowledge that managed-futures indices were roughly flat to modestly positive in 2024 and drew down in the first half of 2025 around the April tariff shock, that US equities rose in 2023–2024 and recovered after April 2025, that gold rallied through 2024–2025, that Treasury yields rose into late 2023 and then ranged, and that crude drifted lower over 2023–2025. None of that is an FX ETF return. The 63-session primary is not changed because of it.
  - **Other studies overlap this OOS calendar on equities, not on these ETFs.** From their result paragraphs and `research/README.md`: over roughly 2024-07-01 → 2026-09-25, QQQ buy-and-hold was about +55% (Sharpe near 1.0) and SPY about +42% (Sharpe near 1.0). The window contains the April 2025 equity crash and rebound. Intraday equity momentum (`qqq-intraday-trend`) was a paper-trading candidate on QQQ (OOS Sharpe 0.82) and lost on SPY. Several intraday QQQ fades lost. Those are equity-index paths. They are not the spot path of these six currencies, and they are not a reason to lengthen or shorten the 63-session window.
- **What I already know about the test windows.** The OOS window 2024-07-01 → 2026-10-01 is **not unseen data**. I know the equity and non-FX futures facts in the previous bullets, including that a published time-series-momentum rule lost money on the futures it could trade in the overlapping months. I do not know whether these six ETFs continued or reversed after a 63-session move, and no such return has been computed. The IS window contains the 2011–2024 dollar cycle; I have not measured it on these series.
- **Where the parameters came from.** The hypothesis, the universe, the 63-session lookback, the monthly schedule, the ±1/6 weights, the 5 bp cost, the sample split, and the acceptance edits below were fixed with the study assignment, before this file and before any return on these series. The 63-session length is the short horizon (about three months) at which FX momentum has been documented, not the 12-month equity and commodity specification. It is not to be lengthened to 12 months or replaced by the best cell in the grid. Published sources for the mechanism, not for a fitted window on this store: Moskowitz, Ooi, and Pedersen (2012); Menkhoff, Sarno, Schmeling, and Schrimpf (2012).

## Hypothesis

A currency ETF with a positive 63-session price return continues in that direction over the next month, and one with a negative 63-session price return continues down.

**Mechanism.** This is time-series momentum (Moskowitz, Ooi, and Pedersen 2012) at the short horizon where FX momentum has been documented (Menkhoff, Sarno, Schmeling, and Schrimpf 2012). Each fund is a foreign currency versus the dollar: long the ETF is long the foreign currency, short the ETF is long the dollar against that currency. The other side is a hedger, or a carry trader, leaning against the spot move.

**This is not a carry test and not a total-return test.** No dividends are stored. CurrencyShares distribute foreign interest, so an ex-distribution drop is inside the price and looks like a spot decline. The series can put the signal on the wrong side of the economic return and can book a distribution as a loss. The study does not repair that. A failure, or a pass, is a statement about these contaminated spot prices.

**Known counter-forces.** Sharp reversals of the spot move (momentum crash); carry overwhelming a one- to three-month spot signal when this price series cannot see the interest; whipsaw in a range; the 5 bp cost on a thin ETF; an ex-distribution drop that looks like negative momentum. The report shows the pre-registered breakdowns where those would appear (costs, year, side, quintile of the basket's move). It does not add a repair.

## Predictions beyond P&L

Scored in the report as *consistent*, *not consistent*, or *not testable*. They do not change the verdict and a passed prediction does not promote a variant.

1. Full-sample gross dollar P&L, summed across the primary evaluation window on the actual share path, is strictly positive in at least 4 of the 6 ETFs. A zero is not positive.
2. The horizon claim: the in-sample Sharpe of the 126-session grid cell is not the highest of the five cells. Consistent only if at least one of {21, 42, 63, 84} has a strictly higher IS Sharpe than 126. If 126 is strictly the highest, or tied for the highest, the short-horizon prediction is not consistent. The primary stays 63 either way.

## Data

- **Instruments.** FXE (euro), FXB (sterling), FXA (Australian dollar), FXC (Canadian dollar), FXF (Swiss franc), FXY (yen), read with `agent-data/mdq.py`. All six are in the universe on every date. None is dropped. SPY daily bars define the book calendar only; SPY is not a traded leg and not the benchmark.
- **Bars.** Stored daily bars, `md.bars(sym, "1d")`, default split adjustment. A daily bar's `ts` is 09:30 ET; its close is not known until the session close (16:00 ET, or 13:00 ET on an early close). No coarser bar is built. Early-close sessions are ordinary daily bars: they are not skipped and the fill is still the next session's open.
- **Prices.** There are no stored splits, and adjusted closes match raw closes, so adjustment changes nothing. Prices are as traded. Dividends are not stored and are not imputed.
- **Book calendar.** The sorted session dates of SPY daily bars from 2011-01-04 through 2026-10-01 inclusive. That set skips dates with no bar. In particular it skips 2012-10-29, 2012-10-30 (Hurricane Sandy), and 2018-12-05 (national day of mourning), which `nyse_sessions` includes and on which the store has no bar. It also skips 2025-01-09, which `nyse_sessions` already treats as closed. 2021-12-31 is a real session and stays in the book.
- **Missing bars.** A name with no bar on the book date earns 0 that day. Its last close is not moved forward and is not used as that day's close. Its shares are not changed that day, including on a fill day. The next session that does have a bar marks the gap from the last observed close to the new open (the missed move is recognized when a price exists, not on the missing day, and not dropped). The signal does not forward-fill: no bar on the signal date means that name's signal is 0. The lookback counts that ETF's own daily bars in order, not calendar slots, so a hole is not a bar.
- **Checks the script must pass before it writes results.** Each of the six has 3,959 daily bars, first 2011-01-04, last 2026-10-01, dates identical to SPY, no non-positive OHLC, no stored corporate action, and no adjusted-versus-raw close difference. The three closure dates are not in the book. If any check fails, the script exits without results and without a run-log entry.

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| Universe | FXE, FXB, FXA, FXC, FXF, FXY | Fixed with the study. All six existed before 2011 and all six stay in. |
| `LOOKBACK` | 63 own daily bars | Short FX-momentum horizon, about three months. Not the 12-month equity or commodity specification. Not chosen from these series. |
| `DENOM` | 6 | Count of the frozen universe. Does not shrink when a name is flat or missing. |
| Signal calendar | Last NYSE session of each calendar month, if that session has a SPY bar | Monthly time-series momentum. Defined below so the sample endpoint is not a false month-end. |
| Fill | Next book session's open | Close of day t is known at the close; the open of the next session is the first price after that. |
| Weight | +1/6, −1/6, or 0 | Sign of the lookback return, divided by 6. No volatility target. No rescaling. |
| `COST` | 5 bp of notional traded, per side (rate 0.0005) | CurrencyShares are thinner than SPY. The store has no quotes. Fixed before any P&L. Not changed after the result. |
| Exits | Opposite signal, flat signal, or sample end | No stop, no target, no time stop other than the monthly signal. |

1. **Month-end.** Let `CLOSED = {2012-10-29, 2012-10-30, 2018-12-05}`. For each calendar month from 2011-01 through 2026-10, take `nyse_sessions` from the first through the last calendar day of that month, drop `CLOSED`, and call the last remaining date the month's last NYSE session. It is a signal date only if that date is in the book (it has a SPY daily bar). 2026-10-01 is not a signal date: the last NYSE session of October 2026 is not in the store. A signal date with no later book session would not fill; October 2026 produces neither a signal nor a fill.
2. **Signal.** On signal date t, for each ETF, let the ETF's own daily bars in book order be `c[0], c[1], ...` and let `i` be the index of t. If t has no bar for that ETF, or `i < 63`, the signal is 0. Otherwise `r = c[i] / c[i-63] - 1`. The signal is `+1` if `r > 0`, `−1` if `r < 0`, and `0` if `r == 0` (exact floating-point zero; identical closes are the intended zero). The target weight is `signal / 6`.
3. **Eligible signal.** A signal date is eligible for the primary when its index in the book is at least 63. Earlier month-ends are warm-up. They are not fills and they are not in the evaluation window. The first fill is the book session immediately after the first eligible signal date. That date does not depend on whether the signal is zero.
4. **Position.** One weight per name, updated only at a fill. Shares are constant between fills. A weight of 0 is flat. A flip is one fill from the old share count to the new share count, not two orders. There is no re-entry block: the next month can take the same side, the other side, or flat. Same-sign months stay in the same round trip even though the share count changes with equity.
5. **Fills and accounting.** Start at equity 1, flat, before the first fill. On each later book session the steps are exactly:
   1. Gap. For each name that has a bar today and a nonzero share count, add `old_shares * (open - last_close)`. `last_close` is the last session on which that name had a bar. A name with no bar today adds 0 and does not move `last_close` or shares.
   2. `equity_open = equity_prev + gap`.
   3. If today is not a fill, `new_shares = old_shares` and `cost = 0`. If today is the fill of an eligible signal, then for each name that has a bar, `new_shares = weight * equity_open / open` and `cost += 0.0005 * abs(new_shares - old_shares) * open`. The weight is the one computed at the signal close. It does not use today's open or close. A name with no bar keeps its old shares and adds no cost. Sizing uses `equity_open` before cost is subtracted. A negative `equity_open` is not floored and not reset; the formula is applied as written. A zero `equity_open` sets every traded name's new shares to 0.
   4. `equity_after_cost = equity_open - cost`.
   5. Open-to-close. For each name with a bar, add `new_shares * (close - open)`.
   6. `equity_close = equity_after_cost + open_to_close`. Update `last_close` for names with a bar.
   7. Net return = `equity_close / equity_prev - 1`. Gross dollar P&L = gap + open-to-close (no cost). Gross return = gross dollar P&L / `equity_prev`. The denominator of both returns is the actual net equity, so the share path is the costed path. The zero-cost sweep below is a separate resimulation, not this gross return.
   On a non-fill day the same shares earn the gap and the open-to-close, which is the full move from `last_close` to today's close. Cash and short proceeds earn no interest. There is no borrow fee beyond the 5 bp.
6. **Round trip.** For one ETF, a round trip starts at the fill where shares go from 0 to nonzero, or where the sign flips. It ends at the fill where shares go to 0 or the sign flips. A same-sign share change does not end it. A flip ends one trip and starts another at the same open: the closing trip's exit price and the new trip's entry price are that open. Cost on a flip is `0.0005 * abs(old_shares) * open` on the trip being closed and `0.0005 * abs(new_shares) * open` on the trip being opened (the sum equals `0.0005 * abs(new_shares - old_shares) * open`). A same-sign change puts the whole `0.0005 * abs(new - old) * open` on the continuing trip. Going from flat to a position charges the open leg only. Going from a position to flat charges the close leg only.
   The trip's gross dollar P&L is the sum of the price P&L of its shares while it is open. The gap on a fill day belongs to the old trip. The open-to-close on a fill day belongs to the new shares' trip. On a non-fill day the whole move belongs to the open trip.
   Entry notional = `abs(entry_shares) * entry_open`. It is not updated when a later same-sign fill changes the share count. Gross return = gross dollars / entry notional. Net dollars = gross dollars − costs assigned to the trip. Net return = net dollars / entry notional.
   A trip still open on 2026-10-01 is marked at that session's close, exit reason `end`, with no exit cost. Its open-to-close P&L on 2026-10-01 is included. If that session is also a fill, the fill is done at the open first and the mark happens at the close. Exit reason `flat` is a fill to zero shares. Exit reason `flip` is a fill that changes sign.
7. **Sample end.** The last book session is 2026-10-01. No fill is invented after it.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Close of signal date t, and the close 63 own bars earlier | Session close of t (16:00 ET; 13:00 ET on an early close). The bar `ts` is 09:30 and is not the knowledge time. | Signal at the close of t. Not used to trade on t. |
| Identity of the month-end | NYSE calendar, known before the session. Not inferred from the absence of later bars in the store. | Deciding that t is a signal date. |
| Target weight | Close of t | The next book session's open, or one session later in the delay check. |
| Open of the fill session, and the gap from `last_close` | That session's open | Sizing and the fill, after the gap is added to equity. |
| Open-to-close move | That session's close | P&L after the fill. New shares do not earn it before the fill. |
| Lookback | That ETF's own bars up through t only | The signal. No full-sample mean, z-score, quantile, or volatility target. |
| Universe | All six funds, listed before the sample and unchanged | Every date. |

A signal on session t fills no earlier than the next book session's open. The same-bar close fill in the checks is an upper bound, not the primary.

## Samples

- **Warm-up.** The first 63 book sessions. No eligible signal falls in them.
- **In-sample.** The first primary fill through the last book session strictly before 2024-07-01.
- **Out-of-sample.** Book sessions from 2024-07-01 through 2026-10-01 inclusive.
- **Why this split.** It was fixed with the study, before any return on these series. The OOS block is the recent tail. It overlaps the equity studies' OOS window and the futures-trend OOS window, so it is not unseen; what is already known is listed under Prior exposure. Daily-return metrics use only sessions inside the named window. A session inside the window with a flat book counts as return 0. A round trip is IS if its entry fill is before 2024-07-01 and OOS if its entry fill is on or after 2024-07-01. A trip is not split at the boundary: its whole dollar P&L stays with the window of its entry. Daily Sharpe, total return, CAGR, volatility, drawdown, and the t-stat are not trip statistics and use only daily returns inside the window.
- **Grid cells** use their own lookback as warm-up. They are scored on the primary's IS and OOS dates. A cell that is not yet filled on a date inside that window contributes return 0. A cell that filled before the window keeps the position its own history implies, and the return on a window date is that cell's return, not a cold start. Nothing is selected from the grid.

## Benchmarks

Equal-weight long-only of the same six names: target weight +1/6 each, on the same eligible signal dates and the same next-open fills, same accounting, cost 0. The denominator stays 6. This is a static short-dollar basket, rebalanced monthly back to equal weight because equity and prices move. It is not a momentum signal. Uncosted. Reported on the same sessions. It is not an acceptance line.

## Secondary candidates

None. There is no second FX ETF set in the store, and the FX futures were already judged unusable for price precision (Prior exposure). Line 5 is not a secondary and is not scored.

## Definitions

- **Sharpe.** Mean of the daily simple returns in the window, divided by their sample standard deviation (`ddof=1`), times √252. Risk-free rate is 0. If the sample standard deviation is 0 or the window has fewer than 2 sessions, Sharpe is undefined and any test that needs it fails.
- **Total return.** Compound of the daily simple returns in the window, minus 1. Equivalently, growth of 1 across the window, minus 1.
- **CAGR.** `equity_end ^ (252 / n_sessions) - 1` on a 252-session year. Undefined if ending equity is not positive.
- **Annualized volatility.** Sample standard deviation of daily returns times √252.
- **Max drawdown.** Minimum of `equity / peak - 1` on the compounded window equity, with the peak starting at 1 before the window's first return. A calendar-year drawdown restarts the peak at 1 on that year's first session in the window.
- **t-stat.** Mean daily return divided by (sample standard deviation / √n).
- **Profit factor.** Sum of winning trips' net dollars divided by the absolute sum of losing trips' net dollars. A trip with net dollars 0 is neither. Winning and losing are strict. If the losing sum is 0 and the winning sum is positive, profit factor is infinite and it satisfies ≥ 1.10. If both are 0, it is undefined and does not satisfy the test. Profit factor is dollar-weighted, not the ratio of the mean net returns.
- **Win rate.** Count of trips with net dollars > 0, divided by the count of trips in the window. Unweighted.
- **Average net trade, bp.** Mean of (net dollars / entry notional) × 10,000 over trips in the window.
- **Exposure.** Time in market is the fraction of window sessions with any nonzero shares at the close. Gross exposure is the mean of `sum(abs(shares) * close) / equity_close` over sessions with nonzero equity. Net exposure is the mean of `sum(shares * close) / equity_close`. A name with no bar that day contributes 0 to the sum, not a forward-filled price.
- **Trades per year.** Trip count in the window divided by (`n_sessions / 252`).

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, IS, and OOS at 5 bp per side: the definitions above, plus the benchmark's total return, Sharpe, and max drawdown.
- Breakdowns: calendar year (strategy return, Sharpe, max drawdown, benchmark return); long versus short trips (count, net dollars, profit factor, win rate); exit reason (`flat`, `flip`, `end`); quintile of the benchmark's daily return (five bins, equal count as far as the length allows, remainder to the earliest bins, ties broken by date). A second quintile table uses SPY's close-to-close return on the same sessions (prior close may be the session before the window). Neither quintile is a filter.
- Per ETF, full-sample gross dollars and net dollars on the primary share path, trip count, profit factor, and average net trade in bp. This is the input to prediction 1.
- Costs: resimulations at 0, 0.5×, 1×, 2×, and 3× the 5 bp rate, i.e. 0, 2.5, 5, 10, and 15 bp per side. Shares are resized because equity compounds net of the cost being tested. Report full-sample Sharpe, OOS Sharpe, and full-sample total return.
- Fill delay: the same rule, fill at the open of the book session after the next one (two sessions after the signal). Scored on the primary window; sessions before that variant's first fill are 0. Upper bound, labelled as such and not a verdict input: fill at the signal session's close. Old shares earn that day's full move from `last_close` to the close; sizing uses that pre-cost close equity; cost uses the close; new shares earn nothing on the signal day and earn the subsequent move from that close. A cost paid on a signal date before the primary window is outside the compounded window return. The report says so.
- Direction placebo: on the primary's actual share path, keep each trip's dates and multiply that trip's gross dollar pieces by an independent ±1. 2,000 draws, seed 20261031. Gross return of a draw uses the actual prior-close net equity as the denominator. Compare the draw's gross Sharpe with the actual gross Sharpe. `p = (1 + count of draws whose gross Sharpe is greater than or equal to the actual) / 2001`. An undefined draw Sharpe counts as 0 in that comparison.
- Timing placebo: 500 draws, seed 20261033. On the eligible signal dates, permute each ETF's own signal in {+1, −1, 0} across those dates, preserving how many of each sign it had. Names are permuted in the order FXE, FXB, FXA, FXC, FXF, FXY, from one generator. Rebuild the book at cost 0. The statistic is that book's full-window Sharpe. The comparison value is the primary rebuilt at cost 0, not the gross-on-net-equity Sharpe from the direction placebo. `p` is computed the same way. This p is reported and is not an acceptance line.
- Block bootstrap: circular, 20-session blocks, 2,000 draws, seed 20261032, of the primary's full-window net daily returns. Report the 2.5th and 97.5th percentiles of the Sharpe and the share of draws with Sharpe ≤ 0.
- Plateau grid, IS only for the verdict: lookback in {21, 42, 63, 84, 126}, weights unchanged at ±1/6, cost 5 bp. Five cells. OOS Sharpe of each cell is shown for selection bias only, with the rank correlation of the five IS Sharpes against the five OOS Sharpes. Nothing is selected.
- Cross-market: not applicable. Recorded here, before the lock, for the reason in Prior exposure.
- Verification: the self-test below, then `verify.py`, which shares no signal code with `backtest.py`. It checks every round trip, which is more than 40, and it also draws a seeded sample of 40 eligible signal dates (seed 20261034) and checks every trip whose entry fill is the fill of one of those dates.

### Self-test, before the store is opened

Synthetic sessions, hand-computed expected equity and trips. The engine is called with a short lookback so the arithmetic fits; the production constants are asserted equal to 63, 6, and 5 bp. The cases are: a positive signal; a negative signal; an exact zero lookback return with a different intermediate close; a flat name that does not rescale the others off 1/6; a missing bar that earns 0 and is not forward-filled into the next signal; a same-sign resize that stays one trip; a flip that is two trips with the cost split; a sample-end mark with no exit cost; sizing off the mark-to-open equity before cost, with old shares earning the gap. The script aborts if any case fails and does not open the store.

## Acceptance

The primary is a **paper-trading candidate** only if every scored line holds at 5 bp per side. Line 5 is not scored.

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full evaluation window.
3. IS Sharpe > 0, and at least 60% of the IS grid cells have Sharpe > 0. That is at least 3 of the 5 cells. The primary cell counts.
4. Full-sample total return > 0 at 2× base cost, i.e. 10 bp per side, on the resimulation.
5. Not applicable. No second FX ETF universe is in the store. CME FX futures were excluded by `micro-futures-trend` before any return because two-decimal prices made them unusable, and this study does not read those files. A missing line 5 is not a failure.
6. At least 24 OOS round trips, counted as trips whose entry fill is on or after 2024-07-01. A round trip is one ETF from entry until flat or flip, as defined above. The protocol default of 100 OOS trades is lowered, before the lock, because the holding period is one month and the OOS window is 27 months: 100 round trips is not a sample this rule can produce. 24 is about one round trip per ETF per year. No other line is lowered.

Status, applied in this order:

- If line 6 fails, the status is **Inconclusive**, even if lines 1–4 also fail.
- If line 6 holds and any of lines 1–4 fails, the status is **Rejected**.
- If lines 1, 2, 3, 4, and 6 all hold, the status is **Paper-trading candidate**.
- **Void** only if an implementation or data defect cannot be fixed without changing these rules.

A failed scored line fails the strategy. The predictions are not lines.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell, including 21, 126, or 252 sessions; the long side or the short side alone; a single currency; a dollar-up or dollar-down filter; a carry rank or any interest-rate sort; a cross-sectional rank across the six; a dividend or distribution repair; dropping a currency; rescaling weights when some signals are zero; a volatility target; a different cost; a different fill; moving the IS/OOS date; treating 2026-10-01 as a month-end. A variant suggested by the results goes under *Ideas for a new study* and needs its own rules and data this study did not use.

## Seeds

| Use | Seed |
|---|---|
| Direction placebo | 20261031 |
| Block bootstrap | 20261032 |
| Timing placebo | 20261033 |
| Verify sample | 20261034 |

20261032 and 20261033 are seeds, not calendar dates. NumPy's `default_rng` is the generator.

## References

- Moskowitz, T. J., Ooi, Y. H., & Pedersen, L. H. (2012). Time series momentum. *Journal of Financial Economics*, 104(2), 228–250.
- Menkhoff, L., Sarno, L., Schmeling, M., & Schrimpf, A. (2012). Currency momentum strategies. *Journal of Financial Economics*, 106(3), 660–684.
