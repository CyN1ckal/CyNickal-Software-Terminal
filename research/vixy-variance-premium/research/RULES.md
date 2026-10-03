# VIXY variance premium: pre-registered rules

Written 2026-10-02, before any strategy return, P&L, forward return, or hit rate was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `research/vixy-variance-premium/research/counts.py`, output `counts.json`. Coverage, corporate actions, split-jump ratios, and counts of month-end dates. No strategy return or P&L. Schema `user_version` 6. 191 instruments.
  - VIXY: 3,960 daily bars, 2011-01-04 through 2026-10-02. One of those bars is 2026-10-02. Sessions on or before 2026-10-01: 3,959. No duplicate sessions, no non-positive OHLC, no high/low violations, no zero-volume bars. Splits, and only splits: 0.25 on 2017-07-17, 0.25 on 2021-05-26, 0.2 on 2023-06-23, 0.25 on 2024-11-07. No dividends.
  - UVXY: 3,771 daily bars, 2011-10-04 through 2026-10-02, of which 3,770 are on or before 2026-10-01. Same bar-quality checks, all clean. Splits, and only splits: 0.2 on 2017-01-12, 0.25 on 2017-07-17, 0.2 on 2018-09-18, 0.1 on 2021-05-26, 0.1 on 2023-06-23, 0.2 on 2024-04-11, 0.2 on 2025-11-20. No dividends.
  - SPY: 3,959 daily bars, 2011-01-04 through 2026-10-01. No bar on 2026-10-02. No corporate actions. SPY has 1-minute bars. VIXY and UVXY have 0 one-minute bars.
  - Coverage for each name: the three NYSE closures 2012-10-29, 2012-10-30, and 2018-12-05 are `missing` with 0 bars and are inside `nyse_sessions`. 2025-01-09 is `missing` with note `expected: market closed` and is already outside `nyse_sessions`. 2021-12-31 is `partial` with one real daily bar, note `expected: real session the terminal's calendar calls a holiday; the bar is good`. It is kept. Inside each product's span through 2026-10-01, NYSE sessions excluding those three closures match the daily bars exactly (0 missing).
  - Split jump check, ex-date close divided by the previous session's close. This is a corporate-action check, not a result. VIXY adjusted ratios were 0.973, 0.953, 1.012, and 0.969 against raw ratios 3.893, 3.812, 5.059, and 3.878 (reverse multiples 4, 4, 5, and 4). UVXY adjusted ratios were 0.997, 0.948, 0.996, 0.932, 1.028, 0.975, and 1.129 against raw ratios 4.987, 3.792, 4.981, 9.320, 10.278, 4.874, and 5.645 (reverse multiples 5, 4, 5, 10, 10, 5, and 5). The adjusted series does not contain the reverse-split jump. `backtest.py` re-checks raw ratio > 3 and adjusted ratio between 0.5 and 1.5 and aborts if that fails. It does not target the residual ratio.
  - Date counts, not outcomes. VIXY month-end signals on or before 2026-10-01: 189, first 2011-01-31, last 2026-09-30, 189 next-open fills, first fill 2011-02-01, last fill 2026-10-01, 28 of those fills on or after 2024-07-01. UVXY: 180 signals, first 2011-10-31, last 2026-09-30, first fill 2011-11-01, last fill 2026-10-01, 28 fills on or after 2024-07-01. October 2026's last NYSE session is 2026-10-30, so 2026-10-01 is not a month-end. The 2026-10-02 bar is not used.
- **Earlier studies on the same instruments, periods, or mechanism:** No study in this repo used VIXY, UVXY, SVXY, VXX, or a variance-risk-premium rule. Reports and `RULES.md` were read. `trades.csv` and `daily.csv` were not. The related fact from those files is the 2024–2026 equity path, not a volatility-premium P&L.
- **What I already know about the test windows:** The out-of-sample window 2024-07-01 through 2026-10-01 was used by the equity and ETF studies below. It is not unseen. From those reports, before any VIXY return was computed: SPY buy-and-hold from 2024-07-01 through 2026-10-01 was about +40.5% (Sharpe about 0.99, max drawdown about −20%). The same window contains the April 2025 tariff crash and rebound; SPY rose about 10.5% on 2025-04-09. Calendar-year SPY price returns in the treasury and RSI studies were about +23.4% in 2024, +16.4% in 2025, and +12% to +13% in 2026 through early October. QQQ buy-and-hold over the overlapping window through 2026-09-25 was about +55% (Sharpe about 1.0). `qqq-intraday-trend` (paper-trading proposal, not live capital) made +17.4% out of sample on QQQ (Sharpe 0.82) and was negative on SPY (Sharpe −0.36). `spy-rsi2-dip-buy` (paper-trading candidate) made +33.8% out of sample (Sharpe 1.29) against SPY +41.7%. `spy-overnight-premium` (Rejected) made +0.27% out of sample after 1 bp per night (Sharpe 0.053); the cash session, not the gap, earned the equity path. `index-opening-pop-fade` (Rejected) made −11.96% out of sample on SPY (Sharpe −0.66). Sector momentum was +6.2% out of sample (Sharpe 0.25, Rejected). FX ETF momentum was +6.36% out of sample (Sharpe 0.49, Rejected). Treasury ETF trend was −10.52% out of sample (Sharpe −0.65, Inconclusive). Commodity ETF momentum was +41.5% out of sample (Sharpe 0.57, Inconclusive on the trade count). `micro-futures-trend` (Rejected) lost money in an overlapping later window (OOS Sharpe −0.19 from 2024-10). Its report, read before this lock, said gold rallied through 2024–2025 and that the book lost money around the April 2025 shock. `small-cap-gap-up-fade` is a paper-trading candidate on a different 2024–2026 sample and a different rule. `qqq-atr-martingale` is a paper-trading candidate; the other QQQ intraday fades and the channel study lost money out of sample. `spy-rsi2-dip-buy` also reported that stored SPY daily bars from 2024-11-20 onward can differ from the 15:59 regular-hours print, including an after-hours close on 2025-04-02. VIXY and UVXY have no 1-minute bars here, so that comparison cannot be repeated. This study uses the stored daily bars anyway.
- **Public product history, not a measured return from this store:** short-dated VIX futures are usually in contango, and a long position in a short-term VIX futures ETP pays that roll. Volatility spikes in 2018 (February, including the XIV termination), 2020, and 2024 (August) are public knowledge. UVXY is a leveraged long-volatility product whose stated leverage was reduced after 2018 and which reverse-splits often. That knowledge is not a reason to drop a year, add a filter, or switch the primary to UVXY.
- **Where the parameters came from:** The always-short weight of −1, the monthly reset, the 5 bp trade cost, the 1% borrow, the 2024-07-01 split, the UVXY cross-market line, and the 24-trade out-of-sample floor were fixed by the request for this study before any VIXY return was computed. The weight grid is the protocol plateau around −1, kept on the short side. Seeds are the study tag `2026107` plus a digit.

## Hypothesis

A constant short of VIXY, reset monthly to a weight of −1, has a positive return after the costs below, because short-dated VIX futures usually sit above subsequent realized volatility and VIXY's roll implements that premium.

**Mechanism.** Sellers of volatility insurance are paid for crash risk (Bakshi and Kapadia 2003; Carr and Wu 2009). The other side is the hedger who buys short-dated VIX futures, including the long volatility ETP itself, which has to roll those futures. There is no timing signal. The premium, if it is there, is the average month, not a selected subset. Simon and Campasano (2014) is the published futures-basis version of the same carry. This study shorts the ETP, not the futures.

**Known counter-forces.** The premium is the payment for a crash. One month in which VIXY doubles can take more than the carry of many calm months. A fully funded short can lose more than its equity. UVXY's leverage and its 2018 leverage change make the same weight a larger bet on that name. The 1% borrow is a prior, not a quote, and it is too low if the shares are hard to borrow in a spike. No distribution is stored; these products' economic return is the futures roll, not a coupon. None of these is a reason to add a filter after the run.

## Predictions beyond P&L

If the mechanism is right, then:

1. Full-sample net total return at the base 5 bp trade cost and the base 1% borrow is positive.
2. The sum of the ten worst full-sample sessions' `strategy_net` is negative, and that sum is larger in absolute value than the average full-sample session's `strategy_net` times 50. That is the crash-skew claim.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. Neither prediction is an acceptance line. A pass of the acceptance table with a failed prediction has not confirmed the mechanism. A pass of prediction 1 does not replace the out-of-sample test.

## Data

- Instruments: VIXY (primary) and UVXY (cross-market, line 5). SPY is a close-to-close series for correlation, not a hurdle. Read with `agent-data/mdq.py`, `bars(symbol, "1d", adjust=True)`. The economic series is the split-adjusted one. Do not apply a second split to shares. Dividends are not adjusted because none are stored. Do not read 1-minute bars for the strategy, the benchmarks, the grid, the placebos, or the delayed path. VIXY and UVXY have none.
- Sample end: session dates on or before 2026-10-01. The VIXY and UVXY bars on 2026-10-02 are confirmed and then ignored. No later bar is used.
- Bars: stored daily bars. A daily bar's `ts` is 09:30 America/New_York. Its close is not known at 09:30. It is known at 16:00, or at 13:00 on an early close. The primary rule does not use the close as a signal.
- Calendar: for each product, the book is that product's own daily sessions from its first bar through 2026-10-01. Do not insert 2012-10-29, 2012-10-30, or 2018-12-05. Do not insert 2025-01-09. Keep 2021-12-31. The pre-lock count says this book matches `nyse_sessions` over the same span with those three closures removed. `backtest.py` aborts if that match fails, if the locked bar counts fail, or if the split-jump check fails. A failure writes no `results.json`, `daily.csv`, or `trades.csv`.
- Missing bars: if a book session has no bar, that session's strategy return is 0, borrow is not charged, shares do not change, and the short is not closed. The next present bar's gap uses the last observed close. The store path has no such session. The self-test does.
- Data checks the script must pass before it writes results: the counts and split lists in the prior-exposure bullet; every open and close positive; adjusted split ratios inside (0.5, 1.5) and raw ratios above 3; VIXY and SPY session dates through 2026-10-01 identical; 2026-10-01 present; 2026-10-02 present for VIXY and UVXY and absent from the book.

## Primary rule

Parameters, all fixed:

- `WEIGHT = -1` (fully short). Source: the request. The grid does not replace it.
- `COST_BPS = 5` on the absolute notional traded. Source: the request, a round prior for a product thinner and more violent than SPY. Not the protocol's 1 bp.
- `BORROW = 0.01` a year. Source: the request. The store has no borrow quotes. Charged as `abs(shares) * prior_close * BORROW / 252`.
- `LAST = 2026-10-01`, `OOS_START = 2024-07-01`.
- `ANNUAL = 252`.
- No VIX level, no trend filter, no lookback, no vol target.

Let the book be the product's daily sessions on or before `LAST`, in date order. A signal date is the last session of each calendar month in that book. October 2026 does not qualify, because its last NYSE session is after `LAST`.

1. **State.** Equity starts at 1. Cash starts at 1. Shares start at 0. There is no indicator. The target weight is `WEIGHT` at every rebalance and never a long weight. Between fills, the share count is constant, so the weight drifts.
2. **Signal.** The month-end date. The decision does not use the open, the high, the low, the close, or the volume. It is known before the close.
3. **Position.** One short. The first fill opens it. Each later fill resets it. Nothing else opens a position.
4. **Exits.** The only scheduled exit of a monthly holding is the next fill's open, reason `next_open`. The last holding, if still open, is marked at the last session's close, reason `end_of_sample`, with no extra trade and no extra cost. A ruin exit is defined below.
5. **Fills.** Primary mode `next_open`: fill at the first book session after the signal that has a bar, at that session's open. If the signal is the last bar, it does not fill. Delay mode `delay1`, reported only: the same rule two book sessions after the signal (`offset = 2`). Same-close mode, labelled upper bound, not a candidate: fill at the signal session's close (`offset = 0`).
6. **Sizing.** On a fill, after the gap and the borrow below, and before the rebalance cost, `shares_new = WEIGHT * equity_open / fill_price`. `WEIGHT` is negative, so the shares are short when `equity_open` is positive. Short notional equals `abs(WEIGHT)` times that equity. Do not resize again after the cost.
7. **Costs.** `rate = COST_BPS / 10000`. Cost = `rate * abs(shares_new - shares_old) * fill_price`. Charge it on the shares traded, not on the whole short when the position is only resized. The new holding pays that cost. The old holding pays no exit commission on a resize. Cash update: `cash = cash - (shares_new - shares_old) * fill_price - cost`. Borrow is separate from the 5 bp, and it is specified in the session order.

Session order, for each book session from the first fill through `LAST`. Sessions before the first fill are not evaluation rows.

1. If equity at the previous mark is <= 0, the account is already frozen. `strategy_net = 0`, equity unchanged, shares unchanged at 0. Stop participating. This is limited liability after a ruin, not a VIX filter.
2. If the session has no bar: `strategy_net = 0`. Do not charge borrow. Do not change shares or the last close.
3. Otherwise let `e0` be equity marked at the previous close, or 1 on the first fill. If shares were held overnight, charge borrow on `abs(shares) * previous_close` and subtract it from cash. Existing shares then earn the gap: price P&L `shares * (open - previous_close)`. Equity at the open is `cash + shares * open`.
4. If this session is a fill and equity at the open is > 0: close the old holding at this open if one is open (`next_open`), set the new shares from the sizing rule, pay the resize cost, and open the new holding. Entry equity of the new holding is the pre-cost equity at the open. New shares earn the open-to-close. Old shares do not.
5. If this session is a fill and equity at the open is <= 0 and shares are still short: cover at the open. The cover is a trade, so pay 5 bp on the absolute notional covered. Do not open a new short. Reason `ruin`. Do not earn the open-to-close. Freeze.
6. If shares are still open after that, they earn `shares * (close - open)`. Then set the last close to this close.
7. If equity at the close is <= 0: close the holding at this close, reason `ruin`, set shares to 0, and freeze. No extra commission, because the primary fills only at opens and this close is the mark that exhausted the account. Later sessions return 0. The overshoot below zero stays in this session's return and in terminal equity. Do not floor equity at 0. Do not flip the sign by resizing a negative equity.
8. `strategy_net = equity_end / e0 - 1` when `e0 > 0`. A frozen account does not get here with a new P&L.

Compounding identity: the sum of holding net dollars equals terminal equity minus 1. Net dollars are price P&L minus the borrow and the commission assigned to that holding. Gross dollars are price P&L only.

`daily.csv` is the VIXY primary only, base cost, mode `next_open`. Columns, in order: `date`, `strategy_net`, `strategy_gross`, `benchmark_long`, `spy_c2c`, `cash`, `equity`, `shares`. `strategy_gross` is the daily return of the same rule with `COST_BPS = 0` and `BORROW = 0`. `cash` is the zero benchmark. `equity` is equity after `strategy_net`.

`trades.csv` is the VIXY primary only. One row per monthly holding. Columns: `side`, `entry_time`, `entry_px`, `exit_time`, `exit_px`, `gross`, `net`, `exit_reason`, `entry_equity`, `hold_sessions`. `side` is `short`. `gross` and `net` are dollar P&L on the equity=1 account, not divided by entry equity. Times are `YYYY-MM-DD HH:MM` with no offset. An open fill is 09:30. An end-of-sample or ruin close is 16:00, or 13:00 when the date is in `mdq.EARLY_CLOSES`. That list starts in 2019, so an earlier early close is stamped 16:00. The stamp does not change any return. The price is the daily open or the daily close, as the exit rule says. `hold_sessions` is the number of book sessions from the entry date inclusive to the exit date exclusive for `next_open`, and inclusive of the exit date for `end_of_sample` and for a close-mark `ruin`.

UVXY uses this rule unchanged, on its own bars and its own first fill. It does not replace VIXY. Its rows stay in `results.json`. They are not written into the primary `trades.csv`.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Decision to be short | Before the month-end close. It does not depend on a price. | The next session's open, primary mode |
| Month-end date | The calendar, knowable before the session | Scheduling the fill |
| Daily open | 09:30 ET on the fill session | Fill price and the gap |
| Daily close | 16:00 ET, or 13:00 on a listed early close. The bar `ts` is 09:30 and is not the knowledge time | Marking the holding. Not a signal. Same-close mode uses it as a fill and is an upper bound only |
| High, low, volume | Not used | Not used |
| Split table | Applied by mdq as a backward adjustment of the stored series | The adjusted open and close. Shares are not split a second time |
| Any rolling statistic, z-score, quantile, or volatility target | Not used | Not used |
| SPY close | 16:00 ET | The correlation series only. Not a signal and not a hurdle |

## Samples

- Warm-up: none beyond the first month-end. The first VIXY signal is 2011-01-31 and the first fill is 2011-02-01. UVXY's first fill is 2011-11-01. No lookback.
- **In-sample:** the first fill through the last session strictly before 2024-07-01.
- **Out-of-sample:** 2024-07-01 through 2026-10-01.
- The split date is the house date used by the earlier index studies. Those studies have already exposed this out-of-sample window. See prior exposure. Do not move it.
- Evaluation-window metrics only. A session's return, including the gap from the previous close, is dated on the session that opens. The holding that enters before 2024-07-01 and exits at the 2024-07-01 open is an in-sample holding. The daily return on 2024-07-01 is an out-of-sample return. Trip profit factor uses entry date. Sharpe and total return use the daily returns. Those two cuts do not sum to each other. Do not "fix" that after seeing results.
- An out-of-sample round trip is one monthly holding whose entry fill is on or after 2024-07-01, including `end_of_sample` and `ruin` if that is the entry rule.

## Benchmarks

- Uncosted long VIXY, bought and held: buy `1 / open` shares at the primary's first fill open, cash zero, no commission, no borrow, no monthly reset. First session earns open-to-close. Later sessions earn close-to-close. Same evaluation dates.
- Cash at zero: every evaluation session returns 0.
- SPY close-to-close on the same dates: `close / previous SPY close - 1`, using SPY's own previous session, including the gap into the first evaluation date. SPY is not the hurdle. Report the Pearson correlation of `strategy_net` with this series on the full, in-sample, and out-of-sample windows.

## Secondary candidates

None. UVXY is line 5, not a second primary. It is never promoted over VIXY.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at base cost: the protocol defaults. Sharpe is the mean daily `strategy_net` divided by its sample standard deviation (n−1), times √252, with a zero risk-free rate. Total return is the compound of those daily returns. CAGR uses a 252-session year and is null if terminal equity of that window is <= 0. Max drawdown is the minimum of compounded equity over its running peak, minus 1. Profit factor is the sum of positive holding net dollars divided by the absolute sum of negative holding net dollars, for holdings whose entry date is inside the window. Average net trade in bp is the mean of `net / entry_equity * 10000`. Exposure is the fraction of evaluation sessions with nonzero shares at the close of the session. Days with no position count as 0.
- Breakdowns: calendar year; side (the primary is short only); exit reason; quintile of the SPY close-to-close move; quintile of VIXY's own close-to-close move. Quintile edges use the sorted sample, remainder sessions assigned to the earliest quintiles. These breakdowns are not filters.
- Pre-registered stress windows, compounded `strategy_net`, not filters and not acceptance lines: February 2018, March 2020, August 2024, April 2025.
- Costs: multiples 0, 0.5, 1, 2, and 3 of the base pair. A multiple scales both `COST_BPS` and `BORROW`. Line 4 uses multiple 2, which is 10 bp on the notional traded and 2% borrow. Fill delay: `delay1`. Upper bound: same-close fill.
- Direction placebo on the zero-cost, zero-borrow path: 2,000 draws, seed 20261071. Each draw multiplies each holding's gross dollar P&L by an independent ±1, which flips that short month into a long month when the sign is minus. The daily placebo return is the summed flipped gross dollars divided by the unflipped zero-cost equity at the previous close. Compare the resulting Sharpe with the actual gross Sharpe. `p = (1 + count of draws whose Sharpe >= actual) / 2001`.
- Timing placebo: not applicable, and not run. The rule is short at every rebalance, so redrawing entry months while keeping the always-short count reproduces the strategy. Seed 20261073 is recorded and unused.
- Block bootstrap: circular 20-session blocks of the full-sample net daily returns, 2,000 draws, seed 20261072. Report the 2.5th and 97.5th percentiles of Sharpe.
- Plateau grid on the in-sample window: `WEIGHT` in {−0.5, −0.75, −1, −1.25, −1.5}. Five cells. The sign stays short. Show out-of-sample Sharpe beside the in-sample Sharpe for selection bias only. Do not select a scale. The primary remains −1. Report the Spearman rank correlation of in-sample and out-of-sample Sharpe across the five cells.
- Cross-market: UVXY, identical weight, costs, borrow, and fill.
- Verification: the self-test below, before the store is opened, and `verify.py` on every monthly VIXY holding, with a seeded draw of at least 40 (seed 20261074) reported inside that full match. `verify.py` shares no signal code with `backtest.py`.

Self-test, hand-built bars, before any store open. Abort if any case fails.

- A down month for the long product: short gross P&L is positive.
- An up month: short gross P&L is negative.
- A reverse split is not applied by the engine. The input series is already smooth. A one-day drift must not be booked as a four- or five-times jump.
- A monthly resize charges 5 bp on the shares traded, not on the whole short, and the share count changes when equity has changed.
- Borrow reduces equity on a flat day by `prior_close_notional * 0.01 / 252`.
- A missing bar returns 0 and leaves the share count unchanged.
- End of sample marks the last close, reason `end_of_sample`, with no extra cost.
- A gap that drives equity through zero freezes the account, leaves shares at 0, and does not open a long.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at the base 5 bp and the base 1% borrow:

1. Out-of-sample Sharpe >= 0.5 and out-of-sample profit factor >= 1.10.
2. Direction placebo p <= 0.05 on the full-sample gross Sharpe.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0 (at least 3 of 5).
4. Full-sample total return > 0 at 2× the base cost pair.
5. Out-of-sample Sharpe > 0 on UVXY under identical rules.
6. At least 24 out-of-sample round trips. A round trip is one monthly holding. It is out of sample when the entry fill is on or after 2024-07-01.

Line 6 is the only change from the protocol defaults. The protocol's floor of 100 out-of-sample trades does not fit a monthly holding. The out-of-sample window has 28 pre-lock fill dates. The floor stays at 24. No other line is lowered.

Status map, fixed here:

- If line 6 fails, the status is **Inconclusive**, even if another line also fails.
- If line 6 passes and any of lines 1 through 5 fails, the status is **Rejected**.
- If all six pass, the status is **Paper-trading candidate**.
- **Void** only if an implementation or data defect cannot be fixed without changing these rules.

A failed line fails the strategy. A grid cell, UVXY, or a stress window cannot replace the primary.

## Not done in this study

The report will not promote any of these in place of the primary: a grid weight other than −1; a long VIXY position; UVXY as the primary; a VIX-level filter; a trend filter; a volatility target; dropping 2018, 2020, or 2024; dropping the leverage-change window; a crash filter; a different cost, borrow, fill, or sample split; using the 2026-10-02 bar; using raw unadjusted prices; using the 15:59 minute print. There is no minute print for these two products in the store.
