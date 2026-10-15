# SSR next-session long: stored names

| | |
|---|---|
| Date | 2026-10-15 |
| Status | **Rejected.** Failed 6 of 10 pre-registered tests. |
| Instruments | Every stored name with a daily bar, except VIXY and UVXY. Equal-weight long, one cohort at a time, sold at the entry session's close. |
| Data | Book sessions 2015-03-25 through 2026-10-01, read via `agent-data/mdq.py`. One session in that span had no SPY bar and was omitted. |
| Rules | [`research/ssr-next-day-long/research/RULES.md`](../research/RULES.md), locked 2026-10-15T04:51:11+00:00, sha256 `33e478d8cfd4` |
| Code | [`research/ssr-next-day-long/research/`](../research/) · 2 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The rule buys, at the next session's open, every stored name whose short-sale restriction began on the previous session, and sells at that session's close. Out of sample, 2024-07-01 through 2026-10-01, the book returned **−94.1%** after 10 bp per side (Sharpe **−1.360**, profit factor **0.796**, 1,362 trades, max drawdown **−95.1%**). The average net trade in that window was **−16.3 bp**. Over the same 566 sessions, uncosted SPY open-to-close returned **+25.14%** (Sharpe 0.709, max drawdown −18.22%). The full sample, 2015-03-25 through 2026-10-01, returned −100.0% (Sharpe −0.915, 4,131 trades). At zero cost the out-of-sample return was −85.9% and the out-of-sample Sharpe was −0.835.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2015-03-25 → 2026-10-01 | 2015-03-25 → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 2898 | 2332 | 566 |
| Total return | −100.0% | −99.5% | −94.1% |
| CAGR | −50.8% | −43.8% | −71.5% |
| Annual volatility | +58.7% | +54.7% | +72.8% |
| Sharpe | -0.92 | -0.78 | -1.36 |
| Max drawdown | −100.0% | −99.6% | −95.1% |
| Trades / profit factor | 4131 / 0.83 | 2769 / 0.84 | 1362 / 0.80 |
| Avg net trade | -15.0 bp | -14.3 bp | -16.3 bp |
| Benchmark Sharpe (max DD) | 0.46 (−24.6%) | 0.39 (−24.6%) | 0.71 (−18.2%) |

The table is the kit summary of `results.json`. Sharpe figures in the prose are the stored values behind that rounding. It failed 6 of the 10 acceptance tests written before the first run (§8). Line 6 holds (1,362 out-of-sample trades; the floor is 100). The recorded status is Rejected because the performance lines failed. The universe is a static snapshot. The locked rule caps that universe at Rejected (Survivorship Contaminated) if every line passes.

**Why.**

1. **The out-of-sample loss is in the prices.** At zero cost the out-of-sample Sharpe is −0.835 and the return is −85.9%. At 10 bp per side the Sharpe is −1.360 and the profit factor is 0.796. Line 1 requires a Sharpe of at least 0.50 and a profit factor of at least 1.10. The average net trade is −16.3 bp out of sample and −15.0 bp on the full sample.
2. **The entry day is a down day for the names that were just restricted.** On the 4,131 entry name-days the mean open-to-close is −0.000176. On the other 499,568 name-days of those same names it is +0.000245. Prediction 1 is not consistent. The account path is the compounded equal-weight book. That path is the primary series.
3. **Both publishers lose.** Nasdaq-sourced episodes have an out-of-sample Sharpe of −0.782 on 1,144 trades. NYSE Group-sourced episodes have −1.027 on 218 trades. Line 5 fails.
4. **The in-sample grid is negative on every hold.** The primary in-sample Sharpe is −0.781. The share of in-sample grid cells with Sharpe above zero is 0. Line 3 fails. Hold 5, which is not the primary, has an out-of-sample Sharpe of +0.356 and an out-of-sample return of −9.0%. The verdict stays on hold 1.
5. **The drawdown line fails on a loss that is spread across the sample.** Full-sample max drawdown is −0.999763. The limit is −0.35. Line 7 fails. Minimum equity is 0.000237, so the ruin line passes. Dropping the best 1% of sessions (29 sessions) leaves a residual Sharpe of −1.942. The bootstrap 95% interval of the full-sample Sharpe is −1.474 to −0.405. Every draw is at or below zero.

**Recommendation.** Do not trade it. The hold grid, the publisher split, the delay, and the same-close upper bound stay in the report. The locked list stays closed: a grid cell, a single symbol, a health-check filter, an unflagged 9% decline, the trigger day's close as a fill, VIXY or UVXY, a different cost, and a different sample split.

## 2. The strategy

### Rules

```
On NYSE session D, a stored name starts an episode when D is short-sale restricted
and the previous NYSE session is not. Nasdaq wins if both publishers restrict D.
Entry session E is the next NYSE session.

If the book is flat and E has a daily bar:
    buy equal weight at E's open, weights summing to 1
    sell at E's close
Otherwise:
    skip E and do not defer it
```

- **Why the next open:** The Nasdaq file is sampled after the cash close. A fill on the trigger day's close would use a file that is not known at 16:00. The primary fill is the next session's open.
- **Why one session:** The question asked for the session after a restriction begins. Holds of 2, 3, and 5 sessions are the decay grid around that question.
- **Why 10 bp per side:** A round prior for a name on the day after a 10% decline. The store has no quote. The sweep is 0, 0.5, 1, 2, and 3 times that cost.
- **Why these names:** Every instrument with a daily bar, matched on the current symbol. `instrument_listing.opened_at` is when this store bound the ticker, so it is not a listing date and it is not a filter. VIXY and UVXY are out because an earlier study found missing reverse splits. Other names with 50% jumps stay in.

### How it trades

| | |
|---|---|
| Fill sessions | 1,633, from 2015-08-25 through 2026-09-29. `study.cohorts` is 1,633. `study.episodes` is 4,777. |
| Trades | 4,131 round trips. Each row in `trades.csv` is one name. |
| Time in market | Exposure 0.563 on the full book, 0.516 in sample, 0.760 out of sample. |
| Holding time | The primary sells at the entry session's close. All 4,131 rows have exit reason `hold`. |
| Long / short | 4,131 long. Profit factor 0.830 full sample, 0.796 out of sample. |
| Win rate | 0.462 full sample, 0.466 in sample, 0.454 out of sample. |

A new entry session is skipped while any share is still open. Hold 1 is flat overnight, so the next session is free. The episode count is larger than the trade count because some signals fall on a session the book skips or a session with no fill bar.

## 3. Hypothesis and predictions

SEC Rule 201 (17 CFR § 242.201) flags a covered security after a decline of 10% or more from the prior regular-hours close. For the rest of that day and the next day a national best bid is disseminated, a short sale generally cannot execute at or below the national best bid. Barardehi, Bird, Karolyi, and Ruchti (Office of Financial Research, 2023) estimate that crossing −10%, relative to stopping just short of it, raises the trigger day's return by about 35 bp, with no reversal after the restriction lifts. This study does not run that comparison. The store has the flag and the daily bars of this snapshot. It does not have the intraday path of names that fell 9% and were not flagged. The hypothesis is that the next session's open-to-close, in an equal-weight book of flagged names, is positive after 10 bp per side. A positive result would be a long after a flagged 10% decline. Switzer, Duangnate, and Torsu (2019) report that the same circuit breaker does not reliably stop the intraday decline.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Entry-day open-to-close of traded names exceeds their other days | Entry mean −0.000176 on 4,131 name-days. Other-day mean +0.000245 on 499,568 name-days. | Not consistent |
| A larger signal-day decline lines up with a larger net trade (quintile 1 above quintile 5) | Mean net on 4,116 trades with a prior bar: Q1 −0.002490, Q2 −0.003095, Q3 +0.000286, Q4 −0.000371, Q5 −0.001601. | Not consistent |
| Mean net trade falls strictly as the hold goes 1, 2, 3, 5 | Hold 1 −0.001496 (4,131), hold 2 −0.001548 (2,374), hold 3 −0.001425 (1,725), hold 5 −0.002558 (1,112). | Not consistent |
| Nasdaq and NYSE Group full-sample total returns have the same nonzero sign | Nasdaq −0.997643. NYSE Group −0.976807. | Consistent |

The acceptance table failed. Predictions 1, 2, and 3 failed with it. Prediction 4 holds because both publisher books lost money. The mechanism is not confirmed.

## 4. Method

- **Data.** Daily bars from `agent-data/mdq.py`, split-adjusted. The dividend table has 0 rows, so an ex-date inside a hold is a price gap. The book is NYSE sessions from 2015-03-25 through 2026-10-01 with a positive SPY open and close. `dropped_spy_sessions` is 1. Short-sale state is the Nasdaq file for that session, or an NYSE Group row with `trigger_session` <= session < `deactivate_session`. Nasdaq wins a tie. A missing file is left missing. The health pass on the 258 symbols did not pass: 89 unadjusted-jump rows on 34 symbols, and 32 zero-dividend flags on the yield ETFs in the snapshot. Those names stay in the book. VIXY and UVXY are the only exclusions, decided from the earlier void study.
- **Pre-registration.** `RULES.md` was locked at 2026-10-15T04:51:11+00:00, sha256 `33e478d8cfd481b9826afd9285f265ed54c5f82b945f76ff0754b5728f4c908c`. `results.json` carries that hash. Git HEAD was `1b4fd133391e71e42b84c4bdb713b595eef97f3b` and the tree was dirty. The lock was not committed before the run. Earlier studies used this snapshot and the window from 2024-07-01. SPY and QQQ rose over it, and it contains the April 2025 decline and rebound. No earlier study scored this SSR rule. The looks before the lock were counts and the health audit. They are listed in `RULES.md`.
- **Fills and costs.** The primary buys the next open and sells that session's close. Cost is 10 bp of dollars bought and 10 bp of dollars sold. The delay check enters one book session later. The upper bound buys the signal close and is labelled an illegal fill.
- **Returns.** Daily simple returns. Flat sessions are 0. Sharpe is the mean divided by the sample standard deviation, times √252, with a zero risk-free rate. CAGR uses a 252-session year. Profit factor and the average net trade use the trade rows. A trade's net is `weight * (exit / entry − 1)` minus the entry cost and the exit cost on the sale proceeds. For hold 1 that net is the day's account return.
- **Verification.** The self-test covers the hand cases in `RULES.md`, including two missing sessions in a row, and it passes with the store closed. `verify.py` rebuilt hold 1 on its own and matched all 4,131 trades and all 2,898 daily rows. `python -m research.kit guard` on that file printed `ok`.
- **Runs.** Two store runs, both logged. The first was the pre-registered run. The second repaired a missing-bar exit that, after a second session with no bar, left the book open and earned 0 on later sessions. Hold 1 exits on the entry session and was off that path. After the repair the primary Sharpe, return, and trade counts in `results.json` match the first run at every stored digit. Hold 3's out-of-sample Sharpe moved from −0.061672 to −0.062249, its out-of-sample trades from 510 to 512, and its out-of-sample return from −0.536679 to −0.537143. Holds 2 and 5 were unchanged. The upper bound's out-of-sample return moved from 0, with a null Sharpe, to −0.915701 and Sharpe −1.164. The report uses the second run.

## 5. Results

![Growth of $1](figures/equity.svg)

The compounded book falls across the full sample, including after the 2024-07-01 line, and the full-sample total return is −100.0%. SPY open-to-close rises on the same axes.

![Drawdown](figures/drawdown.svg)

The full-sample drawdown reaches −0.999763. The out-of-sample window, restarted at 1, reaches −0.950743.

| Strategy (10 bp/side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary, hold 1** | −0.999714 / −0.915 / −0.999763 | −0.781 | −0.940543 / −1.360 / −0.950743 |
| *SPY open-to-close* | +0.863976 / 0.464 / −0.245908 | 0.394 | +0.251411 / 0.709 / −0.182156 |

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | Benchmark |
|---|---:|---:|---:|---:|
| 2015 | −4.08% | −1.099 | −4.21% | −3.48% |
| 2016 | −12.83% | −0.058 | −44.26% | +14.52% |
| 2017 | −50.89% | −1.305 | −53.48% | +7.51% |
| 2018 | −58.82% | −0.967 | −62.24% | −16.77% |
| 2019 | −51.05% | −1.164 | −58.97% | +13.68% |
| 2020 | −68.34% | −1.558 | −72.49% | +2.59% |
| 2021 | +5.59% | +0.364 | −39.38% | +9.50% |
| 2022 | −58.64% | −0.923 | −74.04% | −5.58% |
| 2023 | −44.48% | −0.745 | −55.74% | +19.19% |
| 2024 | −80.04% | −2.682 | −85.30% | +0.36% |
| 2025 | −18.03% | +0.186 | −49.23% | +13.72% |
| 2026 | −72.48% | −2.425 | −79.64% | +14.91% |

2021 is the only calendar year with a positive strategy return. 2025 has a positive Sharpe and a negative return. The benchmark column is SPY open-to-close.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each cohort's dates and flips the book's zero-cost gross daily returns against SPY open-to-close. Two thousand draws, seed 20261014. The actual gross Sharpe is −0.432. The null mean is 0.113 and the null 95th percentile is 0.594. p = 0.975. The bar is 0.05. The gross book sits on the losing side of random assignment of the same spans.

The timing placebo is not an acceptance line. It masks SPY open-to-close to the sessions the book held. Five hundred draws, seed 20261015. Actual Sharpe 0.574, null mean 0.355, null 95th percentile 0.662, p = 0.136. `timing_error` is null. That Sharpe is SPY's, on the days the strategy was in the market.

### Bootstrap

Circular block bootstrap of the primary daily returns, 20-session blocks, 2,000 draws, seed 20261016. The 95% interval of the full-sample Sharpe is −1.474 to −0.405. The share of draws at or below zero is 1. The out-of-sample t-stat of the mean daily return is −2.039. Deflated Sharpe on 4 trials, the four holds, is 0.017. The deflated Sharpe is stored and is not an acceptance line.

### Parameter plateau

![Parameter grid](figures/grid.svg)

| Hold | IS Sharpe | OOS Sharpe | Full return | OOS return | OOS trades | Full PF |
|---|---:|---:|---:|---:|---:|---:|
| **1** | −0.781 | −1.360 | −0.999714 | −0.940543 | 1362 | 0.830 |
| 2 | −0.401 | −0.714 | −0.998208 | −0.827326 | 785 | 0.879 |
| 3 | −0.368 | −0.062 | −0.995770 | −0.537143 | 512 | 0.906 |
| 5 | −0.602 | +0.356 | −0.995072 | −0.089810 | 309 | 0.863 |

Zero of the four in-sample Sharpes are positive. The required share is 60%, which on four cells is three cells. The primary is the worst in-sample Sharpe in the grid. Hold 5 is the only positive out-of-sample Sharpe, and its out-of-sample return is −0.089810. Nothing was selected from the grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.432 | −0.674 | **−0.915** | −1.398 | −1.878 |
| OOS Sharpe | −0.835 | −1.098 | **−1.360** | −1.885 | −2.409 |
| Full-sample return | −0.992424 | −0.998526 | **−0.999714** | −0.999989 | −1.000000 |
| OOS return | −0.859013 | −0.908424 | **−0.940543** | −0.974970 | −0.989481 |

The zero-cost column is already a loss, in sample and out of sample. Line 4's stored full-sample return at 2× is −0.9999892610615512. The required value is a return above zero.

Entering one book session later, still at hold 1 and 10 bp, has full-sample Sharpe −1.145, out-of-sample Sharpe −0.723, full-sample return −0.999806, and out-of-sample return −0.754764.

The labelled upper bound buys the signal close and sells the planned exit close. It is not a legal fill. On the repaired run its full-sample Sharpe is −0.535, its out-of-sample Sharpe is −1.164, its full-sample return is −0.998476, and its out-of-sample return is −0.915701.

### Other markets (identical rules)

These rows are the same hold, cost, and sizing on one publisher's episodes. They are a source split of this snapshot.

| | IS Sharpe | OOS Sharpe | Full return | Full PF | OOS trades |
|---|---:|---:|---:|---:|---:|
| NASDAQ | −0.584 | −0.782 | −0.997643 | 0.864 | 1144 |
| NYSE_GROUP | −0.501 | −1.027 | −0.976807 | 0.817 | 218 |

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Mean strategy daily net by quintile of SPY's open-to-close. Quintile 1 is the lowest SPY day. This cut is a report. It is not a filter.

| Quintile | Sessions | Mean strategy net |
|---|---:|---:|
| 1 | 580 | −0.011697 |
| 2 | 580 | −0.003844 |
| 3 | 579 | −0.002392 |
| 4 | 580 | −0.000999 |
| 5 | 579 | +0.008291 |

The book loses money on the four lower SPY days and makes money on the highest. Every trade is long. Every exit reason in `trades.csv` is `hold`. The signal-day decline quintiles are in §3. They answer a different question, and quintile 1 there is the larger prior decline, which lost more than quintile 5.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. Out-of-sample edge | OOS Sharpe ≥ 0.50 and OOS profit factor ≥ 1.10 | Sharpe −1.360, profit factor 0.796 | ❌ |
| 2. Direction placebo | p ≤ 0.05 | 0.9750124937531235 | ❌ |
| 3. In-sample plateau | IS Sharpe > 0 and at least 60% of IS grid cells have Sharpe > 0 | IS Sharpe −0.781, grid 0.000 | ❌ |
| 4. Cost at 2× | Full-sample total return > 0 | −0.9999892610615512 | ❌ |
| 5. Cross-market | OOS Sharpe > 0 on at least one of NASDAQ and NYSE_GROUP | NASDAQ=−0.782, NYSE_GROUP=−1.027 | ❌ |
| 6. Sample size | At least 100 OOS trades | 1362 | ✅ |
| 7. Max drawdown | Full-sample max drawdown ≥ −0.35 | −0.9997629183027836 | ❌ |
| 8. Minimum equity | Mark-to-market equity > 0 | 0.00023708169721638412 | ✅ |
| 9. Intraday leverage | Max gross leverage ≤ 4 | 1.0010010010010013 | ✅ |
| 10. Overnight leverage | Max gross leverage ≤ 2 | 0.0 | ✅ |

Lines 1 through 5 and line 7 fail. Lines 6, 8, 9, and 10 pass. A passed sample-size line does not carry a failed performance line. The direction placebo is on the zero-cost gross book. The primary Sharpe is the costed book.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| The prices lose before costs | Zero-cost OOS Sharpe −0.835, OOS return −0.859013 | The rule stops here. A lower cost would still face that column. |
| The loss is wide | Residual Sharpe −1.942 after dropping 29 sessions. Worst jackknife session 2018-03-20. Influence 0.077. Concentration check passed: false. | Reported. It is not a filter. |
| Both publishers lose | §6 table. Full-sample returns −0.997643 and −0.976807. | The source split is the cross-market line this snapshot can support. |
| Delay and the illegal early fill | One session later: OOS Sharpe −0.723. Signal-close upper bound: OOS Sharpe −1.164. | Both are worse than a flat book. |
| The window was already read for the market | Studies named in `RULES.md` scored 2024-07-01 onward on SPY, QQQ, and this snapshot. This SSR rule had not been scored. | The split stays. |
| Static snapshot, jumps, and dividends | Universe type `static_snapshot`. 89 jump rows kept. Dividend rows in the store: 0. | Disclosed. Names were not dropped after the result. |

The trailing 252-session Sharpe is post hoc and is discussed in §11. It is not part of this table's verdict.

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A different hold, a short, a name filter, or a later sample is a new study with its own `RULES.md`. The hold sweep in §11 was read after this verdict. The strategy was not ported into the terminal.

## 11. Post hoc (not part of the verdict)

`posthoc.py` reads `daily.csv` and writes a trailing 252-session Sharpe of `strategy_net`. The series is labelled **(post hoc)**. Of 2,647 defined windows, 337 are positive. The minimum is −3.364 and the maximum is +1.163. The figure in §9 draws that series. It does not change §8.

### Hold from 1 to 31 sessions (post hoc)

`hold_sweep.py` reruns the locked book at every hold from 1 through 31 NYSE sessions. The fill, the 10 bp per side, and the one-cohort rule stay as locked. The cost is charged on the entry and on the exit, once each, at every hold. A new cohort waits until the book is flat, so the account takes fewer trades as the hold lengthens. Holds 1, 2, 3, and 5 match `results.json`. The sweep does not change §8.

The second set of columns is every out-of-sample episode held on its own, weight 1. That count stays 1,362 at every horizon. The median is the middle name. The mean is the arithmetic average of the same names.

![Hold sweep](figures/hold_sweep.svg)

The figure shows the out-of-sample account Sharpe and the out-of-sample median name. Holds 12, 25, and 28 are large on the account-return scale because their largest out-of-sample day is 2026-06-18. Those returns are in the table.

| Hold | OOS account | OOS Sharpe | OOS trades | Full account | Full Sharpe | OOS median name | OOS mean name |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | -94.1% | -1.360 | 1362 | -100.0% | -0.915 | -56.1 bp | -30.7 bp |
| 2 | -82.7% | -0.714 | 785 | -99.8% | -0.465 | -87.0 bp | -34.2 bp |
| 3 | -53.7% | -0.062 | 512 | -99.6% | -0.304 | -108.0 bp | -25.7 bp |
| 4 | +26.3% | +0.494 | 370 | -53.3% | +0.265 | -43.1 bp | +57.8 bp |
| 5 | -9.0% | +0.356 | 309 | -99.5% | -0.358 | -28.5 bp | +82.0 bp |
| 6 | -24.6% | +0.216 | 295 | -97.3% | -0.060 | -70.2 bp | +83.7 bp |
| 7 | -35.3% | +0.110 | 251 | -97.7% | -0.127 | -56.4 bp | +79.5 bp |
| 8 | -18.2% | +0.199 | 218 | -98.0% | -0.130 | -42.0 bp | +118.3 bp |
| 9 | -57.6% | -0.223 | 161 | -98.0% | -0.202 | -25.9 bp | +772.1 bp |
| 10 | +181.9% | +0.981 | 158 | -77.9% | +0.156 | +40.7 bp | +804.2 bp |
| 11 | +17.9% | +0.420 | 157 | +13.5% | +0.342 | +30.3 bp | +804.1 bp |
| 12 | +7,661% | +0.714 | 74 | +15,559% | +0.360 | +67.0 bp | +818.1 bp |
| 13 | -79.4% | -0.652 | 127 | -97.5% | -0.103 | +101.4 bp | +865.5 bp |
| 14 | +67.1% | +0.664 | 106 | -92.6% | +0.062 | +108.7 bp | +923.0 bp |
| 15 | -16.9% | +0.231 | 91 | -84.8% | +0.098 | +130.1 bp | +1177.8 bp |
| 16 | -23.2% | +0.099 | 70 | -99.2% | -0.184 | +110.6 bp | +1190.5 bp |
| 17 | +98.1% | +0.765 | 176 | -92.2% | +0.066 | +90.1 bp | +1195.2 bp |
| 18 | +464.2% | +1.457 | 98 | +2,694% | +0.758 | +86.1 bp | +1198.2 bp |
| 19 | +60.2% | +0.641 | 58 | -0.9% | +0.365 | +59.3 bp | +1233.5 bp |
| 20 | -50.7% | +0.051 | 64 | -97.1% | -0.090 | +72.6 bp | +1260.6 bp |
| 21 | +4.7% | +0.328 | 96 | -83.7% | +0.040 | +90.0 bp | +1292.6 bp |
| 22 | +337.5% | +1.239 | 78 | -91.6% | +0.061 | +97.4 bp | +1310.8 bp |
| 23 | +1.8% | +0.356 | 74 | -92.6% | +0.093 | +89.5 bp | +1321.1 bp |
| 24 | +8.2% | +0.448 | 66 | -81.3% | +0.242 | +63.4 bp | +2416.6 bp |
| 25 | +15,844% | +0.688 | 62 | +19,310% | +0.316 | +54.0 bp | +2421.5 bp |
| 26 | +400.9% | +1.477 | 60 | -18.2% | +0.313 | +107.0 bp | +2514.6 bp |
| 27 | +6.0% | +0.673 | 43 | -87.7% | +0.167 | +143.6 bp | +2520.1 bp |
| 28 | +1,136% | +0.661 | 35 | +944.3% | +0.314 | +144.3 bp | +2552.0 bp |
| 29 | +405.8% | +1.262 | 52 | +2,896% | +0.780 | +147.8 bp | +2592.1 bp |
| 30 | +18.3% | +0.461 | 87 | -26.7% | +0.293 | +150.2 bp | +2627.6 bp |
| 31 | +18.6% | +0.446 | 44 | -94.8% | +0.052 | +167.9 bp | +2665.8 bp |

At one session the out-of-sample account loses 94.1% and the middle name loses 56.1 bp. The median stays negative through 9 sessions. From 10 sessions through 31 it is positive out of sample, +40.7 bp at 10 and +167.9 bp at 31. The full-sample median is −34.3 bp at 1 session, −13.9 bp at 10, and −6.4 bp at 21. It is +23.2 bp at 31.

The out-of-sample mean turns up at 4 sessions and is +2,665.8 bp at 31. From 9 sessions on, the largest out-of-sample name is OPI. That round trip's net is +84.004 at 9 sessions and +146.876 at 24. With OPI removed, the 31-session out-of-sample mean is still +1,588.6 bp. The health audit records OPI on 2026-06-18 as a move from 0.27 to 30.36, ratio 112.988, with no split in `corporate_action`. Holds 12, 25, and 28 take their largest out-of-sample day on that session. Their account returns are +7,661%, +15,844%, and +1,136%.

The out-of-sample account Sharpe is positive at 26 holds. The full-sample account return is positive at 6 holds: 11, 12, 18, 25, 28, and 29. The highest out-of-sample Sharpe in the table is hold 26, +1.477, on an out-of-sample return of +400.9% and a full-sample return of −18.2%. The sweep was computed after the lock. Hold 1 stays the primary.

### Ideas for a new study

The Office of Financial Research comparison, names that crossed −10% against names that stopped just short, needs the unflagged path. This store does not have it. That test would need its own rules and data this study did not use. A longer hold chosen from the table above was fit on this sample, including the out-of-sample window.

## 12. Reproduce

From the repo root:

```bash
python research/ssr-next-day-long/research/backtest.py
```

```bash
python research/ssr-next-day-long/research/verify.py
```

```bash
python research/ssr-next-day-long/research/posthoc.py
```

```bash
python research/ssr-next-day-long/research/charts.py
```

```bash
python research/ssr-next-day-long/research/hold_sweep.py
```

`backtest.py` runs the self-test, then the locked store run. It writes `results.json`, `daily.csv`, `trades.csv`, the placebo `.npy` files, and appends `RUNLOG.md`. A further run needs a permitted reason in that log. The logged repair run finished in about 8 seconds on this machine, including the self-test. `verify.py` rebuilds hold 1 and exits 1 on a mismatch. `posthoc.py` writes `posthoc.json`. `charts.py` writes the SVG figures under `report/figures/`. `hold_sweep.py` writes `hold_sweep.json`, `hold_sweep.csv`, and `report/figures/hold_sweep.svg`. It is post hoc. It checks that holds 1, 2, 3, and 5 still match `results.json`, and it appends a run-log row. It leaves `results.json` as it is. Seeds are 20261014, 20261015, and 20261016. The same store and the same code produce the same numbers.

Checklist for this study: the rules hash matches the lock and `results.json`; `kit_schema` is 1; `guard` printed `ok`; both run-log rows have a permitted reason; the self-test passed and `verify.py` matched every trade; the status is the acceptance table's Rejected; the rolling Sharpe is labelled post hoc; prior exposure and the coverage gaps are in §4; the reproduce commands are above. `python -m research.kit audit` reports this study clean. The same audit reports 1 error on the pre-existing orphan `equity-basket-trend`, which is outside this study. Nothing was written to `data/`, and `ingest` was not run.

### References

- 17 CFR § 242.201. Alternative uptick rule (short sale price test restriction).
- Barardehi, Bird, Karolyi, and Ruchti (2023). The effect of the short-sale circuit breaker on prices and trading. Office of Financial Research working paper, 11 October 2023.
- Switzer, Duangnate, and Torsu (2019). The circuit breaker and the intraday decline. Journal of International Financial Management & Accounting.
