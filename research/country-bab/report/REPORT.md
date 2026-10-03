# Country betting against beta: EWA, EWC, EWG, EWH, EWJ, EWS, EWU, EWW, EWZ, EWL, EWT, EWY

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Inconclusive.** Failed 4 of the 5 scored pre-registered tests. Line 6 is 14 out-of-sample round trips against a floor of 24, so the verdict is Inconclusive. |
| Instruments | Twelve iShares country ETFs, long the 3 lowest trailing betas to SPY and short the 3 highest, monthly rebalance, held overnight |
| Data | Evaluation 2012-02-01 → 2026-10-01, read via `agent-data/mdq.py`. Daily bars. The three NYSE closures with no bar are omitted. The 2026-10-02 country bars are excluded. No dividend adjustment. |
| Rules | [`research/country-bab/research/RULES.md`](../research/RULES.md), locked 2026-10-02 21:14 UTC, sha256 `78c6d6ac7a99` |
| Code | [`research/country-bab/research/`](../research/) · 1 store run of the backtest, plus 2 verification replays (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** A monthly long/short book of twelve country ETFs, ranked on trailing beta to SPY, lost money out of sample and missed the sample floor that decides the status. From 2024-07-01 through 2026-10-01 it returned **−37.9%** after 5 bp per side (Sharpe **−0.770**, profit factor **2.060** on 14 round trips, max drawdown −54.1%, 566 sessions). In sample, 2012-02-01 through 2024-06-28, the compound return was −9.5% (Sharpe 0.006). Over the full evaluation it returned −43.8% (Sharpe −0.179, max drawdown −56.3%). The equal-weight country book returned +55.0% out of sample (Sharpe 1.187). SPY close-to-close returned +40.5% out of sample (Sharpe 0.985). Terminal equity is 0.562 for the strategy, 1.935 for the equal-weight book, and 5.823 for SPY (`posthoc.json` `terminal_equity`).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2012-02-01 → 2026-10-01 | 2012-02-01 → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,688 | 3,122 | 566 |
| Total return | −43.8% | −9.5% | **−37.9%** |
| CAGR | −3.86% | −0.80% | −19.09% |
| Annual volatility | 15.4% | 13.3% | 23.8% |
| Sharpe | −0.179 | 0.006 | **−0.770** |
| Max drawdown | −56.3% | −28.9% | −54.1% |
| Trades / profit factor | 119 / 0.832 | 105 / 0.776 | 14 / **2.060** |
| Avg net trade | −35.1 bp | −52.8 bp | +98.1 bp |
| t-stat of the daily mean | −0.68 | 0.02 | −1.15 |
| *SPY close-to-close Sharpe (max DD)* | *0.806 (−34.1%)* | *0.773 (−34.1%)* | *0.985 (−19.9%)* |
| *Equal-weight countries Sharpe (max DD)* | *0.348 (−41.0%)* | *0.191 (−41.0%)* | *1.187 (−15.5%)* |

It failed 4 of the 5 scored acceptance tests written before the first run (§8). Line 5 was recorded as not applicable before the lock. Line 6 failed (14 out-of-sample round trips against a floor of 24), so the verdict is Inconclusive. Lines 1, 2, and 4 failed as well. Line 3 passed.

**Why.**

1. **The out-of-sample daily Sharpe is −0.770 against a required 0.5.** Profit factor on the 14 trips that entered on or after 2024-07-01 is 2.060, which clears 1.10. Line 1 requires both, so it fails. The daily path includes six positions that were opened before 2024-07-01. Those trips stay in the in-sample trade count. Their later marks sit in the out-of-sample Sharpe. The two numbers answer different locked definitions.
2. **The beta rank sits inside both nulls.** Direction-placebo p is 0.686 against a required 0.05. Actual gross Sharpe is −0.157. The null mean is −0.002 and its 95th percentile is 0.473. The timing placebo, which permutes betas across eligible names, has p 0.747 and a null mean of +0.020.
3. **There is no gross edge.** At 0 bp the full-sample return is −40.9% and the out-of-sample Sharpe is −0.757 (`cost_sweep`). Full-sample total return at 10 bp is −46.4%, so line 4 fails. One extra session of fill delay leaves the out-of-sample Sharpe at −0.765.
4. **The short book lost more price P&L than the long book made.** Full-sample gross price P&L is +0.860 on the long book and −1.249 on the short book. Prediction 1 required both to be positive. It is not consistent. Short-side profit factor is 0.320 on 57 trips. Long-side profit factor is 2.136 on 62 trips.
5. **The out-of-sample trip count is 14 against a floor of 24.** That is line 6, and the locked status rule makes the verdict Inconclusive when line 6 fails. The other failed lines stay failed.
6. **The full-sample Sharpe interval contains zero.** The 20-session block bootstrap 95% interval is −0.674 to +0.264. The share of draws with Sharpe ≤ 0 is 0.771. This check is not an acceptance line. It does not soften lines 1, 2, or 4.

In-sample Sharpe is +0.006, so line 3 passes, and 3 of 5 grid cells have in-sample Sharpe above zero, which is the minimum the rule set. The in-sample compound return is −9.5%.

**Recommendation.** Do not trade it. There is no paper-trading proposal. The locked rules do not allow the result to be replaced by the long side, the 126-session grid cell, a dividend adjustment, a shorter beta window, or dropping 2026.

## 2. The strategy

### Rules

On the last NYSE session of each completed calendar month that has a SPY daily bar, on or before 2026-10-01:

```
A name is eligible when it has 252 paired simple returns with SPY
ending on the signal date. A missing session is dropped from the pair.
Beta = cov(r_i, r_SPY) / var(r_SPY), OLS with an intercept.
If fewer than 6 names are eligible, the book is flat.
Otherwise sort by beta ascending, ties by symbol ascending.
Long the first 3 at +1/3. Short the last 3 at -1/3.

Fill at the next session's open. Hold shares until the next fill.
At the open, old shares earn the gap. Size off mark-to-open equity
before cost. Charge 5 bp on the absolute notional of the share change.
New shares earn the open-to-close.
A missing bar earns 0 that day and is not forward-filled into the beta.
No stop, no volatility target, no momentum overlay.
Positions still open on 2026-10-01 are marked to the close
with reason sample_end and with no exit cost.
```

- **Why beta, and why equal weights.** The mechanism is leverage aversion (Frazzini and Pedersen 2014). Their published factor shrinks beta toward 1 and scales the book to be beta-neutral. This rule is the simpler long-short of ranked betas the study locked: three and three, weights +1/3 and −1/3. The signal is the beta, not the past return.
- **Why 252 sessions.** One trading year, fixed before the run. The grid varies that window and is not used to choose it.
- **Why the next open.** The signal uses the close, which is known at 16:00 ET. The fill is the next session's open. The same-bar close is a labelled upper bound.
- **Why 5 bp.** These country ETFs are thinner than SPY, and the store has no quotes. Five basis points is the round prior written into the rules. It was not changed after the P&L.
- **Why 2026-10-01 is not a signal.** October 2026 continues after the shared SPY series. The last signal is 2026-09-30. Country bars on 2026-10-02 are ignored.

### How it trades

| | |
|---|---|
| Rebalance sessions | 177 of 3,688 evaluation sessions (`posthoc.json` `rebalance_sessions`) |
| Round trips | 119. 8.13 per 252 sessions (`posthoc.json` `trades_per_year`) |
| Time in market | 1.0. Mean gross exposure 2.013 |
| Holding time | median 81 sessions, mean 186.9. Out of sample, median 32.5, mean 73.9 |
| Long / short | 62 / 57 trips. Profit factor 2.136 / 0.320. Average net trade +131.5 bp / −216.2 bp |
| Win rate | 51.3%. Average winner +0.0356, average loser −0.0450 |
| Exit reasons | `flat` 113 (profit factor 1.067, net P&L +0.123). `sample_end` 6 (profit factor 0.275, net P&L −0.560, median hold 659.5). `flip` 0 |

Zero flips is the path the independent replay reproduced. Names left a side by going flat. The same-side resize stays inside the open trip.

## 3. Hypothesis and predictions

Investors who cannot use leverage bid up high-beta assets, so low beta earns more per unit of risk than the CAPM allows (Black 1972; Frazzini and Pedersen 2014). The other side of this book is a leverage-constrained buyer of high-beta countries. The published factor is beta-neutral. This study is dollar-neutral and equal-weight. The prices mix the local equity market with the currency against the dollar. The result was published in 2014, so post-publication decay was a listed counter-force before the run. The out-of-sample window was already known to be a rising US equity market.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Full-sample gross price P&L is positive on the long book and positive on the short book | Long +0.8597802628188786, short −1.2488499771387491 (`long_gross_pnl`, `short_gross_pnl`) | Not consistent |
| The strategy's full-sample correlation with SPY is lower than the equal-weight country's correlation with SPY | Strategy −0.37317315142019586, equal-weight +0.8286769445741599 | Consistent |

Prediction 2 holds, and the book still lost money. The mechanism's P&L claim does not. A pass on prediction 2 with a failed book is a short-beta correlation, which §7 shows on the SPY-day quintiles. It is not a confirmed premium.

## 4. Method

- **Data.** Daily bars through `agent-data/mdq.py`, split-adjusted. SPY has 3,959 bars, 2011-01-04 through 2026-10-01. Each country ETF has 3,960 bars, 2011-01-04 through 2026-10-02. The extra session is 2026-10-02 and is excluded. Through 2026-10-01 every name matches SPY's sessions. The book skips 2012-10-29, 2012-10-30, and 2018-12-05, the three `needs_attention` closures with no bars. 2021-12-31 is a real session and is included. 2025-01-09 is not a session. Stored splits, confirmed before the lock: EWJ 0.25 on 2016-11-07, and EWS, EWU, and EWT 0.5 on the same date. Adjusted ex-date close ratios versus 2016-11-04 sit between 1.007 and 1.034. Raw ratios sit near 1/split_ratio. The script refused to write results if those ratios failed the locked bounds. No dividend row is stored. Price returns omit distributions. If low-beta countries have higher yields, this accounting understates the long side. Yields are unknown here.
- **Pre-registration.** `RULES.md` fixed the universe, the beta, the book, the cost, the sample split, both predictions, the grid, the placebos, and the acceptance lines before any return on these series. No earlier study in the repo used these twelve tickers or a beta rank. The out-of-sample window is not unseen. Those reports already showed a rising equity market: SPY close-to-close about +40.5% with a Sharpe near 0.985. This rule is low-minus-high beta. It is not the sector-momentum, commodity-momentum, currency-trend, overnight, or Treasury-trend rule. `RULES.md` was locked and was not committed. The lock records git HEAD `577dd38c7b98928f8fe9a696aff046425df9dafa` and `dirty=yes`. Verification later recorded HEAD `738d3e7ec4340e05458317114dfe247d55a1ef16`, an unrelated commit. The study directory was still untracked.
- **Fills and costs.** Base fill is the next open. Cost is 5 bp per side on the absolute notional of the share change. The store has no quotes, so the cost is the prior in the rules, not a measured spread.
- **Returns.** Daily simple returns. A flat day inside the window counts as 0. Sharpe is the mean divided by the sample standard deviation (`ddof=1`), times √252, with a zero risk-free rate. A trip is assigned by its entry date. Daily Sharpe is assigned by the session date.
- **Verification.** The self-test on synthetic sessions passed, including a missing bar, a deferred fill, a same-side resize, a flip path, and a same-bar close. On this store every name has a bar on every book session, so the missing-bar paths do not bind. `verify.py` shares no signal code with `backtest.py`. It rebuilt every trade. Both verification log entries match 119 trades against 119, and 40 fill-date books drawn with seed 20261064. Recomputed daily nets matched within 1e-8. The recomputed out-of-sample Sharpe in the log is −0.7700316214621308 against −0.7700316214621309 in `results.json`, a one-ulp difference. `python -m research.kit guard research/country-bab/research/verify.py` printed `ok`.
- **Runs.** One store run of `backtest.py`, reason `initial pre-registered run`, at 2026-10-02 21:20 UTC. Two verification replays of the same trades, both matches. The second replay followed removal of an unused helper in `verify.py`. The headline numbers did not change. No bug fix moved the result. `results.json` carries rules hash `78c6d6ac7a99240adcebd98b13af8e9f86e6b533edc8e483f4377575a138130b`, the same value as `RULES.lock`.

## 5. Results

![Growth of $1](figures/equity.svg)

The strategy finishes at 0.56 while the equal-weight country book finishes at 1.94 and SPY finishes at 5.82. The loss deepens after the out-of-sample line.

![Drawdown](figures/drawdown.svg)

The strategy's full-sample max drawdown is −56.3%. The out-of-sample window's own max drawdown, restarted at 1 on 2024-07-01, is −54.1%.

| Strategy (5 bp) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | −43.8% / −0.179 / −56.3% | 0.006 | −37.9% / −0.770 / −54.1% |
| Gross, 0 bp | −40.9% / −0.157 / −54.7% | 0.031 | −37.4% / −0.757 / −53.9% |
| *Equal-weight countries, 0 bp* | *+93.5% / 0.348 / −41.0%* | *0.191* | *+55.0% / 1.187 / −15.5%* |
| *SPY close-to-close* | *+482.3% / 0.806 / −34.1%* | *0.773* | *+40.5% / 0.985 / −19.9%* |

SPY is reported. SPY is not the hurdle. The hurdle is the equal-weight country book.

![Calendar-year return](figures/by_year.svg)

2026, through 1 October, is the worst year on the strategy (−38.6%) and is inside the out-of-sample window. Calendar 2024 straddles the sample split, so that row is not the out-of-sample result.

| Year | Sessions | Strategy | Sharpe | Max DD | Equal-weight | SPY |
|---|---:|---:|---:|---:|---:|---:|
| 2012 | 230 | −5.5% | −0.60 | −9.6% | +6.9% | +8.4% |
| 2013 | 252 | +8.2% | 0.78 | −9.4% | +6.3% | +29.7% |
| 2014 | 252 | +8.5% | 0.76 | −9.3% | −7.2% | +11.3% |
| 2015 | 252 | +8.4% | 0.63 | −8.1% | −13.7% | −0.8% |
| 2016 | 252 | −18.0% | −1.15 | −23.1% | +7.0% | +9.6% |
| 2017 | 251 | −4.6% | −0.30 | −11.3% | +22.8% | +19.4% |
| 2018 | 251 | −1.1% | −0.02 | −9.4% | −14.8% | −6.3% |
| 2019 | 252 | −3.7% | −0.33 | −12.8% | +17.4% | +28.8% |
| 2020 | 253 | +1.6% | 0.18 | −20.6% | +5.0% | +16.2% |
| 2021 | 252 | +14.4% | 1.08 | −12.6% | +5.0% | +27.0% |
| 2022 | 251 | +3.7% | 0.37 | −10.5% | −15.1% | −19.5% |
| 2023 | 250 | −6.1% | −0.59 | −9.7% | +13.0% | +24.3% |
| 2024 | 252 | +2.9% | 0.29 | −13.9% | −4.7% | +23.4% |
| 2025 | 250 | −12.3% | −0.65 | −24.6% | +34.8% | +16.4% |
| 2026 | 188 | −38.6% | −1.76 | −41.3% | +19.0% | +12.0% |

2012 starts on 1 February. 2026 ends on 1 October. 2022 was a down year for SPY (−19.5%) and for the equal-weight book (−15.1%). The strategy's 2022 return was +3.7%.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trip's dates and flips the sign of that trip's daily gross dollars. Two thousand draws, seed 20261061. Actual gross Sharpe is −0.157. The null mean is −0.002, the null 95th percentile is 0.473, and p is 0.686. A negative result inside a sign-flip null means the beta rank did not beat a coin flip on the same holding periods.

The timing placebo permutes the betas of the eligible names at each rebalance and resimulates at 5 bp. Five hundred draws, seed 20261063. The null mean gross Sharpe is +0.020, the 95th percentile is 0.451, and p is 0.747. The shuffled ranks have a higher average gross Sharpe than the real beta rank. This p is not an acceptance line.

### Bootstrap

Circular blocks of 20 sessions, 2,000 draws, seed 20261062. The 2.5th and 97.5th percentiles of the full-sample net Sharpe are −0.674 and +0.264. The share of draws with Sharpe ≤ 0 is 0.771. The out-of-sample t-stat of the daily mean is −1.15. The interval contains zero. The point estimate still failed lines 1, 2, and 4.

### Parameter plateau

![Parameter grid](figures/grid.svg)

Three of five in-sample Sharpes are above zero. The primary window, 252, ranks 3rd of 5 in sample. Spearman rank correlation of the five in-sample Sharpes with the five out-of-sample Sharpes is 0.900. Every out-of-sample Sharpe is negative. The best in-sample cell, 126 sessions, has out-of-sample Sharpe −0.557 and out-of-sample return −29.8%. Nothing was selected from the grid.

| Window | First date on the primary calendar | IS Sharpe | OOS Sharpe | IS return | OOS return |
|---|---|---:|---:|---:|---:|
| 126 | 2011-08-01 | 0.245 | −0.557 | +34.4% | −29.8% |
| 189 | 2011-11-01 | 0.058 | −0.705 | −1.4% | −35.3% |
| **252** | **2012-02-01** | **0.006** | **−0.770** | **−9.5%** | **−37.9%** |
| 315 | 2012-05-01 | −0.053 | −0.999 | −17.5% | −45.7% |
| 378 | 2012-08-01 | −0.152 | −0.998 | −29.2% | −45.7% |

The 252 cell matches the primary in-sample Sharpe. Cells that filled before 2012-02-01 contribute the carried position on the primary dates. Sessions before a cell's own first fill count as 0.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× (2.5 bp) | **1× (5 bp)** | 2× (10 bp) | 3× (15 bp) |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.157 | −0.168 | **−0.179** | −0.200 | −0.222 |
| OOS Sharpe | −0.757 | −0.763 | **−0.770** | −0.783 | −0.796 |
| Full-sample return | −40.9% | −42.4% | **−43.8%** | −46.4% | −49.0% |
| OOS return | −37.4% | −37.6% | **−37.9%** | −38.3% | −38.8% |

The book loses at zero cost. There is no break-even cost above zero on this sample. Filling one book session later (first fill 2012-02-02, 3,687 sessions) gives full-sample Sharpe −0.166, full-sample return −42.2%, and out-of-sample Sharpe −0.765. The same-bar close, a labelled upper bound, gives full-sample Sharpe −0.167 and out-of-sample Sharpe −0.771.

### Other markets (identical rules)

No second country-ETF family is in the store. Line 5 was recorded as not applicable before the lock. It is not a pass and not a fail.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

The figure plots the stored mean daily return in basis points. The stored `strategy_return` inside a bin compounds only the days in that bin. It is not a holding-period path, and it is not used below. Quintile 1 is the worst SPY days. The mean strategy return is positive on the down days and negative on the up days.

| Quintile | Sessions | Mean strategy | Mean SPY |
|---|---:|---:|---:|
| Q1 | 738 | 0.005245 | −0.013198 |
| Q2 | 738 | 0.001359 | −0.002740 |
| Q3 | 738 | −0.000487 | 0.000695 |
| Q4 | 737 | −0.001512 | 0.004486 |
| Q5 | 737 | −0.005160 | 0.013445 |

That pattern matches the correlation in prediction 2. It was not used to filter the rule.

| Side | Trips | Profit factor | Win rate | Net P&L | Gross P&L | Median hold |
|---|---:|---:|---:|---:|---:|---:|
| Long | 62 | 2.136 | 67.7% | +0.836 | +0.860 | 84.5 |
| Short | 57 | 0.320 | 33.3% | −1.273 | −1.249 | 45 |

| Exit | Trips | Profit factor | Net P&L | Median hold | Mean hold |
|---|---:|---:|---:|---:|---:|
| flip | 0 | — | 0 | — | — |
| flat | 113 | 1.067 | +0.123 | 63 | 170.3 |
| sample_end | 6 | 0.275 | −0.560 | 659.5 | 499.2 |

Six positions were open across 2024-07-01. The nets below are the full trip (`posthoc.json` `carried_into_oos`). They are not an out-of-sample slice, and they are not added up to produce the −37.9% out-of-sample return.

| Symbol | Side | Entry | Exit | Reason | Hold | Full-trip net P&L |
|---|---|---|---|---|---:|---:|
| EWA | short | 2022-06-01 | 2024-11-01 | flat | 610 | −0.034704 |
| EWY | short | 2023-11-01 | 2026-10-01 | sample_end | 731 | −0.459566 |
| EWL | long | 2024-01-02 | 2026-10-01 | sample_end | 690 | +0.083291 |
| EWT | short | 2024-01-02 | 2026-10-01 | sample_end | 690 | −0.296501 |
| EWJ | long | 2024-03-01 | 2024-08-01 | flat | 106 | −0.005874 |
| EWU | long | 2024-04-01 | 2026-10-01 | sample_end | 629 | +0.102729 |

The 14 out-of-sample entries, which are the profit-factor sample, split as 3 long trips (net P&L +0.188906, all winners) and 11 short trips (net P&L −0.066927, win rate 0.364) in `posthoc.json` `oos_entry_by_side`. The stored out-of-sample trip net P&L is +0.121979. The stored out-of-sample compound return is −37.9%. Both are reported. The 14 trips are not promoted.

Net P&L by symbol, full sample. This was not used to drop a name. EWY is the largest loss.

| Symbol | Trips | Win rate | Net P&L | Gross P&L |
|---|---:|---:|---:|---:|
| EWA | 8 | 12.5% | −0.268 | −0.264 |
| EWC | 8 | 37.5% | −0.132 | −0.129 |
| EWG | 11 | 27.3% | −0.115 | −0.110 |
| EWH | 9 | 77.8% | +0.257 | +0.261 |
| EWJ | 13 | 23.1% | +0.089 | +0.094 |
| EWS | 16 | 68.8% | −0.184 | −0.179 |
| EWU | 9 | 77.8% | +0.157 | +0.160 |
| EWW | 13 | 69.2% | +0.043 | +0.048 |
| EWZ | 4 | 25.0% | +0.001 | +0.005 |
| EWL | 10 | 80.0% | +0.178 | +0.182 |
| EWT | 8 | 62.5% | −0.069 | −0.066 |
| EWY | 10 | 30.0% | −0.395 | −0.391 |

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. Out-of-sample Sharpe and profit factor | Sharpe ≥ 0.5 and profit factor ≥ 1.10 | Sharpe −0.7700316214621309, profit factor 2.06021564381288 | ❌ |
| 2. Direction placebo | p ≤ 0.05 | p 0.6861569215392304 | ❌ |
| 3. In-sample plateau | IS Sharpe > 0 and at least 3 of 5 grid cells with IS Sharpe > 0 | IS Sharpe 0.00571657328501597, 3 of 5 cells | ✅ |
| 4. Full-sample return at 2× cost | Total return > 0 at 10 bp | −0.4644890822999772 | ❌ |
| 5. Cross-market | Not applicable. No second country-ETF family | Not scored | — |
| 6. Out-of-sample round trips | At least 24 | 14 | ❌ |

Line 3 passes on Sharpe. The in-sample compound return is −9.5%. A direction-placebo pass would still be a low bar, because the placebo flips gross dollars and the cost stays out of the numerator. This study did not pass it. Line 6 sets the status to Inconclusive even though lines 1, 2, and 4 also fail. The status is not Rejected, and it is not a paper-trading candidate.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

The trailing 252-session Sharpe **(post hoc)** ends at −1.613. Of 3,437 windows, 53.5% are positive. The minimum is −2.761, ending 2026-05-08. The maximum is 1.707, ending 2025-04-03.

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | Full-sample gross Sharpe is −0.157. Out-of-sample net Sharpe is −0.770. The 126-session cell, best in sample, is −0.557 out of sample. | No paper trial. A new rule needs its own `RULES.md`. |
| Concentration in a few days | **(post hoc)** Setting the 20 best and 20 worst `strategy_net` days to 0 leaves full-sample Sharpe −0.243 and full-sample return −45.8%. The out-of-sample Sharpe becomes −1.060. | The loss is still there after the tails are removed. |
| Generalization | One hand-picked family of twelve ETFs. Line 5 has no second family. Prediction 1 failed on the short book. | The universe stays the twelve names that were locked. |
| Execution | Loss at 0 bp. One session of delay leaves out-of-sample Sharpe at −0.765. No quotes in the store. The 5 bp prior was not the cause. | Cost stays at the locked 5 bp in this study. |
| Sample and regime | 14 out-of-sample entries over 27 month-end signals. The window was already known to be a rising equity market, and the book is short beta on up days (§7). 2026 through 1 October is −38.6%. | 2026 stays in the sample. The trip floor was not lowered after the run. |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. Fourteen out-of-sample round trips also leave the status at Inconclusive. A different rule, a dividend series, or a later sample is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

None of the following changes §8.

- **Trailing 252 sessions (post hoc).** From 2025-10-01 through 2026-10-01 the strategy returned −42.0% (Sharpe −1.613). SPY returned +15.1% (Sharpe 1.115). The equal-weight book returned +23.8% (Sharpe 1.243). Source: `posthoc.json` `trailing_252`.
- **Rolling Sharpe (post hoc).** See §9. The last window matches the trailing-252 Sharpe.
- **Tail days (post hoc).** Zeroing the 10 best and 10 worst strategy days leaves full-sample Sharpe −0.225 and return −45.8%, and out-of-sample Sharpe −0.993 and return −37.1%. Zeroing 20 and 20 is in §9. Source: `posthoc.json` `concentration`.
- **Out-of-sample entries by side (post hoc).** Three long trips and eleven short trips, as cited in §7. The side split was not used to keep one side.

### Ideas for a new study

Each of these needs its own `RULES.md` and data this study did not use.

- The published beta-neutral betting-against-beta construction, with shrinkage toward 1. This study locked equal weights and did not build that factor.
- A total-return test once a dividend series exists. The store has none. If low-beta countries have higher yields, the price-return accounting understates the long side. The yields are unknown, so this study does not claim that a dividend adjustment would have changed the verdict.
- Betting against beta inside one country's stocks. These legs are country ETFs.

## 12. Reproduce

From the repo root:

```bash
python research/country-bab/research/backtest.py
```

```bash
python research/country-bab/research/verify.py
```

```bash
python research/country-bab/research/posthoc.py
```

```bash
python research/country-bab/research/charts.py
```

`backtest.py` writes `results.json`, `daily.csv`, `trades.csv`, `placebo_direction.npy`, `placebo_timing.npy`, and appends `RUNLOG.md`. It recomputes the `RULES.md` hash and refuses a mismatch. `verify.py` replays the store, checks every trade, and appends `RUNLOG.md`. `posthoc.py` reads the saved files and writes `posthoc.json`. `charts.py` reads those files and writes the eight SVGs under `report/figures/`. Seeds are 20261061 (direction), 20261062 (bootstrap), 20261063 (timing), and 20261064 (verify). A rerun of the backtest on the same store and the same rules reproduces the same numbers. The self-test does not open the store: `python research/country-bab/research/backtest.py self-test`.

### Checklist

- `RULES.md` matches `RULES.lock`, and `results.json` carries that hash.
- `results.json` has no `kit_schema` field. The store run was written at lock HEAD `577dd38` and was not rewritten onto the later kit schema.
- `python -m research.kit guard research/country-bab/research/verify.py` printed `ok`.
- Both verification entries in `RUNLOG.md` have the permitted reason, and both matched.
- The self-test passed. `verify.py` matched all 119 trades and 40 signal books.
- The status is Inconclusive, which is what line 6 requires.
- Every post-hoc number above is labelled, and none of them enters §8.
- Prior exposure and the three skipped closures are in §4. The 2026-10-02 bars are excluded.
- `research/README.md` was not updated. The request forbade edits outside `research/country-bab/`.
- Nothing was written to `data/`, and `ingest` was not run.

### References

- Frazzini, A., and Pedersen, L. H. (2014). Betting against beta. *Journal of Financial Economics*, 111(1), 1–25.
- Black, F. (1972). Capital market equilibrium with restricted borrowing. *Journal of Business*, 45(3), 444–455.
- McLean, R. D., and Pontiff, J. (2016). Does academic research destroy stock return predictability? *Journal of Finance*, 71(1), 5–32.
