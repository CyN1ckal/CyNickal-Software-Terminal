# QQQ holdings relative strength: 101 constituents

| | |
|---|---|
| Date | 2026-10-03 |
| Status | **Paper-trading candidate.** Passed all 6 pre-registered tests. The ticker list is end-of-sample membership, so this file is not a paper book (§10). |
| Instruments | 101 Invesco QQQ equity holdings (CUSIP 46090E103, business date 2026-10-02), long the top quintile, equal weight, rebalanced every 21 sessions at the next open. QQQ is the formation benchmark and the uncosted close-to-close benchmark. |
| Data | Daily split-adjusted bars 2021-10-04 → 2026-10-02, evaluated 2022-10-03 → 2026-10-02, read via `agent-data/mdq.py`. Dividends are not adjusted. Missing bars stay missing. |
| Rules | [`research/qqq-holdings-relative-strength/research/RULES.md`](../research/RULES.md), locked 2026-10-03 15:26 UTC, sha256 `40e29bd16bef` |
| Code | [`research/qqq-holdings-relative-strength/research/`](../research/) · 2 store executions, 1 logged run (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The published 6–1 month rule (Jegadeesh and Titman 1993), long only, ranks these stocks on the 126-session return ending 21 sessions ago minus QQQ's return over that same window, and holds the top quintile for 21 sessions. Out of sample, 2024-07-01 through 2026-10-02, the book returned **+265.7%** after 5 bp per unit of absolute weight change (Sharpe **1.52**, profit factor **2.41**, 520 trades, max drawdown −35.5%). In sample it returned +97.7% (Sharpe 1.75). Over the full evaluation it returned +622.9% (Sharpe 1.53, max drawdown −35.5%). Uncosted QQQ close-to-close returned +55.2% out of sample (Sharpe 0.97) and +171.9% over the full sample (Sharpe 1.27).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2022-10-03 → 2026-10-02 | 2022-10-03 → 2024-06-28 | 2024-07-01 → 2026-10-02 |
| Sessions | 1004 | 437 | 567 |
| Total return | +622.9% | +97.7% | +265.7% |
| CAGR | +64.3% | +48.1% | +77.9% |
| Annual volatility | +37.1% | +24.1% | +44.6% |
| Sharpe | 1.53 | 1.75 | 1.52 |
| Max drawdown | −35.5% | −12.1% | −35.5% |
| Trades / profit factor | 900 / 2.45 | 380 / 2.53 | 520 / 2.41 |
| Avg net trade | +25.0 bp | +20.1 bp | +28.6 bp |
| Benchmark Sharpe (max DD) | 1.27 (−24.2%) | 1.73 (−11.4%) | 0.97 (−24.2%) |

It passed all 6 of the acceptance tests written before the first run (§8).

**Why.** The lines passed. The same outputs limit what the pass means.

1. **The rank does not beat a random quintile of the same eligible names.** The timing placebo p-value is 0.417. Actual gross Sharpe is 1.536. The null mean is 1.500 and its 95th percentile is 1.789 (500 draws, seed 20261003). The direction placebo p-value is 0.0010 and clears line 2. That null is a sign flip of a long-only book while QQQ rose +171.9% in the full sample and +55.2% out of sample.
2. **The laggards rose too.** Across 47 rebalances the mean forward open-to-next-rebalance return is +4.59% in the held top quintile and +2.61% in the bottom quintile. Prediction 1 is consistent. Both means are positive. The bottom quintile stays unheld.
3. **The book was invested through QQQ's drawdown, at about twice QQQ's volatility.** Out of sample, mean `held` is 1.0 on 489 sessions where compounded QQQ equity was below its running peak and 1.0 on the other 78. Same-session correlation of the strategy with QQQ is 0.026. Out-of-sample annual volatility is +44.6% against QQQ's +22.9%.
4. **The published cell is the best in sample and the worst out of sample.** In-sample Sharpe of L126-S21 is 1.753, rank 1 of 6. Its out-of-sample Sharpe, 1.516, is the lowest of the six. Spearman correlation of the six in-sample Sharpes with the six out-of-sample Sharpes is 0.086. L252-S0 has the highest out-of-sample Sharpe, 1.809. Nothing was selected from the grid.
5. **The universe and the window were known in ways a 2022 signal was not.** Membership is the 2026-10-02 constituent file, used on every signal. Index membership is look-ahead. Bars omit dividends, so price momentum understates total-return momentum. The out-of-sample window is not unseen for the index. HALF1 and HALF2 are one alphabetical split of this same list, and HALF2's full-sample return is +1270.8% against HALF1's +212.8%.
6. **Ten sessions carry a large share of the compound path (post hoc).** Setting the 10 best days to zero leaves a full-sample return of +197.8% and a Sharpe of 0.98. The best stored day is 2026-07-31 at +16.8%. This block is not an acceptance input.

**Recommendation.** The status stays paper-trading candidate because every locked line passed. Do not paper-trade this ticker list. The tested names were the end-of-sample constituents. Do not replace the primary with a grid cell, the skip=0 formation, the bottom-quintile short, one half, or the random-name book.

## 2. The strategy

### Rules

```
At signal close t, every 21 evaluated sessions:
    stock  = close[t-21] / close[t-21-126] - 1     # that name's own bars
    qqq    = QQQ close on those same two dates
    excess = stock - qqq
    eligible if both endpoints and the signal session have a bar
    if either QQQ endpoint is missing: nobody is eligible
Rank eligible names by excess descending.
Ties break by symbol ascending. The alphabet takes the higher rank.
N = eligible count
if N < 5: hold nothing
else: k = max(1, N // 5); hold the first k names at weight 1/k
Signal at the close. Fill at the next session's open.
Hold until the next rebalance open. No mid-month entry.
A missing bar while held contributes 0 and keeps the slot.
```

- **Why 126, 21, and a quintile:** Jegadeesh and Titman (1993) rank on an intermediate past return and skip the most recent month because that month reverses. Those published defaults are used unchanged. They were not estimated on this store.
- **Why long only:** the locked book buys the top quintile. The bottom quintile is scored in prediction 1 and is not held.
- **Why excess over QQQ:** the names are the holdings of the index being ranked against. Published weights are not used.
- **Why 5 bp:** these are stocks. The cost is `0.0005 * sum(|Δ weight|)` on the session where weights change. It was fixed before the run.

### How it trades

| | |
|---|---|
| Sessions with a trade | 48 entry sessions out of 1,004 evaluated sessions. 48 signals, 48 fills. First signal 2022-10-03, first fill 2022-10-04, last signal 2026-09-10, last fill 2026-09-11. The first fill opened 18 lots. |
| Trades per year | 226 |
| Time in market | Mean gross weight 0.998 over the full sample, 0.995 in sample, 1.0 out of sample |
| Holding time | Median 21 sessions, mean 20.9 sessions |
| Long / short | 900 long, profit factor 2.45. 0 short. |
| Win rate | 62.2%. Average winner +68.0 bp, average loser −45.7 bp |

Each name-interval that can print is its own trade, including a name that is closed and reopened at the same open. The 520 out-of-sample trades are lots whose entry session is on or after 2024-07-01. The 20 `sample_end` lots are the last cohort.

## 3. Hypothesis and predictions

Jegadeesh and Titman (1993) find that buying intermediate-horizon winners and selling losers earns a return their tests do not attribute to systematic risk, and that the most recent month reverses. This study trades only the long side, inside one index, against that index's own past return. The ranking uses price closes. Dividends are not in the bars.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Held top quintile has a higher average forward open-to-next-rebalance return than the bottom quintile of the same ranking. | 47 rebalances with both quintiles priced. Mean top +4.59%. Mean bottom +2.61%. | Consistent |
| 2. Among the six in-sample cells, the three skip=21 cells are not uniformly worse than the three skip=0 cells. | For lookbacks 63, 126, and 252, skip=21 is below skip=0 only for 252: pairs `[false, false, true]`. All six in-sample Sharpes are positive, so the comparison is defined. | Consistent |
| 3. Daily returns move with QQQ, so the out-of-sample Sharpe does not come from being flat in a QQQ drawdown alone. | Pearson correlation 0.026. Mean held 1.0 on 489 underwater sessions and 1.0 on 78 other sessions. | Consistent |

The three scores are consistent. The timing placebo is a separate pre-registered check, and it does not show that this rank beat a same-size random book (§6). Prediction 1 does not promote a short of the bottom quintile. Prediction 2 does not replace the primary. Prediction 3's correlation clears the locked bar of strictly greater than zero. The strategy row on session `i` is the open-to-open return that ends at that morning's open. The benchmark row is QQQ's close-to-close on session `i`. The two rows cover different hours.

## 4. Method

- **Data.** Daily bars from `data/market-data.sqlite` through `agent-data/mdq.py`, split-adjusted. Dividends are not adjusted, so price momentum understates total-return momentum. The calendar used for the lock has 1,255 NYSE sessions from 2021-10-04 through 2026-10-02: 251 warm-up sessions and 1,004 evaluated sessions. In sample is 437 sessions through 2024-06-28. Out of sample is 567 sessions from 2024-07-01. There is no session between those dates. 2025-01-09 is not a session. QQQ has no bar on 2026-10-02, so that benchmark row is 0. Later listings and holes, left missing with no forward-fill: CEG from 2022-01-19, WBD from 2022-04-04, GEHC from 2022-12-15, FER sparse from 2023-08-01 through 2024-05-02, ARM from 2023-09-14, ALAB from 2024-03-20, NBIS from 2024-10-21, SNDK from 2025-02-13, CRWV from 2025-03-28, HONA from 2026-06-15, ALNY missing 2023-09-13, SPCX missing 104 sessions. A name is eligible on a signal date only if it has bars on both formation endpoints and on the signal session. A non-positive price is treated as missing. A name that loses its bar while held contributes 0 until the exit can fill and stays in the slot.
- **Pre-registration.** `RULES.md` fixed the formation, the skip, the quintile, the 21-session rebalance, the long-only book, the 5 bp cost, the alphabetical half split, the three predictions, and the six acceptance lines before any return. `counts.py` printed session and bar counts only. Prior exposure: `spdr-sector-momentum` is 12–1 month momentum on sector ETFs and was rejected (its report summary: out of sample +6.2%, Sharpe 0.253, profit factor 0.935, from 2024-07-01 through 2026-10-01). `commodity-etf-momentum` (inconclusive) and `fx-etf-momentum` (rejected) are the same family on other ETFs and were not paper-trading candidates. Their shared out-of-sample window is 2024-07-01 through 2026-10-01. QQQ's path in that window is already described by earlier QQQ reports. This constituent ranking had not been run. The out-of-sample window is not unseen for the index. `RULES.md` was locked at git HEAD `738d3e7` with a dirty tree and was not committed. The user instruction for this study was not to commit.
- **Fills and costs.** The signal is the session close. The primary fill is the next session's open. Cost is 5 bp times the sum of absolute weight changes, charged on that open's row. A failed buy is not retried inside the hold; its `1/k` stays in cash at return 0, and the filled names are not grossed up. A failed sell waits for a later positive open and can leave gross weight above 1. No borrow fee is charged. No quoted spread was measured.
- **Returns.** Daily gross is the sum of prior weights times open-to-open simple returns. A leg with a missing open contributes 0. Flat days are 0. The account compounds. Sharpe is the mean divided by the sample standard deviation, times `sqrt(252)`. The benchmark is uncosted QQQ close-to-close. The arithmetic sum of daily net returns equals the arithmetic sum of trade nets, 2.253. The compound total return is +622.9%. `kit_schema` is 1 and `return_model` is `compound`.
- **Verification.** The store-free self-test covers ranking, the alphabetical tie break, a flat book when fewer than 5 names are eligible, the quintile count, a skip window that excludes the last 21 closes, and a missing endpoint making a name ineligible. `verify.py` imports neither `backtest.py` nor the study signal. It matched 900 trades on side and on entry and exit time and price, including 40 entry sessions drawn with seed 20261003. `python -m research.kit guard` on that file printed `ok`.
- **Runs.** Two store executions. The first computed the book and then raised `NameError` on `write_results` before `results.json`, `daily.csv`, `trades.csv`, or `RUNLOG.md` were written. No headline was saved from that crash. The second execution is the logged run, 2026-10-03T15:36:52Z, after the missing import was added. The run log states that the missing import does not change the book. Rules sha256 `40e29bd16bef992aca504d1f9e1dbda8f5e4a6ebbcd8f9a222c77f305c7a9c27`. No deviation from the locked rule was applied after seeing returns.

## 5. Results

![Growth of $1](figures/equity.svg)

The strategy ends above QQQ, the in-sample boundary is marked, and the deep loss in the path is out of sample.

![Drawdown](figures/drawdown.svg)

The strategy drawdown reaches −35.5%, deeper than QQQ's −24.2%, and that low is out of sample.

| Strategy (5 bp per unit \|Δw\|) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary, L126-S21** | +622.9% / 1.53 / −35.5% | 1.75 | +265.7% / 1.52 / −35.5% |
| *QQQ close-to-close, uncosted* | +171.9% / 1.27 / −24.2% | 1.73 | +55.2% / 0.97 / −24.2% |

There is no secondary. Full-sample t-stat of the daily mean is 3.04. The out-of-sample t-stat is 2.27.

![Calendar-year return](figures/by_year.svg)

In 2023 the strategy returned less than QQQ, and 2026 is the largest strategy year, with the −35.5% drawdown sitting in 2025.

| Year | Strategy | Sharpe | Max DD | Benchmark |
|---|---:|---:|---:|---:|
| 2022 | +7.7% | 1.32 | −7.6% | −2.7% |
| 2023 | +40.0% | 1.58 | −11.7% | +53.8% |
| 2024 | +57.4% | 1.64 | −27.8% | +24.8% |
| 2025 | +52.5% | 1.23 | −35.5% | +20.2% |
| 2026 | +99.7% | 1.99 | −33.1% | +21.1% |

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo flips the sign of each trade's gross pieces (2,000 draws, seed 20261003). Actual gross Sharpe is 1.536. The null mean is 0.008 and the null 95th percentile is 0.836. The p-value is 0.0010, which is one draw at or above the actual out of 2,000, scored as `(1 + 1) / 2001`. That rejects a coin-flip of this long book. It does not ask whether the rank was needed.

The timing placebo draws `k` names uniformly from the eligible names on each primary signal, in alphabetical order into the generator, and holds them with the same fills (500 draws, its own generator, seed 20261003). Actual gross Sharpe is the same 1.536. The null mean is 1.500 and the null 95th percentile is 1.789. The p-value is 0.417. A random quintile of the eligible names matches the published rank in this window. The timing placebo is not an acceptance line. It does not change §8.

### Bootstrap

The 20-session block bootstrap of the primary net daily path (2,000 draws, seed 20261003) puts the full-sample Sharpe between 0.71 and 2.42. The share of draws at or below zero is 0. The out-of-sample t-stat is 2.27. The interval is not an acceptance line.

### Parameter plateau

![Parameter grid](figures/grid.svg)

All six in-sample Sharpes are above zero, so the in-sample share is 1.000. The primary is in-sample rank 1. Out of sample it is the lowest cell. Picking the in-sample winner keeps the primary. The rank correlation between the in-sample and out-of-sample Sharpes is 0.086. Nothing was selected from the grid.

| Cell | IS Sharpe | OOS Sharpe |
|---|---:|---:|
| L63-S0 | 1.330 | 1.663 |
| L63-S21 | 1.525 | 1.714 |
| L126-S0 | 1.513 | 1.605 |
| **L126-S21 (primary)** | **1.753** | **1.516** |
| L252-S0 | 1.609 | 1.809 |
| L252-S21 | 1.526 | 1.744 |

Skip=21 is below skip=0 for lookback 252 only. Rebalance stays 21 and the quintile stays `N // 5` in every cell.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1× (5 bp)** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 1.536 | 1.531 | **1.525** | 1.515 | 1.504 |
| OOS Sharpe | 1.524 | 1.520 | **1.516** | 1.508 | 1.500 |
| Full-sample return | +634.2% | +628.5% | **+622.9%** | +611.7% | +600.7% |

The full-sample return at 2× cost is +611.7%, which is the line-4 actual 6.117. The return stays positive through 3×, so the break-even cost multiple is null. One extra session of delay has full-sample Sharpe 1.532, out-of-sample Sharpe 1.539, and full-sample return +634.9%. The same-bar close fill is an upper bound only: full-sample Sharpe 1.534, out-of-sample Sharpe 1.515, full-sample return +645.3%. It is close to the open book and is not the primary.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|
| HALF1 | 1.591 | 0.817 | +212.8% / 1.84 |
| HALF2 | 1.604 | 1.856 | +1270.8% / 3.00 |

HALF1 is the first 51 tickers in alphabetical order. HALF2 is the last 50. Each half is ranked with the same rule against full-sample QQQ, with `k = max(1, N // 5)` and `N` at least 5 inside that half. The split does not use returns. It is not a second market. It is the only identical-rules check this store can support, and line 5 is scored on it. Both out-of-sample Sharpes are above zero. HALF1 out-of-sample return is +72.4% on 254 trades. HALF2 out-of-sample return is +581.0% on 251 trades. Neither half replaces the primary.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

The quintiles are QQQ's daily close-to-close move. They are not a filter. Means are the strategy's same-day net return. The open-to-open strategy row and the close-to-close QQQ row cover different hours.

| QQQ move quintile | Sessions | Mean strategy net |
|---|---:|---:|
| Q1 | 201 | −31.0 bp |
| Q2 | 201 | +9.4 bp |
| Q3 | 201 | +39.0 bp |
| Q4 | 201 | +63.7 bp |
| Q5 | 200 | +31.1 bp |

Q1 is negative. Q5 is not the largest bucket. Q4 is.

By side, the book is 900 long trades and 0 short trades. By exit reason, 880 lots end at a rebalance (profit factor 2.38, average net +24.3 bp) and 20 lots are marked at the sample end with no exit cost (profit factor 15.83, average net +55.5 bp). None of these breakdowns was used to change the rule.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. Out-of-sample edge | OOS Sharpe >= 0.50 and OOS profit factor >= 1.10 | Sharpe 1.516, profit factor 2.410 | ✅ |
| 2. Direction placebo | direction placebo p <= 0.05 | 0.0009995 | ✅ |
| 3. In-sample plateau | IS Sharpe > 0.00 and at least 60% of IS grid cells have Sharpe > 0 | IS Sharpe 1.753, grid 1.000 | ✅ |
| 4. Two times cost | full-sample total return > 0 at 2× cost | 6.117 | ✅ |
| 5. Cross-market | OOS Sharpe > 0 on at least one cross-market instrument | HALF1=0.817, HALF2=1.856 | ✅ |
| 6. Sample size | at least 100 OOS trades | 520 | ✅ |

Failed lines: none. A direction placebo on a long-only book while the index rose is a low bar. Line 5 is an alphabetical split of this universe, not a second market. The timing placebo in §6 is the check on the rank, and it is outside this table.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

The rolling 126-session Sharpe is post hoc: 879 windows, 99.4% positive, minimum −0.46, maximum 4.87. It is not an input to the verdict.

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | Primary in-sample Sharpe 1.753 is rank 1. Primary out-of-sample Sharpe 1.516 is the lowest of six cells. Spearman 0.086. | The primary stays L126-S21. No cell was promoted. |
| Concentration in a few days | **(post hoc)** Zeroing the 10 best days leaves full-sample return +197.8% and Sharpe 0.98. The largest day is +16.8% on 2026-07-31. The worst is −14.3% on 2025-04-07. | No day was dropped. The verdict uses the full path. |
| Generalization | HALF2 full-sample return +1270.8% against HALF1 +212.8%. Membership is the 2026-10-02 file on every signal. The out-of-sample window is already described for QQQ. | The split is disclosed and is not a second market. A point-in-time list is a new study. |
| Execution (costs, latency, closing print) | Full-sample Sharpe moves from 1.536 at zero cost to 1.504 at 3×. Delay out-of-sample Sharpe is 1.539. Close-fill upper bound out-of-sample Sharpe is 1.515. | The locked cost stays 5 bp. The close fill stays an upper bound. |
| Short sample / regime coverage | 567 out-of-sample sessions. In 2023 the strategy returned +40.0% against QQQ +53.8%. The −35.5% drawdown is the 2025 low. | No regime filter was added after the run. |

## 10. No deployment of this ticker list

The acceptance status is paper-trading candidate. This exact list cannot be paper-traded as specified. The universe is the equity holdings of Invesco QQQ, CUSIP 46090E103, business date 2026-10-02, applied to signals back to 2022-10-03. Index membership is look-ahead. A trader on those signal dates did not have this file. Publishing the 2026 constituents as the live book would repeat that look-ahead.

The locked rule has no stop, target, or time stop inside the hold. None is added here. A negative stretch is not converted into a stop after seeing the −35.5% drawdown.

A point-in-time membership test is a new study with its own `RULES.md`. So is a dividend-adjusted rerun, a promoted grid cell, a short of the bottom quintile, or a book that trades one half.

## 11. Post hoc (not part of the verdict)

`posthoc.json` is labelled `post hoc` and its note is "post hoc. Not an input to the verdict."

The rolling 126-session Sharpe has 879 windows from 2023-04-03 through 2026-10-02. The fraction positive is 0.994. The minimum is −0.459 and the maximum is 4.872.

**(post hoc)** Setting the 10 best daily net returns to zero leaves a compound return of +197.8% and a Sharpe of 0.98, from a full-sample return of +622.9%. Setting the 10 worst to zero leaves +1715.7% and a Sharpe of 2.31. The 10 best days are 2026-07-31 (+16.8%), 2025-04-08 (+13.9%), 2025-04-10 (+11.1%), 2024-08-06 (+8.4%), 2026-03-10 (+8.2%), 2026-02-03 (+7.8%), 2026-02-09 (+7.1%), 2026-04-01 (+6.8%), 2026-06-12 (+6.6%), and 2026-06-15 (+6.5%). The 10 worst start at 2025-04-07 (−14.3%), 2025-11-21 (−10.2%), and 2024-08-05 (−9.0%). The path is concentrated in a few sessions and stays positive when those best sessions are zeroed. None of this changes §8.

### Ideas for a new study

- Point-in-time QQQ membership, on a membership history this study did not use. The rank, the quintile, and the cost would be locked again. This file's list cannot be reused as if it had been knowable in 2022.
- The same rule on dividend-adjusted total returns. These bars omit dividends. That series is data this study did not use.
- A later window, after this sample, for the question the timing placebo asked here: whether the published rank beats a random quintile of the eligible names. The p-value 0.417 was computed on this sample, so this sample cannot confirm that test.

## 12. Reproduce

From the repo root, on Windows:

```bash
python research/qqq-holdings-relative-strength/research/backtest.py
```

```bash
python research/qqq-holdings-relative-strength/research/verify.py
```

```bash
python research/qqq-holdings-relative-strength/research/posthoc.py
```

```bash
python research/qqq-holdings-relative-strength/research/charts.py
```

```bash
python -m research.kit summary research/qqq-holdings-relative-strength/research
```

```bash
python -m research.kit guard research/qqq-holdings-relative-strength/research/verify.py
```

`backtest.py` checks the lock, runs the self-test, reads the store once, and writes `results.json`, `daily.csv`, `trades.csv`, the placebo arrays, and one `RUNLOG.md` entry. `verify.py` replays the primary open book and prints the match line. `posthoc.py` writes `posthoc.json`. `charts.py` writes the eight SVGs in `report/figures/`. `summary` prints the §1 table. `guard` checks that `verify.py` does not import the study engine. Seed 20261003 for the direction placebo, the timing placebo, the bootstrap, and the verifier's entry-session draw. A rerun on the same store, under the same lock, produces the same numbers and appends another run-log entry.

### References

- Jegadeesh, N., and Titman, S. (1993). Returns to Buying Winners and Selling Losers: Implications for Stock Market Efficiency. *Journal of Finance*, 48(1), 65–91.
