# OBV divergence: QQQ equity holdings

| | |
|---|---|
| Date | 2026-10-03 |
| Status | **Rejected.** Failed 5 of 6 pre-registered tests. |
| Instruments | 101 end-of-sample QQQ equity holdings, equal absolute weight, long and short, open-to-open |
| Data | 2022-10-03 → 2026-10-02, read via `agent-data/mdq.py`. No forward-fill. Dividends are not adjusted. |
| Rules | [`research/qqq-holdings-obv-divergence/research/RULES.md`](../research/RULES.md), locked 2026-10-03T15:31:05+00:00, sha256 `9bd635f6d06f` |
| Code | [`research/qqq-holdings-obv-divergence/research/`](../research/) · 1 store run (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Granville on-balance-volume divergences on the current 101 QQQ equity holdings lost money after 5 bp per side. Out of sample, 2024-07-01 through 2026-10-02, the book returned −11.3% (Sharpe −0.22, profit factor 0.96, 520 trades). The full sample returned −36.4% (Sharpe −0.66, 914 trades). At zero cost the full-sample return was still −27.5%.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2022-10-03 → 2026-10-02 | 2022-10-03 → 2024-06-28 | 2024-07-01 → 2026-10-02 |
| Sessions | 1004 | 437 | 567 |
| Total return | −36.4% | −28.3% | −11.3% |
| CAGR | −10.7% | −17.4% | −5.2% |
| Annual volatility | +15.5% | +12.9% | +17.2% |
| Sharpe | -0.66 | -1.42 | -0.22 |
| Max drawdown | −40.9% | −31.6% | −22.2% |
| Trades / profit factor | 914 / 0.85 | 394 / 0.70 | 520 / 0.96 |
| Avg net trade | -4.4 bp | -8.5 bp | -1.3 bp |
| Benchmark Sharpe (max DD) | 1.29 (−24.2%) | 1.79 (−11.4%) | 0.97 (−24.2%) |

It failed 5 of the 6 acceptance tests written before the first run (§8). The minimum-sample line passed, with 520 out-of-sample trades.

**Why.**

1. The gross book is negative. At a cost multiple of 0 the full-sample return is −27.5% and the full-sample Sharpe is −0.44. Out-of-sample Sharpe at zero cost is −0.04.
2. The short side's gross profit factor is 0.568. The long side's is 1.379. The locked rule keeps both. Prediction 1 is not consistent.
3. Failure exits are 517 and time exits are 392. The recorded failure share is 0.5688. Prediction 2 is not consistent.
4. The same rule on QQQ alone and on SPY alone has out-of-sample Sharpe −0.070 and −0.375.
5. The direction-placebo p-value on the full-sample gross Sharpe is 0.858. The required ceiling is 0.05.
6. Full-sample total return at 2× cost is −44.2%. The test requires a positive return.

**Recommendation.** Do not trade it. The long side, any grid cell, the one-bar delay, the same-bar close bound, and a swing-gap cutoff stay diagnostics. The rules do not let one of them replace the primary.

## 2. The strategy

### Rules

```
OBV starts at 0 on the first stored bar.
Up close adds that bar's split-adjusted volume. Down close subtracts it. Equal close leaves OBV unchanged.

W = 5. A swing low at t-W is confirmed at t only when that close is strictly below
the W closes on each side. A swing high is the mirror. It is not known before t.

Bullish at t when the newest confirmed swing low and the nearest prior confirmed
swing low are 10 to 60 sessions apart, the newer close is below the older close,
and the newer OBV is above the older OBV. Bearish is the mirror on swing highs.
If the nearest prior swing is outside 10 to 60, do not walk further back.
If both sides confirm on the same session, take neither.
If the name is in a position or a fill is pending, ignore the signal.

Enter at the next printed open. Exit at the next open, checked at the close:
  (1) failure: a long whose close is below the newer swing low, or a short whose
      close is above the newer swing high;
  (2) time: the open 20 evaluated sessions after the entry session.
Failure wins if both fill the same open. A missing bar contributes 0 and delays the fill.
```

- **Why OBV.** Joseph E. Granville, *Granville's New Key to Stock Market Profits* (Prentice-Hall, 1963), adds volume on an up close and subtracts it on a down close. The claim is that volume turns before price, so the traders still paying the new price extreme are the ones who do not watch the cumulative line.
- **Why W = 5, a 10-to-60 session gap, and a 20-session hold.** Round choices fixed before any return on these names. They were not estimated on this sample.
- **Why 5 bp.** The protocol's 1 bp is for QQQ and SPY. These are single stocks. The QQQ and SPY books in the cross-market check use 1 bp.
- **Why equal absolute weight.** The book is the set of names the previous close said to hold. Absolute weights sum to 1 when any name is held. It is not dollar-neutral and it does not use published index weights.

### How it trades

| | |
|---|---|
| Sessions with an entry | 522 distinct entry sessions (`study.entry_sessions`) |
| Trades | 914. By entry year **(post hoc)**: 2022: 55, 2023: 224, 2024: 236, 2025: 237, 2026: 162 |
| Time in market | Exposure 0.999 full, 0.998 in sample, 1.0 out of sample |
| Holding time | Median 13 evaluated sessions, mean 12.18. Entry session counts inside that span |
| Long / short | 429 / 485 trades. Gross profit factor 1.379 / 0.568. Net profit factor 1.313 / 0.537 |
| Win rate | 34.9% full sample. Average winning net 0.007256. Average losing net −0.004569 |
| Names | 100 of the 101 names appear in `trades.csv` |

The year counts are entry sessions. 2024 contains both in-sample and out-of-sample entries. In-sample trades are 394 and out-of-sample trades are 520, split by the entry date, not by the calendar-year table. Net and gross on a trade are that name's contribution to the account, not the stock's own percent move. Five trades end as `sample_end` and are not in the failure-share denominator.

## 3. Hypothesis and predictions

The mechanism is Granville's: a price swing that OBV does not confirm is where price-only traders are still paying an extreme after cumulative volume has turned. This study takes both sides of that disagreement on the current QQQ holdings and holds until the swing fails or 20 sessions pass. The swing confirmation is not Granville's. It waits W closes so the pivot does not use future bars.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Both long and short gross profit factor > 1 | Long 1.379 (429 trades). Short 0.568 (485 trades) | Not consistent |
| Failure exits are a minority of failure and time exits | 517 failure, 392 time, share 0.5688. Five `sample_end` exits are excluded | Not consistent |
| Above-median absolute swing-price gap has the higher mean gross | Median gap 2.9367. Above −0.000366 (457). Below −0.000233 (457) | Not consistent |

All three scores are the values in `results.json` under `study.predictions`. The P&L did not pass, and the mechanism predictions did not pass either. The gap is not a filter.

## 4. Method

- **Data.** Split-adjusted daily bars from `agent-data/mdq.py`, 2021-10-04 through 2026-10-02 for warm-up and 2022-10-03 through 2026-10-02 for the book (1,004 sessions). Dividends are not adjusted. A missing bar is not a price and is not forward-filled. Later listings and holes are the ones locked in `RULES.md`: CEG, WBD, GEHC, ARM, ALAB, NBIS, SNDK, CRWV, FER, and HONA start inside the window; ALNY is missing 2023-09-13; FER is missing 162 sessions from 2023-08-02 through 2024-05-02; SPCX is missing 104 sessions. 2025-01-09 is not a session. QQQ and SPY have no bar on 2026-10-02, so the benchmark that session is 0. The open-to-open step across a hole uses the weight on the name's previous printed bar. No name was dropped after the run.
- **Universe.** The 101 equity holdings of Invesco QQQ, CUSIP 46090E103, business date 2026-10-02. Both GOOGL and GOOG are included. Membership is end-of-sample, not a historical constituent tape. Index membership is look-ahead. Published end-of-sample weights are not used.
- **Pre-registration.** `RULES.md` was locked at 2026-10-03T15:31:05+00:00, sha256 `9bd635f6d06f8675d672cefed21fc31cb6da17c287d35f37eb17b7ba3aac0e3f`, before the store run. `results.json` carries that hash. The file was not committed. Prior exposure, as locked: there is no earlier OBV study. Constituent OBV paths had not been studied. Mean-reversion studies on other universes are not this rule. The out-of-sample window is not unseen at the index level. House reports already describe 2024-07-01 through 2026-10-01 for several ETFs, and the intraday QQQ reports describe QQQ through 2026-09-25, including a buy-and-hold path of about +55% with a Sharpe near 1.
- **Fills and costs.** Primary fill is the next printed open after the confirmation close. Cost is 0.0005 times the sum of absolute target-weight changes. That charges entries, exits, and resizes when the number of held names changes. It does not charge price drift. Cash earns 0. There is no extra borrow fee. `sample_end` charges no exit.
- **Returns.** Compounded daily simple returns. A session with no position, and a name with no bar, contributes 0. Sharpe is the mean divided by the sample standard deviation, times √252, with a risk-free rate of 0. A trade belongs to the sample of its entry session. Daily Sharpe uses the session date.
- **Verification.** `backtest.py` runs a store-free self-test before it opens the store: a bullish time exit, a bearish case, both sides on one day, a swing that is not confirmed until the later bars exist, a signal ignored while a position is open, a missing bar, and a failure exit. `verify.py` does not import `backtest.py`. It matched all 914 trades on side, entry time, entry price, exit time, and exit price. `python -m research.kit guard research/qqq-holdings-obv-divergence/research/verify.py` printed `ok`.
- **Runs.** `RUNLOG.md` has one entry, 2026-10-03T15:47:11+00:00, reason `initial pre-registered run`. Git HEAD `738d3e7ec4340e05458317114dfe247d55a1ef16`, dirty. An earlier process in this session computed the primary path and then raised in the timing placebo, before `results.json` or `RUNLOG.md` existed. The repair changed the sampler so a later random entry does not block an earlier entry that does not overlap it. That is the locked non-overlap rule. The primary signal, fills, and costs were not changed after that crash, and there is no saved headline from it. The study was not committed. `research/README.md` was not updated: the request for this study forbade that edit. That checklist item is open.

## 5. Results

![Growth of $1](figures/equity.svg)

The compounded book falls from the first year of the sample through the end. The in-sample / out-of-sample boundary is marked. Uncosted QQQ, on the same axes, rises over the full window.

![Drawdown](figures/drawdown.svg)

| Strategy (5 bp per side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | −36.4% / −0.66 / −40.9% | −1.42 | −11.3% / −0.22 / −22.2% |
| *QQQ close-to-close, uncosted* | +178.2% / 1.29 / −24.2% | 1.79 | +55.2% / 0.97 / −24.2% |

Out-of-sample t-stat of the mean daily net return is −0.335. Full-sample t-stat is −1.309.

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | Benchmark |
|---|---:|---:|---:|---:|
| 2022 | +1.2% | 0.32 | −9.9% | −0.4% |
| 2023 | −15.7% | −1.56 | −18.7% | +53.8% |
| 2024 | −14.2% | −1.33 | −20.7% | +24.8% |
| 2025 | −10.8% | −0.50 | −18.5% | +20.2% |
| 2026 | −2.6% | −0.12 | −17.0% | +21.1% |

2022 starts on 2022-10-03 and 2026 ends on 2026-10-02. These are calendar slices of the daily path. They are not a reason to drop a year.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trade's dates and flips the sign of that trade's gross pieces. Two thousand draws, seed 20261003. Actual gross Sharpe is −0.445. The null mean is 0.015 and the null 95th percentile is 0.736. p = 0.858. A negative result this far from the right tail does not clear p ≤ 0.05. The placebo does not say the loss was a cost artifact: the gross path is the thing being flipped.

The timing placebo is not an acceptance line. Five hundred draws place 914 non-overlapping random entries, sides even, failure level equal to the prior close, then the same failure and time exits. Its null mean gross Sharpe is −0.269, its null 95th percentile is 0.481, and p against the primary gross Sharpe is 0.637.

### Bootstrap

Circular 20-session blocks of the net daily path, 2,000 draws, seed 20261003. The 95% interval of full-sample Sharpe is −1.519 to 0.216. The share of draws at or below 0 is 0.9235. Out-of-sample t-stat is −0.335.

### Parameter plateau

![Parameter grid](figures/grid.svg)

Nine cells, W in {3, 5, 8} and hold in {10, 20, 40}. Nothing was selected. The acceptance line records in-sample Sharpe −1.421 and a grid share of 0.000. **(Post hoc)** the share of in-sample Sharpes above 0 is 0. The primary, W = 5 and hold = 20, ranks 5th of 9 on in-sample Sharpe, with rank 1 the highest. Spearman correlation of the nine in-sample Sharpes with the nine out-of-sample Sharpes is 0.20.

The highest in-sample cell is W = 8, hold = 10, in-sample Sharpe −1.054, out-of-sample Sharpe −0.354, full-sample return −48.5%. Two cells have a positive out-of-sample Sharpe and a negative in-sample Sharpe: W = 3, hold = 20 (out-of-sample 0.099) and W = 8, hold = 20 (out-of-sample 0.120). They were not eligible to replace the primary.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.44 | −0.55 | **−0.66** | −0.87 | −1.08 |
| OOS Sharpe | −0.04 | −0.13 | **−0.22** | −0.41 | −0.60 |
| Full-sample return | −27.5% | −32.1% | **−36.4%** | −44.2% | −51.0% |

There is no cost multiple in the sweep with a positive full-sample return, so there is no break-even inside the sweep. The zero-cost row is already a loss.

Filling at the second printed open, with the same exits measured from that later entry, has full-sample Sharpe −0.36, full-sample return −25.1%, out-of-sample Sharpe 0.08, and out-of-sample return −0.3%. The out-of-sample Sharpe is positive and the out-of-sample return is not. This row is not an acceptance line and it is not a new rule.

The same-bar close path is an upper bound only. It is not tradable. Its full-sample Sharpe is −0.61, full-sample return −36.4%, out-of-sample Sharpe −0.40, and out-of-sample return −18.4%.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|
| QQQ | 0.05 | −0.07 | −0.6% / 1.00 |
| SPY | −1.08 | −0.38 | −12.0% / 0.20 |

Position size is +1, −1, or 0. Cost is 1 bp per side. Acceptance line 5 asked for an out-of-sample Sharpe above 0 on at least one of them. Both are below 0.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

| Quintile of the QQQ close-to-close move | Sessions | Mean strategy net |
|---|---:|---:|
| 1 | 201 | −0.000356 |
| 2 | 201 | −0.000234 |
| 3 | 201 | −0.001143 |
| 4 | 201 | −0.000420 |
| 5 | 200 | 0.000144 |

Quintile 1 is the lowest QQQ move. Quintile 5 is the only bucket with a positive mean. These cuts were not used to filter the rule.

By side, the long book has a gross profit factor above 1 and the short book does not (§3). By exit, 517 trades leave on the failure rule, 392 on the time rule, and 5 at `sample_end`. None of these counts was used to drop a side or an exit.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| Out-of-sample edge | OOS Sharpe ≥ 0.50 and OOS profit factor ≥ 1.10 | Sharpe −0.223, profit factor 0.957 | ❌ |
| Direction placebo | p ≤ 0.05 | 0.858 | ❌ |
| In-sample plateau | IS Sharpe > 0 and at least 60% of IS grid cells have Sharpe > 0 | IS Sharpe −1.421, grid 0.000 | ❌ |
| Cost | Full-sample total return > 0 at 2× cost | −0.442 | ❌ |
| Cross-market | OOS Sharpe > 0 on at least one of QQQ and SPY | QQQ = −0.070, SPY = −0.375 | ❌ |
| Sample size | At least 100 OOS trades | 520 | ✅ |

The result strings are the `actual` fields in `results.json`. A passed sample-size line does not carry the other five. The direction placebo is on gross pieces. Costs are not what put the gross Sharpe on the wrong side of zero.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

The rolling figure is a trailing 126-session Sharpe of the primary net path. It is post hoc and it is not in the verdict. **(Post hoc)** 879 windows are defined. The lowest is −3.895 on 2024-07-23. The highest is 2.131 on 2025-03-13. The share above 0 is 0.290.

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | In-sample Sharpe −1.42 and out-of-sample Sharpe −0.22. Both are below 0. The zero-cost full-sample Sharpe is −0.44 | No deployment in §10. A later window would be a new study |
| Concentration in a few days | Bootstrap 95% interval of full-sample Sharpe is −1.519 to 0.216, and 0.9235 of draws are at or below 0. Quintile means are negative in four of five buckets | The interval is reported. It was not used to drop days |
| Generalization | QQQ and SPY out-of-sample Sharpes are −0.070 and −0.375. The universe is end-of-sample membership, which is look-ahead toward names that were still in QQQ on 2026-10-02 | The cross-market books use the same rule. Membership was not rewritten after the loss |
| Execution | Zero cost still loses. One extra bar of delay leaves the full sample at −25.1%. The same-bar close bound is also negative out of sample. The store has no quotes; 5 bp was set before the run | The cost sweep stays at the locked rate. The delay and the bound are not tradable substitutes |
| Short sample / regime coverage | 1,004 sessions, and the out-of-sample window was already known to be a rising tape for QQQ. 2022 inside this file is a partial year and is the only calendar slice with a positive strategy return | Both sides stayed in the primary. The rising tape was disclosed before the lock and was not removed |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A different rule, a long-only book, a grid neighbour, or a later sample is a new study with its own `RULES.md`. This one is not ported into the terminal.

## 11. Post hoc (not part of the verdict)

`posthoc.json` is labelled post hoc. None of it changes §8.

- Trailing 126-session Sharpe: minimum −3.895 on 2024-07-23, maximum 2.131 on 2025-03-13, and 0.290 of the 879 windows are above 0.
- Entry-year trade counts from `trades.csv`: 2022: 55, 2023: 224, 2024: 236, 2025: 237, 2026: 162.
- Grid description computed from the saved nine cells: in-sample positive share 0, primary in-sample rank 5 of 9, Spearman of in-sample versus out-of-sample Sharpe 0.20. The in-sample winner and the two positive out-of-sample cells are listed in §6.

### Ideas for a new study

A long-only version is the breakdown this sample produced, not a rule that was locked. Testing it needs a new `RULES.md` and a sample this study has not used. A point-in-time constituent tape would remove the look-ahead membership this file accepted on purpose. Neither change is applied here.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-holdings-obv-divergence/research/backtest.py
```

```bash
python research/qqq-holdings-obv-divergence/research/verify.py
```

```bash
python research/qqq-holdings-obv-divergence/research/posthoc.py
```

```bash
python research/qqq-holdings-obv-divergence/research/charts.py
```

`backtest.py` checks the lock, runs the store-free self-test, then one store run. It writes `results.json`, `daily.csv`, `trades.csv`, `placebo_direction.npy`, `placebo_timing.npy`, and appends `RUNLOG.md`. Seeds for the direction placebo, the timing placebo, and the bootstrap are 20261003. `verify.py` replays every primary trade. `posthoc.py` reads the saved files and writes `posthoc.json`. `charts.py` calls `render_standard` and writes the SVGs under `report/figures/`. The same store and the same code return the same numbers. `python -m research.kit summary research/qqq-holdings-obv-divergence/research` prints the table in §1.

`research/README.md` does not list this study. That file was left unchanged because the request forbade editing it.

### References

- Granville, J. E. (1963). *Granville's New Key to Stock Market Profits*. Prentice-Hall.
