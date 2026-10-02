# SPDR sector momentum: XLK, XLF, XLE, XLV, XLI, XLY, XLP, XLU, XLB, XLRE, XLC

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Rejected.** Failed 2 of the 5 applicable pre-registered tests. The minimum sample was met, so the verdict is Rejected. |
| Instruments | Eleven SPDR sector ETFs, long the top 3 and short the bottom 3, monthly rebalance, held overnight |
| Data | Evaluation 2012-03-01 → 2026-10-01, read via `agent-data/mdq.py`. Daily bars. The three NYSE closures with no SPY bar are omitted. No dividend adjustment. |
| Rules | [`research/spdr-sector-momentum/research/RULES.md`](../research/RULES.md), locked 2026-10-02 19:54 UTC, sha256 `7bcd689a27ad` |
| Code | [`research/spdr-sector-momentum/research/`](../research/) · 1 store run (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** A monthly long/short book of the eleven SPDR sector ETFs, ranked on the 12-month return that skips the last month, made a small positive return out of sample and missed the tests that decide whether to paper-trade it. From 2024-07-01 through 2026-10-01 it returned **+6.2%** after 1 bp per side (Sharpe **0.253**, profit factor **0.935**, 35 round trips, max drawdown −16.0%). In sample, 2012-03-01 through 2024-06-28, it returned +9.5% (Sharpe 0.124). Over the full evaluation it returned +16.3% (Sharpe 0.144, max drawdown −29.2%). SPY buy-and-hold returned +40.5% out of sample (Sharpe 0.985). Terminal equity is 1.16 for the strategy, 5.58 for SPY, and 3.89 for the equal-weight sector book (`posthoc.json` `terminal_equity`, equal to one plus the stored total returns).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2012-03-01 → 2026-10-01 | 2012-03-01 → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,668 | 3,102 | 566 |
| Total return | +16.3% | +9.5% | **+6.2%** |
| CAGR | 1.04% | 0.74% | 2.71% |
| Annual volatility | 14.8% | 14.8% | 15.0% |
| Sharpe | 0.144 | 0.124 | **0.253** |
| Max drawdown | −29.2% | −29.2% | −16.0% |
| Trades / profit factor | 226 / 1.055 | 191 / 1.072 | 35 / **0.935** |
| Avg net trade | +9.4 bp | +12.3 bp | −6.0 bp |
| t-stat of the daily mean | 0.55 | 0.44 | 0.38 |
| *SPY buy-and-hold Sharpe (max DD)* | *0.79 (−34.1%)* | *0.76 (−34.1%)* | *0.98 (−19.9%)* |
| *Equal-weight sectors Sharpe (max DD)* | *0.67 (−36.6%)* | *0.65 (−36.6%)* | *0.86 (−16.9%)* |

It failed 2 of the 5 applicable acceptance tests written before the first run (§8). Line 5 was recorded as not applicable before the lock. Line 6 was met (35 out-of-sample round trips against a floor of 24), so the verdict is Rejected.

**Why.**

1. **The out-of-sample book misses both bars on line 1.** Sharpe is 0.253 against a required 0.5. Profit factor is 0.935 against a required 1.10. The out-of-sample win rate is 51.4%, and the average losing trip (−0.0221) is larger than the average winning trip (+0.0195). At zero cost the out-of-sample Sharpe is 0.261 (`gross.oos.sharpe`). The cost sweep leaves that Sharpe between 0.238 and 0.261.
2. **The rank is inside the sign-flip null.** The direction placebo p-value is 0.278 against a required 0.05. Actual gross Sharpe is 0.152. The null mean is 0.000 and its 95th percentile is 0.387. The timing placebo, which reshuffles formation returns across eligible names, has p 0.293.
3. **The short book lost more than a dollar of price P&L for each dollar the long book made.** Full-sample gross price P&L is +1.914 on the long book and −1.733 on the short book. Prediction 1 required both to be positive. Short-side profit factor is 0.260 on 108 trips (win rate 25%). Long-side profit factor is 4.01 on 118 trips (win rate 69.5%).
4. **A wider winner-minus-loser formation gap did not come with a higher holding return.** Across 176 non-flat rebalances the top half of the gap (median 0.240, 88 months) has a mean holding return of −0.000806. The bottom half has +0.003856. Prediction 2 is not consistent.
5. **The full-sample Sharpe interval contains zero.** The 20-session block bootstrap 95% interval of the full-sample net Sharpe is −0.312 to +0.634.

**Recommendation.** Do not trade it. The locked rules do not allow the result to be replaced by the 126-session grid cell, the long side, one sector, the 2022 calendar year, a dividend adjustment, a different skip, a different cost, or a different sample split.

## 2. The strategy

### Rules

On the last NYSE session of each completed calendar month that has a SPY daily bar:

```
Eligible names have a split-adjusted close on the signal date
and a close 273 of their own sessions earlier.
Formation = close[t-21] / close[t-273] - 1.
If fewer than 6 names are eligible, the book is flat.
Otherwise sort by formation descending, ties by symbol ascending.
Long the top 3 at +1/3. Short the bottom 3 at -1/3. Middle names are flat.

Fill at the next session's open. Hold shares until the next fill.
At the open, old shares earn the gap. Size off mark-to-open equity
before cost. Charge 1 bp on the absolute notional of the share change.
New shares earn the open-to-close.
A missing bar earns 0 that day and does not force an exit.
No stop, no volatility target, no overlay.
Positions still open on 2026-10-01 are marked to the close
with reason sample_end and with no exit cost.
```

- **Why 252 sessions with a 21-session skip.** That is the Jegadeesh-Titman 12-1 formation. The skip is fixed. The grid varies the lookback and is not used to choose it.
- **Why three and three.** The study design specified an equal-weight long/short book with gross long 1 and gross short 1. A month with fewer than six eligible names is flat. After the first fill, every month in this store had at least six.
- **Why the next open.** The signal uses the close, which is known at 16:00 ET. The fill is the next session's open. The same-bar close is reported as a labelled upper bound.
- **Why 1 bp.** The protocol default for SPY-class liquidity. These ETFs are in that class. The cost was not changed after the P&L.

### How it trades

| | |
|---|---|
| Rebalance sessions | 176 of 3,668 evaluation sessions (`posthoc.json`) |
| Round trips | 226. 15.5 per 252 sessions (`posthoc.json` `trades_per_year`) |
| Time in market | 1.0. Mean gross exposure 2.013 |
| Holding time | median 62 sessions, mean 98.4. Out of sample, median 43, mean 67.0 |
| Long / short | 118 / 108 trips. Profit factor 4.01 / 0.260. Average net trade +153 bp / −147 bp |
| Win rate | 48.2% full sample. Average winning trip +0.0289, average losing trip −0.0255, in account dollars |
| Exits | flat 211, flip 9, sample_end 6. Of the 35 trips that enter on or after 2024-07-01: 28 flat, 1 flip, 6 sample_end (`posthoc.json`, post hoc) |

The first evaluation row, 2012-03-01, is long 3 and short 3, with a strategy return of −0.00363 and a cost of 0.0002 (`posthoc.json` `first_day`). The average out-of-sample trip is −6.0 bp. The out-of-sample compound return is still +6.2% because the daily window includes marks of positions that were entered before 2024-07-01. Trip profit factor uses trips whose entry fill falls inside the window.

## 3. Hypothesis and predictions

Capital and attention move slowly across industries, so the recent cross-section of sector returns was claimed to predict the next month. The other side is an investor who buys recent losers, or who is tied to a sector benchmark and rebalances toward losers and away from winners (Jegadeesh and Titman 1993; Moskowitz and Grinblatt 1999).

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Full-sample gross price P&L is positive on the long book and positive on the short book | Long +1.914, short −1.733 (`long_gross_pnl`, `short_gross_pnl`) | Not consistent |
| Months in the top half of the winner-minus-loser formation gap have a higher mean holding return than the bottom half | 176 holds, median gap 0.240, 88 / 88. Mean holding return −0.000806 in the top half and +0.003856 in the bottom half | Not consistent |

Both predictions fail. The positive full-sample price return does not confirm the mechanism. The short side, which is where a slow-moving loser discount would show up, lost money. Months with a wider gap between winners and losers had the lower mean holding return.

The pre-registered quintiles point the same way as a market-beta reading of the book. Mean daily strategy return is −0.00120 on the worst fifth of SPY days and +0.00086 on the best fifth (§7). The book loses money on the days a crisis-alpha claim would need it to make money.

## 4. Method

- **Data.** Daily split-adjusted bars from `data/market-data.sqlite` through `agent-data/mdq.py`. SPY and the nine original sectors have 3,959 bars, 2011-01-04 through 2026-10-01. XLRE has 2,759 bars from 2015-10-08. XLC has 2,083 bars from 2018-06-19. No sector ETF has 1-minute bars. The only post-listing holes versus SPY's sessions are XLRE on 2015-10-14 and 2015-11-27. The calendar sessions with no SPY bar are 2012-10-29, 2012-10-30, and 2018-12-05. They are skipped. 2021-12-31 is included. Splits in the store are the 2-for-1 on XLK, XLE, XLY, XLU, and XLB, ex-date 2025-12-05. Adjusted close ratios that day are between 0.991 and 1.006. The stored open and high on XLK, XLE, and XLB are discontinuous; the rule uses the stored open and close anyway. 2025-12-05 is not a fill (the November signal fills on 2025-12-01). There are no dividend rows. Price returns omit distributions, so a high-distribution sector looks worse when held long and better when held short than it would in a total-return test.
- **Known daily-bar defect.** On 1,254 overlapping sessions, SPY's stored daily close differs from the last regular-hours 1-minute close by more than 20 bp on 111 sessions, all on or after 2024-11-01. The largest is 2025-04-02 (stored daily close 544.83, last 1-minute close 564.52). This was re-verified before the lock. The sector ETFs cannot be checked the same way. The study uses the stored daily open and close, as the rules required. That is a measurement risk inside the out-of-sample window. It is not treated as a Void: the rule names those columns, and there is no 1-minute sector history back to 2011 to switch to.
- **Pre-registration.** `RULES.md` was hashed before any strategy return. `rules_sha256` in `results.json` is `7bcd689a27ad44dc7e6d41a13d302e2a96f05f96b45219abe037f7622b148ec0`, the same value as `RULES.lock`. The file was not git-committed. The invoking request forbade a commit. Git HEAD at the lock and at the run was `8068750d3850669df5441a076ec108499b4acd44`, and the worktree was dirty. No earlier study in this repo used these eleven SPDRs. IGV appears in intraday studies as one software ETF. `micro-futures-trend` is a rejected time-series futures book (out-of-sample Sharpe −0.19), not this cross-section. `low-liq-high-vol-mean-reversion` is a weekly small-cap reversal and lost money out of sample. The out-of-sample calendar is not unseen: through 2026-09-25, earlier equity studies had already reported SPY buy-and-hold near +42% (Sharpe near 1) and QQQ near +55% (Sharpe 1.01), with SPY calendar gains on the order of +23% in 2024, +16% in 2025, and +13% in 2026 to 25 September, plus the April 2025 selloff and the rebound of about +10% in SPY on 9 April. This study's own SPY buy-and-hold from 2024-07-01 through 2026-10-01, on the stored daily closes, is +40.5% (Sharpe 0.985) over 566 sessions. The sector cross-section was not known before the lock. Sessions 2026-09-26 through 2026-10-01 were not in those earlier reports. The 2022 bear market is inside the in-sample window.
- **Fills and costs.** Primary fill is the next session's open. Cost is 1 bp of notional per side on the share change. No borrow fee and no cash interest. A same-side resize stays one round trip and its cost stays on that trip.
- **Returns.** Daily simple returns on the evaluation sessions. A flat day would count as 0. After the first fill the book is in the market every session (`time_in_market` 1.0). Sharpe is the mean divided by the sample standard deviation of those daily returns, times √252, with a zero risk-free rate. CAGR uses a 252-session year.
- **Verification.** The self-test on synthetic sessions passed before the store was opened, covering entry, hold, same-side resize, flip, a month with fewer than six names, a missing bar while held, a deferred fill, the alphabetical tie-break, and the sample-end mark. `verify.py` shares no signal code with `backtest.py`. It matched all 226 trades on side, entry date, entry price, exit date, exit price, and exit reason. Seed 20261014 drew 40 rebalance signals; 43 trades enter on those fill dates, and those matched too.
- **Runs.** One store run, reason `initial`, 2026-10-02 20:04 UTC. No later run. No bug fix changed the headline numbers. `research/README.md` was not updated: the invoking request forbade editing it.

## 5. Results

![Growth of $1](figures/equity.svg)

The strategy ends at 1.16 while SPY ends at 5.58 and the equal-weight sector book ends at 3.89. The dashed line is 2024-07-01. The strategy's gain is a small slope on a path that spends years below its start.

![Drawdown](figures/drawdown.svg)

The strategy's worst peak-to-trough is −29.2%, and that trough is inside the in-sample window. The out-of-sample max drawdown is −16.0%. SPY's full-sample max drawdown is −34.1%. The equal-weight sector book reaches −36.6%.

| Strategy (1 bp / side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary, lookback 252** | +16.3% / 0.144 / −29.2% | 0.124 | +6.2% / 0.253 / −16.0% |
| *SPY buy and hold, uncosted* | +458% / 0.791 / −34.1% | 0.756 | +40.5% / 0.985 / −19.9% |
| *Equal-weight eligible sectors, uncosted* | +289% / 0.673 / −36.6% | 0.647 | +26.7% / 0.855 / −16.9% |

![Calendar-year return](figures/by_year.svg)

2024 is a calendar year that mixes the in-sample months through June with the out-of-sample months from July. 2012 starts on 1 March (210 sessions). 2026 ends on 1 October (188 sessions). The largest strategy year is 2022, which is in sample.

| Year | Strategy | Sharpe | Max DD | SPY |
|---|---:|---:|---:|---:|
| 2012 | +2.5% | 0.35 | −11.8% | +3.9% |
| 2013 | +8.9% | 1.19 | −5.3% | +29.7% |
| 2014 | −6.1% | −0.63 | −13.0% | +11.3% |
| 2015 | +11.9% | 1.01 | −8.7% | −0.8% |
| 2016 | −4.6% | −0.26 | −16.1% | +9.6% |
| 2017 | −5.3% | −0.61 | −11.2% | +19.4% |
| 2018 | −8.6% | −0.47 | −20.3% | −6.3% |
| 2019 | +4.1% | 0.38 | −10.9% | +28.8% |
| 2020 | +16.5% | 0.71 | −23.2% | +16.2% |
| 2021 | −20.0% | −1.25 | −21.3% | +27.0% |
| 2022 | +34.9% | 1.80 | −11.5% | −19.5% |
| 2023 | −17.0% | −1.17 | −22.4% | +24.3% |
| 2024 | +6.6% | 0.51 | −13.4% | +23.4% |
| 2025 | −1.7% | −0.07 | −11.1% | +16.4% |
| 2026 | +5.3% | 0.47 | −11.6% | +12.0% |

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trip's dates and multiplies that trip's daily gross dollar P&L by an independent ±1 (2,000 draws, seed 20261011). Gross Sharpe uses price P&L divided by the primary path's prior close equity. Actual gross Sharpe is 0.152. The null mean is 0.0005 and the null 95th percentile is 0.387. p = (1 + number of draws at least as large as the actual) / 2001 = 0.278.

The timing placebo rebuilds the book (500 draws, seed 20261013). At each rebalance it permutes formation returns across the eligible names and applies the same holding, sizing, and cost rules. The null mean gross Sharpe is 0.005 and the null 95th percentile is 0.404. p = 0.293 on the same actual gross Sharpe of 0.152.

Both placebos put the actual rank inside the bulk of the null. They do not say the accounting is wrong. `verify.py` matched the trades. They say this ranking of these sectors, on this sample, is compatible with assigning the formation numbers at random.

### Bootstrap

Circular blocks of 20 sessions on the full-sample net daily returns, 2,000 draws, seed 20261012. The 2.5th and 97.5th percentiles of the Sharpe are −0.312 and +0.634. The interval contains 0 and contains the out-of-sample acceptance bar of 0.5. The t-stat of the mean daily return is 0.55 full sample and 0.38 out of sample. The bootstrap is not an acceptance line.

### Parameter plateau

![Parameter grid](figures/grid.svg)

Five lookbacks, skip fixed at 21, book fixed at three and three. Every in-sample Sharpe is above zero (5 of 5). The primary, lookback 252, ranks 3 of 5 in sample. The in-sample winner is lookback 126 (IS Sharpe 0.240). Out of sample that cell returned −20.0% (Sharpe −0.606). The Spearman correlation of the five in-sample Sharpes with the five out-of-sample Sharpes is −0.10. The primary is the one cell with a positive out-of-sample Sharpe. Nothing was selected from the grid.

| Lookback | First fill | IS Sharpe | OOS Sharpe | OOS return |
|---|---|---:|---:|---:|
| 126 | 2011-09-01 | 0.240 | −0.606 | −20.0% |
| 189 | 2011-12-01 | 0.058 | −0.526 | −18.5% |
| **252 (primary)** | 2012-03-01 | 0.124 | 0.253 | +6.2% |
| 315 | 2012-06-01 | 0.112 | −0.325 | −13.0% |
| 378 | 2012-09-04 | 0.232 | −0.230 | −11.1% |

Each cell's in-sample Sharpe starts at that cell's own first fill. Out-of-sample dates are still on and after 2024-07-01 for every cell.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.152 | 0.148 | **0.144** | 0.137 | 0.129 |
| OOS Sharpe | 0.261 | 0.257 | **0.253** | 0.246 | 0.238 |
| Full-sample return | +18.2% | +17.3% | **+16.3%** | +14.4% | +12.6% |

Full-sample return at 2 bp is +14.4%, which clears line 4. Out-of-sample return at 3 bp is +5.7%. There is no cost in this sweep at which the out-of-sample Sharpe reaches 0.5. Delaying the fill by one extra SPY session drops the out-of-sample Sharpe to 0.141 and the full-sample return to +6.9% (`fill_delay`). The labelled same-bar close fill has an out-of-sample Sharpe of 0.185 and an out-of-sample return of +3.8% (`signal_close_upper_bound`), below the next-open primary.

### Other markets (identical rules)

No second sector family is in the store. Line 5 was marked not applicable before the lock. No other market was fetched.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Quintile 1 is the worst fifth of SPY close-to-close days in the full evaluation sample. The strategy's mean daily return rises with SPY's day. These buckets are descriptive. They were not used to filter the rule.

| Quintile | Sessions | Mean strategy return | Mean SPY return |
|---|---:|---:|---:|
| 1 | 733 | −0.001198 | −0.013248 |
| 2 | 734 | +0.000179 | −0.002762 |
| 3 | 733 | +0.000116 | +0.000677 |
| 4 | 734 | +0.000461 | +0.004483 |
| 5 | 734 | +0.000864 | +0.013453 |

| Side | Trips | Win rate | Profit factor | Gross P&L | Net P&L | Avg net trade |
|---|---:|---:|---:|---:|---:|---:|
| Long | 118 | 69.5% | 4.01 | +1.914 | +1.905 | +153 bp |
| Short | 108 | 25.0% | 0.260 | −1.733 | −1.742 | −147 bp |

| Exit reason | Trips | Win rate | Profit factor | Avg net trade |
|---|---:|---:|---:|---:|
| flat | 211 | 48.3% | 1.04 | +9.1 bp |
| flip | 9 | 33.3% | 0.148 | −137 bp |
| sample_end | 6 | 66.7% | 13.4 | +243 bp |

The six `sample_end` trips are the book marked to the 2026-10-01 close with no exit cost. All six enter on or after 2024-07-01, so they sit inside the out-of-sample profit factor (`posthoc.json` `oos_trades_by_exit`). None of the side, exit, or quintile splits was used to change the rule.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. Out-of-sample Sharpe and profit factor | Sharpe ≥ 0.5 and PF ≥ 1.10 | Sharpe 0.253, PF 0.935 | Fail |
| 2. Direction placebo | p ≤ 0.05 on the full sample | p = 0.278 | Fail |
| 3. In-sample plateau | IS Sharpe > 0 and at least 3 of 5 grid cells with Sharpe > 0 | IS Sharpe 0.124, 5 of 5 cells above zero | Pass |
| 4. Double cost | Full-sample total return > 0 at 2 bp per side | +0.144 | Pass |
| 5. Cross-market | Where a second sector family exists | Not applicable. Recorded before the lock. Neither a pass nor a fail | n/a |
| 6. Minimum sample | At least 24 out-of-sample round trips | 35 | Pass |

Line 6 is lower than the protocol's 100 because the rule rebalances monthly and the out-of-sample window has 27 completed month-ends. A 100-trip floor would have required a higher-frequency rule than the published monthly hold. No other threshold was lowered. Because line 6 is met and lines 1 and 2 fail, the status is Rejected.

A pass on lines 3 and 4 means the in-sample Sharpe is positive, the neighbouring lookbacks are positive in sample, and 2 bp per side does not turn the full-sample compound return negative. It does not mean the out-of-sample rank cleared a null.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| The short book is the loss | Short gross P&L −1.733, profit factor 0.260, win rate 25% | None in this study. The long side is not promoted |
| Market direction | Quintile means rise from −0.00120 to +0.00086 with SPY's day. Out-of-sample correlation of daily strategy returns with SPY is 0.378 (post hoc). Full-sample correlation is 0.054 (post hoc) | The rule has no volatility target and no beta hedge. None is added now |
| One in-sample year | 2022 strategy return +34.9% while SPY returned −19.5%. Dollar equity rose by 0.328 that year, against a full-sample dollar gain of 0.163 (post hoc, share 2.01). Equity ended 2021 at 0.941 and ended 2022 at 1.269 | 2022 stays in the sample. It is not a sleeve |
| Recent window | Trailing 252 sessions, 2025-10-01 through 2026-10-01: strategy return −0.99%, Sharpe 0.030, against SPY +15.1% and Sharpe 1.12 (post hoc). Trailing strategy Sharpe was positive in 54.1% of 3,417 windows, minimum −1.40 on the window ending 2024-01-03, maximum +1.80 on the window ending 2022-12-20, last value 0.030 (post hoc) | Reported. Not a reason to move the split |
| Dividends omitted | No dividend rows in the store. High-distribution sectors look worse long and better short than a total-return test. XLU, XLRE, and XLP are three of the four worst names by net trip P&L (post hoc) | A dividend series was not invented |
| Borrow omitted | Short-sale proceeds earn zero and no borrow fee is charged | A fee would be a different pre-registered cost. It is not fitted now |
| Stored daily closes | 111 SPY sessions from 2024-11-01 differ from the 16:00 print by more than 20 bp. Sector ETFs have no 1-minute bars to compare | The rule uses the stored open and close. The 1-minute print is not swapped in |
| Split prints | XLK, XLE, and XLB opens and highs on 2025-12-05 are discontinuous. That date is not a fill | The stored open is kept |
| Sample length out of sample | 27 month-ends, 35 round trips, 566 sessions. The bootstrap interval of the full-sample Sharpe contains zero | Line 6 makes this a rejection rather than an inconclusive sample. It does not make the Sharpe estimate tight |
| Placebo | Direction p 0.278, timing p 0.293 | The primary stays rejected |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A different rule, a dividend-adjusted series, or a later window is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

`posthoc.py` reads `daily.csv`, `trades.csv`, and `results.json`. It does not open the store. The sums of daily long and short price P&L match `long_gross_pnl` and `short_gross_pnl`.

- **(post hoc) Trailing year.** The 252 sessions ending 2026-10-01 returned −0.99% for the strategy (Sharpe 0.030) and +15.1% for SPY (Sharpe 1.12). The equal-weight sector book returned +8.5% (Sharpe 0.90) over the same sessions.
- **(post hoc) Rolling Sharpe.** A 252-session strategy Sharpe was positive in 54.1% of windows. The low is −1.40 on the window ending 2024-01-03. The high is +1.80 on the window ending 2022-12-20. The last value is 0.030.
- **(post hoc) Day concentration.** Setting the 20 highest and the 20 lowest strategy days to zero, and keeping those days in the sample, raises the full-sample return from +16.3% to +36.4% (Sharpe 0.227) and the out-of-sample return from +6.2% to +14.6% (Sharpe 0.594). The left tail is larger than the right tail. This is not an acceptance test, and those days stay in the primary result.
- **(post hoc) Correlation with SPY.** 0.054 over the full sample and 0.378 out of sample.
- **(post hoc) 2022 dollars.** Equity went from 0.941 on 2021-12-31 to 1.269 on 2022-12-30, a gain of 0.328. The full-sample gain is 0.163. The 2022 gain is 2.01 times the full-sample gain. Later years gave that dollar gain back and more. The year's short-book price P&L was +0.263 and the long-book price P&L was +0.066. 2015 is the other year in which the short book's price P&L was positive (+0.114). In the other thirteen calendar years the short book lost money.
- **(post hoc) By name, net trip P&L.** XLC +0.418 (11 trips), XLK +0.361 (21), XLV +0.152 (20), XLY +0.052 (27), XLE +0.020 (18), XLI −0.003 (21), XLB −0.053 (21), XLF −0.172 (24), XLP −0.179 (29), XLRE −0.203 (11), XLU −0.231 (23). No name is dropped and no name is promoted.

None of these figures enter §8.

### Ideas for a new study

A total-return test needs a dividend series this store does not have, and it needs a window this study has not scored. That is a different rules file. This report does not treat the long side, the 126-session lookback, or 2022 as that study.

## 12. Reproduce

From the repo root:

```bash
python research/spdr-sector-momentum/research/backtest.py
```

```bash
python research/spdr-sector-momentum/research/verify.py
```

```bash
python research/spdr-sector-momentum/research/posthoc.py
```

```bash
python research/spdr-sector-momentum/research/charts.py
```

`backtest.py` recomputes the SHA-256 of `RULES.md` after normalizing CRLF to LF, refuses to run on a mismatch with `RULES.lock`, and writes the hash into `results.json`. The logged store run is the initial run. A repeat on the same store appends another `RUNLOG.md` entry. Seeds are 20261011 (direction), 20261012 (block bootstrap), 20261013 (timing), and 20261014 (the verify draw). `verify.py` re-reads the store and checks `trades.csv`. `posthoc.py` and `charts.py` read the saved outputs only. Charts write SVG under `report/figures/`.

`counts.py` is the pre-lock coverage script. It does not compute a strategy return.

Checklist items that fail, on purpose: `research/README.md` does not list this study, because the request forbade editing it, and `RULES.md` was not committed, because the request forbade a commit. The hash in `RULES.lock` matches `results.json`. Nothing was written to `data/`, and `ingest` was not run.

### References

- Jegadeesh, Narasimhan, and Sheridan Titman. 1993. "Returns to Buying Winners and Selling Losers: Implications for Stock Market Efficiency." *Journal of Finance* 48 (1): 65–91.
- Moskowitz, Tobias J., and Mark Grinblatt. 1999. "Do Industries Explain Momentum?" *Journal of Finance* 54 (4): 1249–1290.
