# Treasury ETF time-series momentum: TLT and IEF

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Inconclusive.** Four of the seven scored acceptance rows failed. Line 6 failed, so the locked rule sets this status even though lines 1 and 2 also failed. |
| Instruments | TLT and IEF, one book, monthly next-open fills, held between month-ends. |
| Data | Daily bars 2011-01-04 through 2026-10-01, read via `agent-data/mdq.py`. Evaluation 2012-02-01 through 2026-10-01. NYSE closures 2012-10-29, 2012-10-30, and 2018-12-05 are skipped. |
| Rules | [`research/treasury-etf-trend/research/RULES.md`](../research/RULES.md), locked 2026-10-02T19:50:10Z, sha256 `2e570c264cb3` |
| Code | [`research/treasury-etf-trend/research/`](../research/) · 3 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The locked rule is long or short each of TLT and IEF at weight 1/2 when that fund's own prior 252-session price return is positive or negative, rebalanced at the next open after each month-end, at 1 bp per side. Out of sample, 2024-07-01 through 2026-10-01, the book returned −10.52% (Sharpe −0.65, profit factor 0.55, 16 round trips, max drawdown −21.1%) over 566 sessions. The full sample, 2012-02-01 through 2026-10-01, returned +10.33% (Sharpe +0.12, 44 round trips, max drawdown −29.0%). In sample, through 2024-06-28, the book returned +23.31% (Sharpe +0.22).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2012-02-01 → 2026-10-01 | 2012-02-01 → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,688 | 3,122 | 566 |
| Total return | +10.33% | +23.31% | −10.52% |
| CAGR | +0.67% | +1.71% | −4.83% |
| Annual volatility | 9.88% | 10.29% | 7.21% |
| Sharpe | +0.12 | +0.22 | −0.65 |
| Max drawdown | −29.0% | −29.0% | −21.1% |
| Trades / profit factor | 44 / 1.24 | 28 / 1.70 | 16 / 0.55 |
| Avg net trade | +34.2 bp | +75.1 bp | −37.3 bp |
| *Benchmark Sharpe (max DD)* | −0.14 (−42.4%) | −0.09 (−40.8%) | −0.50 (−16.8%) |

Percentages and Sharpes in this table are `results.json` rounded for display. Section 8 quotes the acceptance values as stored. It failed four of the seven scored acceptance tests written before the first run (§8). Line 5 was not scored.

**Why.**

1. Out of sample there is no gross edge. At 0 bp the OOS Sharpe is −0.64 and the OOS return is −10.37%. At 1 bp they are −0.65 and −10.52%. Costs are not why the OOS test failed.
2. The full-sample gross Sharpe is +0.12. The direction placebo p is 0.31. The timing placebo p is 0.38. The bootstrap 95% interval of full-sample net Sharpe is −0.35 to +0.58.
3. Line 6 required at least 24 OOS round trips and the book has 16. The locked status rule therefore says Inconclusive. Lines 1 and 2 also fail. Lines 3 and 4 pass.
4. The full-sample gain is not broad. On the 1 bp path the long side's net dollars are −0.080 and the short side's are +0.183. Calendar 2022, inside the in-sample window, returned +30.92% with no new round trip started: both funds were already short from entries on 2021-03-01. The futures study's rates sleeve, a different implementation, lost money on Treasury futures in the overlapping OOS window, and the loss was larger in longer duration. That was known before this lock. It is prior exposure. It is not a reason to change this rule.
5. The crisis-convexity prediction is consistent on the full window, and the book still lost money out of sample. Equity studies had already described that OOS calendar as a rising market. The prediction is not an acceptance line.
6. The mechanism is not established as a trading rule.

**Recommendation.** Do not trade it. The 189-session grid cell, the short side, a 1/3/12 blend, a volatility target, a coupon adjustment, a short-Treasury leg, and ZN or ZB futures are not substitutes for the primary.

## 2. The strategy

### Rules

```
signal dates = last NYSE session of each calendar month that has a SPY daily bar
skip 2012-10-29, 2012-10-30, 2018-12-05
for each of TLT and IEF, on its own daily bars, no forward fill:
    if the signal date has no bar, or fewer than 252 prior own bars:
        target = 0
    else:
        target = sign(close[t] / close[t-252] - 1) / 2    # sign(0) = 0
fill at the next session's open
size off mark-to-open equity before cost; denominator stays 2
charge 1 bp per side on absolute notional traded
old shares earn the gap; new shares earn open-to-close
a missing bar earns 0
no stop; shares constant between fills
open trips at the last close are marked end_of_sample, with no extra exit cost
```

- **Why one 252-session lookback:** it is the 12-month leg in Moskowitz, Ooi, and Pedersen (2012). It was fixed before any return on TLT or IEF was computed. The 21-session and 63-session legs are not in this book.
- **Why weight 1/2 with a fixed denominator:** two funds, one book. A flat fund is not a reason to double the other.
- **Why 1 bp:** the skill default for SPY-class liquidity. TLT and IEF are in that class. The cost was not changed after the P&L.

### How it trades

| | |
|---|---|
| Sessions with a trade | 34 entry sessions in the evaluation window (`entry_sessions`) |
| Trades per year | 3.01 full sample; 7.12 out of sample |
| Time in market | Exposure 1.0 in every window. Both funds printed zero exact-zero 12-month returns, so the book was never flat at a close. |
| Holding time | Full sample median 83 sessions, mean 167.6. Out of sample median 65, mean 66.6. |
| Long / short | Full sample 22 long, profit factor 0.69, net dollars −0.080; 22 short, profit factor 2.10, net dollars +0.183. |
| Win rate | Full sample 40.9%. Average winner 316 bp, average loser −161 bp. The stored ratio of those two averages is 1.97. Out of sample the win rate is 31.3%. |

Exit reasons recorded in `results.json` are `flip` 42 and `end_of_sample` 2. A round trip is in-sample or out-of-sample by its entry date. The TLT short entered 2021-03-01 and the IEF short entered the same day are in-sample trips. Their daily P&L in 2022 sits in the in-sample window. Their later daily P&L, through each exit, sits in whichever window contains that session.

Eligible month-end signals: TLT +74 / −104 / 0 exact zeros, 12 ineligible; IEF +88 / −90 / 0 exact zeros, 12 ineligible.

## 3. Hypothesis and predictions

TLT and IEF were hypothesized to continue in the direction of their own prior 252-session price return over the next month. The stated other side is a hedger, a mortgage-convexity desk, or a rebalancer selling into a rally and buying into a decline. The citation locked with the rule is Moskowitz, Ooi, and Pedersen (2012). McLean and Pontiff (2016) was named as a reason a published result can shrink. The OOS window was already known, from `micro-futures-trend`, to be a period when a cousin of this rule lost money on Treasury futures.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| TLT and IEF have the same sign of full-sample gross P&L. | TLT gross dollars +0.0606, IEF gross dollars +0.0474, on the 1 bp share path. Same sign. These are sums of gap and open-to-close dollars with starting equity 1, not compounded returns. | Consistent |
| In calendar months where SPY close-to-close is in its worst quintile, the strategy's mean monthly return is above its mean in the middle quintile. | Q1 mean +1.55% (n=36). Q3 mean +0.02% (n=35). | Consistent |

Both predictions can be consistent and the book can still fail the test it was written to pass. They do not confirm a tradeable edge, and they do not change §8. The second prediction is the crisis-convexity claim. It is not an acceptance line. Q5, the best SPY months, has strategy mean −0.97%.

## 4. Method

- **Data.** `data/market-data.sqlite` through `agent-data/mdq.py`, schema user_version 6. The pre-lock script `counts.py` printed counts only. TLT and IEF each have 3,959 daily bars, 2011-01-04 through 2026-10-01, no duplicate sessions, and `action_counts` 0. Each has 0 one-minute bars. SPY has the same 3,959 daily sessions and also has one-minute bars. Book sessions against the SPY calendar, after skipping the three closures, are 3,959, and neither fund is missing a bar on those sessions. No coupon and no ETF distribution is stored. A long is understated, and a short is overstated, by about the distribution yield times exposure. Price volatility of these two funds is large relative to a year's coupon, which is why the universe is these two and not a short-Treasury fund. No coupon was booked after the results. Stored SPY daily bars from late 2024 are not regular-hours prints; that was already known from `spy-rsi2-dip-buy`. This study uses the stored daily bars. SPY close-to-close inherits that defect. SPY is not the hurdle.
- **Pre-registration.** `RULES.md` fixed the universe, the single lookback, the weights, the fill, the cost, the split, the predictions, the grid, and the acceptance lines before any TLT or IEF return was computed. The hash is `2e570c264cb3732130a73c7a3e927760d701a705db568b293e42bbb111565eb4`. `results.json` carries the same hash. The user asked for the study to be run and forbade a git commit, so `RULES.md` was not committed. Logged `git_head` is `8068750d3850669df5441a076ec108499b4acd44` with `dirty=yes`. Prior exposure, written before the lock: no earlier study used TLT or IEF. `micro-futures-trend` (Rejected) used a 1/3/12 blend, a volatility target, whole micro contracts, and equities and commodities in the same book. Its rates sleeve had IS Sharpe +0.10, OOS Sharpe −1.12, full net −$12,676 (Sharpe −0.42), IS P&L +$865, OOS P&L −$13,541. By market, full / OOS Sharpe was ZT −0.21 / −0.62, ZF −0.23 / −0.53, ZN −0.42 / −1.03, ZB −0.61 / −1.49. That study's OOS, 2024-10 through 2026-09, sits inside this OOS. Its rules file stated, as the author's prior knowledge, that Treasury yields rose into late 2023 and then ranged. That sentence is prior knowledge, not a path measured here. Equity-study OOS windows, mostly through 2026-09-25, were a rising market: QQQ about +55% (Sharpe about 1.0, drawdown about −23%), SPY about +42% (Sharpe about 1.0, drawdown about −19%), including the April 2025 tariff crash and the +10.5% SPY session on 2025-04-09. This study's in-sample window contains the 2022 bear market. This study's OOS is not unseen.
- **Fills and costs.** Base fill is the next open after the month-end close. The close is known at 16:00 ET (13:00 on an early close). The lookback uses 252 prior own bars. Quintiles, placebos, and the bootstrap use the finished sample and do not set a share quantity. Base cost is 1 bp per side of absolute notional. The share path is rebuilt at each cost in the sweep.
- **Returns.** Daily simple returns. A flat day inside the window counts as 0. Sharpe is the mean divided by the sample standard deviation (n−1), times √252, with a zero risk-free rate. Each window compounds from 1 on its first session. CAGR uses a 252-session year.
- **Verification.** `backtest.py` runs the synthetic cases before it opens the store: both long, both short, one flat with the other fund left at weight 1/2, a two-fund flip, a one-fund flip, a missing bar that does not book the gap and does not carry the old signal, a one-session fill delay, a same-close fill, and the end of the sample. The first attempt, before any store open, raised `AttributeError` because `_Trip` had no `hold` slot. That was fixed before any return was written, so it has no `RUNLOG` entry. Each of the three store runs then completed, which requires those cases to pass. `verify.py` shares no signal code with `backtest.py`. Its first invocation raised `KeyError` on the column name `reason`. The column is `exit_reason`. No trade was compared. The rerun at 2026-10-02T20:02:46Z matched 44/44 round trips on side, entry date and price, exit date and price, and exit reason. Seed 20261054 checked 40 rebalance dates. All 178 eligible dates were replayed.
- **Runs.** Three store runs. Headlines did not change. The 19:59:17Z rerun fixed a reporting bug: benchmark exposure had been written as 0 because the no-trade helper ignored the position flag. The share path, the trades, and the headline Sharpe and return were unchanged. Benchmark exposure in the current `results.json` is 1.0. The 20:04:45Z rerun saved the placebo draws for the figure and left the headlines unchanged. SPY's exposure field is 0 because that series is close-to-close, not a share path. This report does not use it. No deviation from the locked share-path rule was found.

## 5. Results

![Growth of $1](figures/equity.svg)

Growth of $1 over the full sample, with the 2024-07-01 split marked: the strategy ends +10.3% and the uncosted long-only TLT+IEF benchmark ends −25.3%, and past the dashed line the strategy returns −10.5%.

![Drawdown](figures/drawdown.svg)

Drawdown from the running peak: the strategy's maximum is −29.0%, reached in sample, and the out-of-sample maximum is −21.1%. The benchmark's maximum is −42.4%.

| Strategy (1 bp) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | +10.33% / +0.12 / −29.0% | +0.22 | −10.52% / −0.65 / −21.1% |
| *Long-only TLT+IEF, uncosted* | −25.31% / −0.14 / −42.4% | −0.09 | −10.04% / −0.50 / −16.8% |
| *SPY close-to-close, price return* | +482.34% / +0.81 / −34.1% | +0.77 | +40.52% / +0.98 / −19.9% |

SPY's full-sample CAGR in `results.json` is +12.79%. That is a price return on stored daily bars. It is not a total return, and it is not the hurdle. Out of sample the trend book returned −10.52% and the uncosted long-only bond book returned −10.04%. The full-sample comparison is strategy +10.33% against benchmark −25.31%. The full-sample t-stat is 0.45. The OOS t-stat is −0.98.

![Calendar-year return](figures/by_year.svg)

The strategy's calendar-year return is negative in 9 of 15 years. 2022 is +30.9% and is inside the in-sample window. 2026 is partial, 188 sessions through 2026-10-01. Calendar 2024 straddles the split: in-sample ends 2024-06-28 and out-of-sample starts 2024-07-01. Years with zero trades were not flat years. Exposure is 1.0, and no new round trip started.

| Year | Strategy | Sharpe | Max DD | Benchmark | SPY | Trades |
|---|---:|---:|---:|---:|---:|---:|
| 2012 | +1.35% | +0.20 | −6.80% | +1.36% | +8.45% | 2 |
| 2013 | +4.01% | +0.48 | −6.93% | −11.85% | +29.69% | 4 |
| 2014 | −0.17% | +0.02 | −8.05% | +14.95% | +11.29% | 2 |
| 2015 | −1.70% | −0.11 | −11.40% | −2.24% | −0.81% | 1 |
| 2016 | −11.61% | −1.34 | −13.95% | −0.89% | +9.64% | 7 |
| 2017 | −2.95% | −0.40 | −7.35% | +3.58% | +19.38% | 2 |
| 2018 | −8.62% | −1.45 | −8.24% | −2.73% | −6.35% | 4 |
| 2019 | +10.34% | +1.21 | −6.12% | +8.69% | +28.79% | 2 |
| 2020 | +12.67% | +0.91 | −10.38% | +12.67% | +16.16% | 0 |
| 2021 | −9.87% | −1.05 | −12.19% | −5.02% | +27.04% | 2 |
| 2022 | +30.92% | +1.96 | −10.95% | −25.11% | −19.48% | 0 |
| 2023 | −1.97% | −0.08 | −13.87% | +0.10% | +24.29% | 0 |
| 2024 | −6.02% | −0.61 | −14.60% | −7.82% | +23.40% | 4 |
| 2025 | −7.19% | −1.06 | −11.90% | +1.92% | +16.37% | 7 |
| 2026 | +8.47% | +1.86 | −2.57% | −9.07% | +12.05% | 7 |

2012 starts at the first fill, 2012-02-01, so that row has 230 sessions. Two `trades.csv` rows cover 2022. TLT short, entry 2021-03-01 at 140.66, exit 2024-09-03 at 97.56, reason `flip`, net dollars +0.1569, hold 883. IEF short, entry 2021-03-01 at 115.47, exit 2024-01-02 at 95.935, reason `flip`, net dollars +0.0821, hold 715. Both entries are before 2024-07-01, so both trips are in-sample. This report does not add those two net-dollar figures into a share of the +10.33% compounded return. The two open trips at the sample end are both short, entered 2026-07-01: TLT exit 77.7, net dollars +0.0501; IEF exit 89.3129, net dollars +0.0272. Exit reason `end_of_sample`. No extra exit cost. 2026 is a partial year.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each zero-cost trip's dates and flips that trip's gross dollars by an independent ±1. The daily placebo return divides the summed flipped dollars by the unflipped zero-cost equity at the previous close. Two thousand draws, seed 20261051. Actual gross Sharpe +0.12, null mean −0.005, null 95th percentile +0.34, p = 0.307. The actual Sharpe sits inside the null. p = (1 + count of draws with Sharpe ≥ actual) / 2001.

The timing placebo is not an acceptance line. Five hundred draws, seed 20261053, permute each fund's signal across its own eligible rebalance dates and keep its counts of positive, negative, and zero signals. Actual gross Sharpe is the same +0.12. Null mean +0.032, null 95th percentile +0.43, p = 0.381.

### Bootstrap

Circular 20-session blocks of the full-sample net daily returns, 2,000 draws, seed 20261052. The 2.5th and 97.5th percentiles of Sharpe are −0.35 and +0.58. The median is +0.11. The fraction of draws with Sharpe ≤ 0 is 0.309. The OOS t-stat of the mean daily return is −0.98.

### Parameter plateau

![Parameter grid](figures/grid.svg)

All 5 in-sample Sharpes are positive. The locked 252-session cell is rank 3 of 5 in sample and −0.65 out of sample. The IS/OOS Spearman rank correlation is −0.30. Nothing was selected.

| Lookback | IS Sharpe | OOS Sharpe | IS return | OOS return | First fill |
|---:|---:|---:|---:|---:|---|
| 126 | +0.270 | −0.680 | +34.23% | −11.93% | 2011-08-01 |
| 189 | +0.218 | +0.886 | +24.01% | +14.56% | 2011-11-01 |
| **252** | **+0.216** | **−0.651** | **+23.31%** | **−10.52%** | **2012-02-01** |
| 315 | +0.189 | +0.307 | +18.72% | +4.40% | 2012-05-01 |
| 378 | +0.144 | +0.005 | +12.00% | −0.56% | 2012-08-01 |

The in-sample winner is 126 sessions, and that cell's OOS Sharpe is −0.68. The 189-session cell's OOS Sharpe is +0.89. It was found on this data. It is not the primary, and it is not a candidate. The 315-session OOS Sharpe is +0.31, below the 0.5 line. The five OOS Sharpes are not a plateau: −0.68, +0.89, −0.65, +0.31, +0.005.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

Out of sample, Sharpe stays negative from 0 bp (−0.64) through 3 bp (−0.67). There is no break-even inside this sweep for the OOS loss. The full-sample return stays positive through 3 bp, which is why line 4 passes. Few round trips, about 3 per year on the full sample, are why the cost drag is small.

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | +0.121 | +0.119 | **+0.117** | +0.114 | +0.111 |
| OOS Sharpe | −0.641 | −0.646 | **−0.651** | −0.661 | −0.671 |
| Full-sample return | +10.85% | +10.59% | **+10.33%** | +9.81% | +9.30% |
| OOS return | −10.37% | −10.45% | **−10.52%** | −10.67% | −10.82% |

Filling one book session later (signal at t, open of t+2) gives full-sample Sharpe +0.13 and return +11.77%, and OOS Sharpe −0.74 and return −11.97%. First fill 2012-02-02, 3,687 sessions. The same-bar close fill is an upper bound, not a candidate: full-sample Sharpe +0.13 and return +12.07%, OOS Sharpe −0.54 and return −8.87%. First fill 2012-01-31, 3,689 sessions. Neither fill rescues the OOS loss.

### Other markets (identical rules)

Line 5 was recorded as not applicable before the lock. No third Treasury ETF is in the store. The futures rates series were already used in `micro-futures-trend` and were not reused as a line-5 instrument. The row is not scored.

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|
| Cross-market | not scored | not scored | not scored |

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Mean strategy return by SPY month quintile on the full evaluation window: Q1 is +1.55% and Q3 is +0.02%. Q5, the best equity months, is −0.97%. Quintiles rank the whole window after the fact. They are not a filter and they did not change a weight.

| Quintile | Months | SPY mean | Strategy mean | Benchmark mean |
|---|---:|---:|---:|---:|
| Q1 worst | 36 | −4.86% | +1.55% | −0.80% |
| Q2 | 36 | −0.37% | +0.47% | −0.05% |
| Q3 | 35 | +1.53% | +0.02% | +0.35% |
| Q4 | 35 | +3.01% | −0.67% | −0.28% |
| Q5 best | 35 | +6.30% | −0.97% | +0.15% |

Daily SPY quintiles, same window, strategy mean return: Q1 +0.153%, Q2 +0.022%, Q3 +0.025%, Q4 −0.028%, Q5 −0.149%. The sign pattern matches the monthly table. It was not used to drop a day.

By side, full sample, on the 1 bp path: long 22 trips, win rate 31.8%, profit factor 0.69, gross dollars −0.077, net dollars −0.080. Short 22 trips, win rate 50%, profit factor 2.10, gross dollars +0.185, net dollars +0.183. The short side is where the full-sample net dollars are positive. Missing coupons make a short look better than a total-return test and make a long look worse. The short side is not promoted. By fund, gross dollars are TLT +0.061 (24 trips) and IEF +0.047 (20 trips), which is prediction 1.

April 2025, the pre-registered V-shaped-reversal check, is 21 sessions: strategy +1.29%, benchmark −0.81%, SPY +0.41%. It is not an acceptance line.

The out-of-sample split by side was computed after the run. It is in §11.

## 8. Acceptance tests (fixed before the first run)

The locked status rule, written before the run: if line 6 fails, the status is Inconclusive even if another scored line also fails. If line 6 passes and any of lines 1 through 4 fails, the status is Rejected. If lines 1, 2, 3, 4, and 6 all pass, the status is Paper-trading candidate.

| Criterion | Required | Result | |
|---|---|---|---|
| 1 OOS Sharpe | ≥ 0.5 | −0.650837390695698 | ❌ |
| 1 OOS profit factor | ≥ 1.10 | 0.5527327696309291 | ❌ |
| 2 direction placebo p | ≤ 0.05 | 0.3073463268365817 | ❌ |
| 3 IS Sharpe | > 0 | 0.21581258588301078 | ✅ |
| 3 IS grid Sharpe > 0 | ≥ 3 of 5 | 5 of 5 | ✅ |
| 4 full return at 2 bp | > 0 | 0.09814438093067235 | ✅ |
| 5 cross-market | not applicable | not scored | not scored |
| 6 OOS round trips | ≥ 24 | 16 | ❌ |

Four scored rows failed and three passed. The OOS daily sample is 566 sessions, and the point estimate is negative. Failing the minimum sample does not replace that point estimate with a pass. A direction placebo on gross Sharpe is the comparison the rules specified; this book's p is 0.31.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

Trailing 252-session Sharpe **(post hoc)** is positive in 49% of 3,437 windows. The range is −2.28 to +2.55. The last window is +1.54, which includes the partial 2026 year.

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | IS Sharpe +0.22, OOS Sharpe −0.65. OOS profit factor 0.55. | None in this study. A later window needs its own `RULES.md`. |
| Concentration in a few days | **(post hoc)** Zeroing the 20 best strategy days changes the full-sample return from +10.33% to −31.23% and the Sharpe to −0.22. Those 20 days are 5.6% of the sum of positive daily returns and sum to 0.479 in simple return. **(post hoc)** Zeroing every 2022 daily return leaves full-sample return −15.73% and Sharpe −0.08. | Shown, not removed. 2022 stays in the sample. |
| Generalization | One book, two funds. Line 5 not scored. IS/OOS rank correlation −0.30. A cousin rule already lost on Treasury futures in the overlapping window, worst in long duration. | The futures result was disclosed before the lock. It does not retune this book. |
| Execution | OOS Sharpe −0.64 at 0 bp and −0.74 with a one-session delay. Same-close upper bound OOS Sharpe −0.54. No dividends are stored. The full-sample profit is on the short side, so the missing coupon makes that profit look better than a total-return test. | Cost stays at the pre-registered 1 bp. No coupon adjustment was added. |
| Short sample | 16 OOS round trips against a locked floor of 24. OOS t-stat −0.98. Bootstrap interval on the full sample includes zero and negative Sharpes. | The floor was not lowered after the run. The status stays Inconclusive. |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question: the minimum sample, the OOS Sharpe, and the OOS profit factor. A different rule, a coupon series, or data after 2026-10-01 is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

`posthoc.py` reads `daily.csv` and `trades.csv`. It does not open the store. None of these numbers enters §8.

- **(post hoc)** Rolling 252-session Sharpe: n=3,437, minimum −2.28, maximum +2.55, last +1.54, fraction positive 0.488.
- **(post hoc)** Zeroing the 20 best `strategy_net` days: total return −0.312, Sharpe −0.225. Sum of those days 0.479. Share of the sum of positive days 0.056.
- **(post hoc)** Zeroing all 2022 daily returns: total return −0.157, Sharpe −0.082.
- **(post hoc)** Zeroing the 10 best OOS days: OOS return −0.205, Sharpe −1.49. The OOS sample is already negative before those days are removed.
- **(post hoc)** Correlation of `strategy_net` with SPY close-to-close is −0.194 full sample and −0.114 out of sample. Correlation with the long-only benchmark is −0.094 full sample and −0.206 out of sample. Correlation of gross with net is 1.000.
- **(post hoc)** OOS round trips by entry date on or after 2024-07-01: long 8 trips, net dollars −0.115, win rate 0.125; short 8 trips, net dollars +0.040, win rate 0.50. These are dollar sums on the share path. The compounded OOS return is the −10.52% in §1. The short side out of sample is not a new rule.

### Ideas for a new study

A total-return test needs distribution data this store does not have, and a window that starts after 2026-10-01, with its own `RULES.md`. The 189-session cell and the short side were seen on this sample and are not candidates. A 1/3/12 blend with a volatility target was already rejected on Treasury futures in `micro-futures-trend`.

## 12. Reproduce

From the repo root:

```bash
python research/treasury-etf-trend/research/backtest.py
```

```bash
python research/treasury-etf-trend/research/verify.py
```

```bash
python research/treasury-etf-trend/research/posthoc.py
```

```bash
python research/treasury-etf-trend/research/charts.py
```

`backtest.py` checks the rules hash, runs the synthetic cases, then writes `results.json`, `daily.csv`, `trades.csv`, `placebo_direction.npy`, `placebo_timing.npy`, and appends `RUNLOG.md`. `verify.py` replays the trades and appends `RUNLOG.md`. `posthoc.py` writes `posthoc.json` and does not open the store. `charts.py` writes the eight SVGs in `report/figures/` from those files. Seeds: direction 20261051, bootstrap 20261052, timing 20261053, verify 20261054. Runtime was not recorded. The two reruns after the first store run left the headline Sharpe and return unchanged. A later run on the same store, with the same code, is expected to match those headlines. Nothing was written to `data/`. Ingest was not run.

### Phase 6 checklist

- The `RULES.md` hash matches `RULES.lock`, and `results.json` carries `2e570c264cb3732130a73c7a3e927760d701a705db568b293e42bbb111565eb4`.
- Each store rerun in `RUNLOG.md` has a permitted reason: a reporting bug, then saving the placebo draws. The verify column-name crash is logged as a note, not as a fourth store run. The pre-store `_Trip` crash produced no returns and has no log line.
- The synthetic cases pass before the store opens. `verify.py` matched 44/44 round trips after the column name was corrected.
- The status is Inconclusive, which is what the locked table requires when line 6 fails.
- Every post-hoc number above is labelled **(post hoc)** and none of them feeds §8.
- Prior exposure and the coverage gaps are in §4: three skipped closures, no minute bars for TLT or IEF, no dividends or coupons, and the SPY daily extended-hours defect.
- The commands above reproduce the outputs from the repo root, with the seeds recorded.
- This folder's `README.md` has the status, links, and commands. `research/README.md` was not updated. The request forbade editing that file. That checklist item fails.
- Nothing was written to `data/`, and `ingest` was not run. The strategy was not ported into the terminal.

### References

- Moskowitz, T. J., Ooi, Y. H., and Pedersen, L. H. (2012). Time series momentum. *Journal of Financial Economics*, 104(2), 228–250.
- McLean, R. D., and Pontiff, J. (2016). Does academic research destroy stock return predictability? *Journal of Finance*, 71(1), 5–32.
