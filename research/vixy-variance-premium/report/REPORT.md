# VIXY variance premium: VIXY and UVXY

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Void.** The stored VIXY series is missing reverse splits. The short died on 2013-06-10. The out-of-sample window has no trades. |
| Instruments | VIXY, fully short, reset monthly. UVXY is the cross-market line, not a replacement. |
| Data | VIXY daily bars 2011-01-04 through 2026-10-02, read via `agent-data/mdq.py`. Evaluation 2011-02-01 through 2026-10-01. The 2026-10-02 bar is ignored. |
| Rules | [`research/vixy-variance-premium/research/RULES.md`](../research/RULES.md), locked 2026-10-02T21:17:26Z, sha256 `32ca8212e029` |
| Code | [`research/vixy-variance-premium/research/`](../research/) · 2 logged store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The locked rule is a constant short of VIXY, reset at each month's next open to a weight of −1, with 5 bp on the shares traded and 1% a year of borrow. Out of sample, 2024-07-01 through 2026-10-01, the strategy returned 0 on 566 sessions. Sharpe is undefined. Profit factor is undefined. There are 0 round trips and the max drawdown of that restarted window is 0. The account had already been frozen on 2013-06-10. Full-sample terminal equity is −3.191. Full-sample total return is −419.1% and full-sample Sharpe is −0.003. In sample, through 2024-06-28, the total return is the same −419.1% because every later session is a zero.

Percentages below are `results.json` multiplied by 100 and rounded. Sharpes at least 0.05 in absolute value are shown to two decimals. Smaller Sharpes are shown to three decimals. Section 8 quotes the acceptance values as stored.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2011-02-01 → 2026-10-01 | 2011-02-01 → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,940 | 3,374 | 566 |
| Total return | −419.1% | −419.1% | 0 |
| CAGR | undefined | undefined | 0 |
| Annual volatility | 134.9% | 145.8% | 0 |
| Sharpe | −0.003 | −0.003 | undefined |
| Max drawdown | −316.3% | −316.3% | 0 |
| Trades / profit factor | 29 / 0.28 | 29 / 0.28 | 0 / undefined |
| Avg net trade | −941 bp | −941 bp | undefined |
| *Long VIXY Sharpe (max DD)* | +0.05 (−100.0%) | +0.07 (−99.9%) | −0.16 (−81.4%) |

`results.json` records status `Inconclusive` because line 6 failed. The locked status map also says **Void** when a data defect cannot be fixed without changing the rules. That is this study. The six lines in §8 are the mechanical score of the defective series. They are not a test of the variance premium.

**Why.**

1. On 2013-06-10 the adjusted VIXY close and the raw VIXY close both jump by 4.947, from a raw close of 10.26 to 50.76 (`jumps.json`). ProShares' 24 May 2013 exhibit says VIXY reverse-splits 1-for-5 and trades at the post-split price on 10 June 2013. The locked rule uses `mdq` with `adjust=True`. That series was not adjusted on this date. The short is marked ruined at the close. Terminal equity is −3.191. Shares stay 0 through 2026-10-01.
2. A second VIXY jump of the same kind is 2016-07-25, raw ratio 4.966. An OCC memo sets a 1-for-5 VIXY reverse split effective at the open that day. The strategy is already flat, so the jump shows up in the long-VIXY benchmark, including the 2016 benchmark return of +59.5%, and not in the strategy.
3. UVXY, under the same rule, ruins on 2012-03-08. Adjusted and raw closes both jump by 5.507. A 27 February 2012 notice sets a 1-for-6 UVXY reverse split with post-split trading on 8 March 2012. UVXY out-of-sample Sharpe is undefined. The same SEC exhibit sets a 1-for-10 UVXY reverse split for 10 June 2013. `jumps.json` has that day at 9.770, plus four other UVXY sessions whose adjusted ratio equals the raw ratio and is above 3.
4. The pre-lock check, and the check inside `backtest.py`, tested only the splits stored in the database. Those four VIXY splits and seven UVXY splits pass. The missing ones were not on that list. Rewriting the price series inside the study, after seeing the ruin, would change the locked rule. Writing splits into `data/` was not allowed.
5. On this path, friction is 0.021 dollars and the ruin holding's net is −4.284. The zero-cost full-sample return is −430.8%. Costs are not the loss. The 28 `next_open` holdings sum to +0.093 net dollars and then stop. That sum is not a measured premium.

**Recommendation.** Do not trade it. Do not drop 2013, 2016, 2018, 2020, or 2024. Do not switch the primary to UVXY. Do not add a VIX filter, a trend filter, or a volatility target. Do not take a grid weight other than −1. A corrected price series is a different study.

## 2. The strategy

### Rules

```
book = the product's own daily sessions on or before 2026-10-01
signal = last session of each calendar month in that book
target weight = -1 at every signal
fill at the next session's open
shares_new = -1 * equity_at_open / open     # before the rebalance cost
charge 5 bp on abs(shares traded) * open, not on the whole short when only resizing
existing shares earn the gap; new shares earn open-to-close
borrow = abs(shares) * prior_close * 0.01 / 252 when the short was held overnight
a missing bar earns 0 and does not close the short
if equity at a fill open is <= 0: cover, pay 5 bp, reason ruin, freeze
if equity at a close is <= 0: mark ruin at the close, no extra commission, freeze
later sessions earn 0; equity is not floored at 0
```

- **Why weight −1 with no signal:** the hypothesis is the average variance premium, not a timed short. The weight was fixed before any VIXY return was computed.
- **Why 5 bp and 1% borrow:** both were fixed in the request. The store has no borrow quotes. The sweep scales both together. Neither was changed after the P&L.
- **Why the freeze:** a fully funded short can lose more than its equity. The freeze is limited liability. It is not a VIX filter. It does not resize a negative equity into a long.

### How it trades

| | |
|---|---|
| Sessions with a trade | 29 holdings, first fill 2011-02-01. The last entry is 2013-06-03. |
| Trades per year | 1.85 over the full sample; 2.17 in sample. The full-sample rate counts the flat years after the freeze. |
| Time in market | Exposure 0.15 full sample, 0.175 in sample, 0 out of sample. |
| Holding time | Median 21 sessions, mean 20.4. |
| Long / short | 29 short. Net dollars −4.191. Profit factor 0.28. There is no long. |
| Win rate | 72.4%. Average winner +1,372 bp. Average loser −7,012 bp. |

Exit reasons: `next_open` 28 holdings, net dollars +0.093, gross dollars +0.114, win rate 0.75; `ruin` 1 holding, net dollars −4.284, gross dollars −4.284. The ruin row is short, entered 2013-06-03 at 09:30 at 3302.4, exited 2013-06-10 at 16:00 at 16243.2, hold 6, entry equity 1.093. No holding has reason `end_of_sample`.

## 3. Hypothesis and predictions

Sellers of volatility insurance are paid for crash risk (Bakshi and Kapadia 2003; Carr and Wu 2009). The other side is the hedger who buys short-dated VIX futures. VIXY's roll is that long-futures position. Simon and Campasano (2014) trade the futures basis. This study shorts the ETP every month. There is no timing rule.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Full-sample net total return at 5 bp and 1% borrow is positive. | Stored value −4.191. | Not consistent |
| The sum of the ten worst `strategy_net` sessions is negative and larger in absolute value than 50 times the average session. | `results.json` records the sum −7.075, the mean session −0.0000173, and the threshold 0.000863, and scores the prediction consistent. | Consistent on the stored series. Not evidence of crash skew. |

The ten worst sessions are in `posthoc.json`. The worst is 2013-06-10, `strategy_net` −3.902, and it is 0.552 of the ten-worst sum. That session is the unrecorded 1-for-5 split. The other nine worst sessions are in 2011, mostly August, and they are not rows in `jumps.json`. The best session is 2011-08-23 at +3.153, also not a jump row. A constant share count lets the short weight drift above 1 after losses, so one down day in the product can print a very large account return. Prediction 2's score mixes that drift with the split print. It does not confirm the mechanism. Neither prediction is an acceptance line.

## 4. Method

- **Data.** `counts.py` ran before the lock: coverage, stored corporate actions, and month-end counts. No P&L. VIXY has 3,960 daily bars, 2011-01-04 through 2026-10-02, of which 3,959 are on or before 2026-10-01. UVXY has 3,771 daily bars from 2011-10-04, of which 3,770 are on or before 2026-10-01. SPY has 3,959 daily bars through 2026-10-01 and no corporate actions. NYSE was closed 2012-10-29, 2012-10-30, and 2018-12-05. Those dates are skipped. 2025-01-09 is a holiday and is not a session. 2021-12-31 is a partial daily bar and is kept. No dividends are stored. VIXY and UVXY have no 1-minute bars, so the known SPY extended-hours daily defect from about 2024-11-20 cannot be checked on these two products. The study uses the stored daily bars.
- **Corporate actions.** The stored VIXY splits, 2017-07-17, 2021-05-26, 2023-06-23, and 2024-11-07, are adjusted. The stored UVXY splits from 2017-01-12 through 2025-11-20 are adjusted. `jumps.json`, written by `verify.py`, lists sessions where the adjusted close ratio and the raw close ratio are both above 3. VIXY: 2013-06-10 (4.947) and 2016-07-25 (4.966). UVXY: 2012-03-08 (5.507), 2012-09-07 (8.857), 2013-06-10 (9.770), 2014-01-24 (4.703), 2015-05-20 (5.017), and 2016-07-25 (4.951). The engine does not apply a second split, and it was not given a corrected series after the run.
- **Pre-registration.** Rules were locked before the backtest. `RULES.md` was not committed. The lock's git HEAD is `577dd38`. The logged runs saw HEAD `738d3e7` with a dirty tree. The out-of-sample window was already used by earlier equity and ETF studies. It is not unseen. From those reports, before any VIXY return: SPY buy-and-hold over this window was about +40.5% (Sharpe about 0.99, max drawdown about −20%), including about +10.5% on 2025-04-09. This study's own SPY close-to-close over the same dates is +40.5%, Sharpe 0.98, max drawdown −19.9%. Calendar SPY returns in `results.json` are +23.4% in 2024, +16.4% in 2025, and +12.0% in 2026 through 2026-10-01. No earlier study in the repo used VIXY, UVXY, SVXY, VXX, or this rule. Public knowledge of the 2018, 2020, and 2024 volatility spikes was written down before the lock and was not used to drop a year.
- **Fills and costs.** Primary fill is the next open. `delay1` fills two book sessions after the signal and is reported only. Same-close is a labelled upper bound, not a candidate. Borrow uses the prior close notional.
- **Returns.** Daily simple returns. A flat or frozen session is 0. Sharpe is the mean divided by the sample standard deviation, times √252, with a zero risk-free rate. Sharpe is null when the sample standard deviation is 0. CAGR is null when window terminal equity is ≤ 0. Trip membership uses the entry date. The daily return on 2024-07-01 would be out of sample even if a holding had entered earlier. No such holding exists. Cash is zero on every session. SPY close-to-close is a correlation series, not the hurdle.
- **Verification.** `backtest.py` runs the synthetic self-test before it opens the store. The logged store run exited 0, so that self-test passed. `verify.py` does not import the study engine. It matched all 29 VIXY holdings on side, entry time and price, exit time and price, exit reason, hold, gross, and net. The population is 29, so a sample of 40 without replacement does not exist. Seed 20261074 redrew 40 indexes with replacement from those 29. UVXY out-of-sample Sharpe is null in both the replay and `results.json`. `python -m research.kit guard research/vixy-variance-premium/research/verify.py` prints `ok`.
- **Runs.** Two logged store runs. The first `backtest.py` process crashed after the path was computed, on a Spearman rank of undefined out-of-sample Sharpes, and wrote no output file. The logged backtest run is the fix for that crash. Its reason states that there is no before headline. The headlines in that entry are the only stored headlines. `verify.py` is the second entry. It matched. No rerun changed a headline.

### Checklist

- The `RULES.md` hash matches `RULES.lock` and `results.json`.
- `results.json` does not have `kit_schema` 1. The locked rules specify this study's columns, and the file was already written. `python -m research.kit.charts` and `python -m research.kit.summary` cannot read it. `charts.py` draws the eight figures from the saved files.
- Both `RUNLOG.md` entries have a permitted reason: a code crash, then an independent replay.
- The self-test passed on the logged backtest process. `verify.py` matched every holding.
- The report status is Void under the locked defect clause. The `status` field in `results.json` remains `Inconclusive`, which is the line-6 branch on the uncorrected series.
- Post-hoc numbers are labelled and do not feed §8.
- Prior exposure and the coverage gaps are stated above.
- Seeds are 20261071, 20261072, 20261073 (recorded, unused), and 20261074.
- This folder's `README.md` has the status and the commands. `research/README.md` does not list the study. The request forbade that edit.
- Nothing was written to `data/`. Ingest was not run.

## 5. Results

![Growth of $1](figures/equity.svg)

The short falls through zero on 2013-06-10 and stays at −3.191 through the out-of-sample boundary. The plunge is the unrecorded 1-for-5 reverse split, not a volatility spike. The uncosted long VIXY is on the same axes and decays toward zero. The vertical mark in 2013 is that split date. The dashed line is 2024-07-01.

![Drawdown](figures/drawdown.svg)

Drawdown uses each series' running peak. The short's drawdown reaches −316% and does not recover. The long VIXY drawdown reaches about −100%.

| Strategy (5 bp and 1% borrow) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary, VIXY weight −1** | −419.1% / −0.003 / −316.3% | −0.003 | 0 / undefined / 0 |
| *Uncosted long VIXY* | −99.9% / +0.05 / −100.0% | +0.07 | −60.7% / −0.16 / −81.4% |
| *SPY close-to-close* | +494.3% / +0.75 / −34.1% | +0.71 | +40.5% / +0.98 / −19.9% |
| *Cash* | 0 / undefined / 0 | undefined | 0 / undefined / 0 |

Pearson correlation of `strategy_net` with SPY is 0.120 full sample and 0.129 in sample. Correlation with the long VIXY benchmark is −0.508 full sample and −0.517 in sample. Both out-of-sample correlations are null because the strategy is flat.

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | Long VIXY | Trades |
|---|---:|---:|---:|---:|---:|
| 2011 | −71.1% | +0.65 | −97.3% | +11.7% | 11 |
| 2012 | +147.1% | +1.70 | −38.5% | −77.5% | 12 |
| 2013 | −547.7% | −0.87 | −375.7% | +67.7% | 6 |
| 2014 | 0 | undefined | 0 | −26.4% | 0 |
| 2015 | 0 | undefined | 0 | −36.5% | 0 |
| 2016 | 0 | undefined | 0 | +59.5% | 0 |
| 2017 | 0 | undefined | 0 | −72.8% | 0 |
| 2018 | 0 | undefined | 0 | +66.8% | 0 |
| 2019 | 0 | undefined | 0 | −67.8% | 0 |
| 2020 | 0 | undefined | 0 | +10.5% | 0 |
| 2021 | 0 | undefined | 0 | −72.4% | 0 |
| 2022 | 0 | undefined | 0 | −25.0% | 0 |
| 2023 | 0 | undefined | 0 | −72.7% | 0 |
| 2024 | 0 | undefined | 0 | −27.4% | 0 |
| 2025 | 0 | undefined | 0 | −43.1% | 0 |
| 2026 | 0 | undefined | 0 | −33.4% | 0 |

2011 can show a positive Sharpe and a large negative return because the share count is constant between month-ends and the weight drifts. 2013 is the unrecorded split. From 2014 on, the strategy return is zero. The 2016 long-VIXY return of +59.5% is the second missing VIXY reverse split. The 2018 long-VIXY return is not a row in `jumps.json`.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo flips each holding's gross dollars on the zero-cost, zero-borrow path, 2,000 draws, seed 20261071. The actual gross Sharpe is −0.0059. The null mean is −0.0002. The null 95th percentile is 0.310. 1,003 draws are at least as large as the actual. p = (1 + 1,003) / 2,001 = 0.502. The gross path is not distinguishable from sign flips. The placebo does not repair the price series. The timing placebo was not run. The rule is short at every rebalance, so redrawing months with the same always-short count reproduces the strategy. Seed 20261073 is unused.

### Bootstrap

Circular 20-session blocks of the full-sample net daily returns, 2,000 draws, seed 20261072. The 2.5th and 97.5th percentiles of Sharpe are −0.366 and +0.639. The median is +0.012. The fraction of draws with Sharpe ≤ 0 is 0.48. The interval is wide because one session dominates the variance. The out-of-sample t-stat is null.

### Parameter plateau

![Parameter grid](figures/grid.svg)

| Weight | IS Sharpe | IS return | OOS Sharpe | OOS return |
|---|---:|---:|---:|---:|
| −0.50 | −0.184 | −241.8% | undefined | 0 |
| −0.75 | −0.177 | −382.5% | undefined | 0 |
| **−1.00** | **−0.003** | **−419.1%** | **undefined** | **0** |
| −1.25 | −0.160 | −103.6% | undefined | 0 |
| −1.50 | −0.062 | −135.1% | undefined | 0 |

Zero of five in-sample Sharpes are positive. The primary has the least negative in-sample Sharpe, so its in-sample rank is 1, and it has the most negative in-sample total return. Those two facts can sit together because a ruin followed by years of zeros pulls the Sharpe toward zero without repairing the terminal loss. Every out-of-sample Sharpe is null, so the Spearman correlation of in-sample and out-of-sample Sharpe is null. Nothing was selected from the grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

The multiple scales both the 5 bp commission and the 1% borrow.

| Cost multiple | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.006 | −0.005 | **−0.003** | −0.000 | +0.003 |
| OOS Sharpe | undefined | undefined | **undefined** | undefined | undefined |
| Full-sample return | −430.8% | −424.9% | **−419.1%** | −407.8% | −396.8% |

No multiple has a positive full-sample total return. Out-of-sample return is 0 at every multiple. The 3× Sharpe is positive while the account is still ruined. That is the zeros after the freeze, not a cost the strategy can pay. There is no break-even cost in this sweep.

`delay1` first fills on 2011-02-02, ruins, and has a full-sample return of −370.0% and a full-sample Sharpe of −0.15, with 29 holdings and 0 out of sample. Same-close first fills on 2011-01-31, ruins, and has a full-sample return of −564.2% and a full-sample Sharpe of −0.13. The upper bound is also a dead account. It is not a candidate.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF / trades |
|---|---:|---:|---|
| UVXY, weight −1 | −0.21 | undefined | −1,092% / 0.14 / 5 |

UVXY's first fill is 2011-11-01. It ruins on 2012-03-08. Terminal equity is −9.922. Out of sample it has 0 trades on 566 sessions. It does not replace VIXY.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Mean `strategy_net` by quintile. Each quintile has 788 sessions. Quintile 1 is the lowest move.

| Quintile | SPY sort mean | Strategy, by SPY | VIXY sort mean | Strategy, by VIXY |
|---|---:|---:|---:|---:|
| 1 | −1.37% | −1.01% | −5.18% | +1.41% |
| 2 | −0.28% | −0.08% | −2.13% | +0.39% |
| 3 | +0.07% | −0.49% | −0.61% | +0.07% |
| 4 | +0.46% | +0.22% | +1.03% | −0.21% |
| 5 | +1.38% | +1.35% | +7.05% | −1.67% |

The VIXY ordering puts the 2013-06-10 jump in the highest product quintile. The slope is not a clean premium. The SPY ordering is not monotonic. Neither split was used as a filter.

The pre-registered stress windows are February 2018 (19 sessions), March 2020 (22), August 2024 (22), and April 2025 (21). Strategy return is 0 in each. The account was already frozen. These windows are not a test of those crashes.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS Sharpe and profit factor | Sharpe ≥ 0.5 and PF ≥ 1.10 | Sharpe null, PF null | ❌ |
| 2. Direction placebo | p ≤ 0.05 | p = 0.5017491254372813 | ❌ |
| 3. IS Sharpe and grid | IS Sharpe > 0 and at least 3 of 5 cells > 0 | IS Sharpe −0.0034833348778102327; 0 of 5 | ❌ |
| 4. Full-sample return at 2× | > 0 | −4.077588742799616 | ❌ |
| 5. UVXY OOS Sharpe | > 0 | null | ❌ |
| 6. OOS round trips | ≥ 24 | 0 | ❌ |

Line 6 fails, so the engine's status string is Inconclusive even though lines 1 through 5 also fail. The study is Void because the missing reverse splits cannot be fixed without changing the locked price rule. A pass or a fail on this table is not evidence about the variance premium. The placebo is gross of costs, and it is still far from 0.05.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| Bad prices | Two VIXY sessions and six UVXY sessions still jump in the adjusted series. The primary ruin date is one of them. | None in this study. The series was not rewritten. |
| Edge decay | There is no out-of-sample position to decay. The trailing 252-session Sharpe is undefined at the sample end. | Not a trading problem to manage. The account is closed. |
| Concentration | **(post hoc)** The worst session is 0.552 of the ten-worst sum. The ten best sessions sum to 5.707 against 14.702 of positive-session sum, a ratio of 0.388. | Not used to drop a day. |
| Generalization | UVXY ruins on its own missing split, eighteen months earlier. | UVXY was not promoted. |
| Execution | Friction on the stored path is 0.021 dollars. The 1% borrow was never observed in a quote. Same-close and `delay1` also ruin. | No fill rule in the sweep produces an out-of-sample trade. |
| Sample | 0 out-of-sample round trips against a floor of 24. The economic short never reaches 2018, 2020, or 2024. | The floor was not lowered. |

## 10. No deployment

There is no paper-trading proposal. The primary is not a test of a live short, and it failed the tests written to decide that question. A different price series, a later sample, or a different rule needs its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

Computed from `daily.csv` only. None of it changes §8. Zeroing a session is not a split correction. Sessions after the freeze stay at 0.

- Trailing 252-session Sharpe is defined on 592 windows. The minimum is −1.01. The maximum is +2.35. The fraction positive is 0.574. The last defined window ends 2014-06-09 with Sharpe −1.00. The last session of the sample has no defined trailing Sharpe.
- Setting the 20 best `strategy_net` sessions to 0 produces a total return of −1.036 and a Sharpe of −0.405. The best session, 2011-08-23 at +3.153, is not an unadjusted jump. Removing it changes the compound because later percentage losses, including the split day, are applied to a path that no longer includes that gain.
- Every one of the 566 out-of-sample `strategy_net` values is 0. There is no best out-of-sample day to remove.

### Ideas for a new study

The same always-short monthly rule can be registered again only on a price series that already contains the missing reverse splits, and only on data this study did not use. This sample and this file are used up. A VIX filter, a trend filter, a volatility target, or a switch to UVXY fitted on this path is not that study.

## 12. Reproduce

From the repo root:

```bash
python research/vixy-variance-premium/research/backtest.py
```

```bash
python research/vixy-variance-premium/research/verify.py
```

```bash
python research/vixy-variance-premium/research/posthoc.py
```

```bash
python research/vixy-variance-premium/research/charts.py
```

`backtest.py` checks the lock, runs the self-test, then writes `results.json`, `daily.csv`, `trades.csv`, and `placebo_direction.npy`, and appends `RUNLOG.md`. `verify.py` replays the holdings, writes `jumps.json`, and appends `RUNLOG.md`. `posthoc.py` reads `daily.csv` and writes `posthoc.json` and `rolling_sharpe.csv`. It does not open the store. `charts.py` reads those files and writes the eight SVGs under `report/figures/`. Seeds: direction 20261071, bootstrap 20261072, timing 20261073 unused, verify 20261074. A later `backtest.py` run on the same store and the same code appends another log entry and reproduces the stored headlines. Do not run it to refresh the prose.

### References

- Bakshi, G., and N. Kapadia (2003). Delta-Hedged Gains and the Negative Market Volatility Risk Premium. *Review of Financial Studies*.
- Carr, P., and L. Wu (2009). Variance Risk Premiums. *Review of Financial Studies*.
- Simon, D. P., and J. Campasano (2014). The VIX Futures Basis: Evidence and Trading Strategies. *Journal of Derivatives*.
- ProShares (24 May 2013). Exhibit 99.1, ETF share splits. SEC accession via `https://www.sec.gov/Archives/edgar/data/1415311/000119312513241209/d546453dex991.htm`. VIXY 1-for-5 and UVXY 1-for-10, post-split trading 10 June 2013.
- IndexUniverse / Nasdaq (27 February 2012). ProShares 1-for-6 reverse split of UVXY, post-split trading 8 March 2012.
- OCC information memo 39300 (12 July 2016). VIXY 1-for-5 reverse split effective at the open on 25 July 2016. `https://www.miaxglobal.com/sites/default/files/alert-files/VIXY_Split_39300.pdf`.
