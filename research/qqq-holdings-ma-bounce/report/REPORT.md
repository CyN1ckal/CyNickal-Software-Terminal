# QQQ-holdings 150/250 moving-average bounce

| | |
|---|---|
| Date | 2026-10-03 |
| Status | **Paper-trading candidate.** Passed all 6 pre-registered tests. The mechanism prediction on exit type did not. |
| Instruments | 101 Invesco QQQ equity holdings, CUSIP 46090E103, business date 2026-10-02. Equal-weight long book. QQQ is the benchmark. QQQ and SPY are the cross-market rows. |
| Data | 2022-10-03 → 2026-10-02 evaluated, read via `agent-data/mdq.py`, split-adjusted daily bars. Dividends are not adjusted. Missing bars are not forward-filled. |
| Rules | [`research/qqq-holdings-ma-bounce/research/RULES.md`](../research/RULES.md), locked 2026-10-03, sha256 `a3303e9e97f3` |
| Code | [`research/qqq-holdings-ma-bounce/research/`](../research/) · 1 store run (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Out of sample, from 2024-07-01 through 2026-10-02, the 5 bp book returned 0.6545142383828009 with Sharpe 1.0171774262432778, profit factor 1.4422280396580611, and 476 trades. The full sample, 2022-10-03 through 2026-10-02, returned 1.4550571919689181 with Sharpe 1.1060682655093068 and 734 completed trades. Uncosted QQQ returned 0.5520444156874218 out of sample (Sharpe 0.9654003957787437) and 1.7823093616702859 over the full sample (Sharpe 1.294334133519726).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2022-10-03 → 2026-10-02 | 2022-10-03 → 2024-06-28 | 2024-07-01 → 2026-10-02 |
| Sessions | 1004 | 437 | 567 |
| Total return | +145.5% | +48.4% | +65.5% |
| CAGR | +25.3% | +25.6% | +25.1% |
| Annual volatility | +22.7% | +19.2% | +25.1% |
| Sharpe | 1.11 | 1.28 | 1.02 |
| Max drawdown | −19.5% | −13.8% | −19.5% |
| Trades / profit factor | 734 / 1.51 | 258 / 1.63 | 476 / 1.44 |
| Avg net trade | +13.0 bp | +16.3 bp | +11.3 bp |
| Benchmark Sharpe (max DD) | 1.29 (−24.2%) | 1.79 (−11.4%) | 0.97 (−24.2%) |

It passed all 6 of the 6 acceptance tests written before the first run (§8).

**Why.**

1. Prediction 1 is not consistent. Completed time-stop trades have a higher mean gross account return than completed trend-break trades. The trend-break group loses money. The decay story is not what the trades show (§3, §7).
2. The timing placebo does not separate these entry dates from random entry sessions that use the same exits. Its p is 0.7245508982035929. Acceptance required the direction placebo, which passed at 0.005997001499250375 (§6).
3. Over the full sample the book trails QQQ on total return and Sharpe. Calendar 2023 is the wide gap (§5).
4. The universe is the holdings list as of 2026-10-02, applied to every earlier session. Index membership is look-ahead. The out-of-sample window is not unseen at the index level (§4).
5. Eighteen positions are still open at the last session. Their earned open-to-open steps sit in the daily path. They are not rows in `trades.csv` and they are not in the direction-placebo pieces (§2, §6).

**Recommendation.** Paper trading of this locked rule, with the halt rules in §10. This is not a live-capital decision. The study keeps the primary at SMA 150/250 and a 20-session hold. A grid hold, another average length, a short side, an ATR stop, a profit target, published QQQ weights, a dropped name, the same-bar close fill, and the ex-NVDA book each stay out of this verdict.

## 2. The strategy

### Rules

```
SMA150_t = mean of the 150 most recent split-adjusted closes ending at t
SMA250_t = mean of the 250 most recent split-adjusted closes ending at t
sessions with no bar are omitted, not filled

at an evaluated close, both averages defined:
  trend  = close_t > SMA250_t and SMA150_t > SMA250_t
  touch  = low_t is inside the closed interval between SMA150_t and SMA250_t
  bounce = close_t > SMA150_t
  if trend and touch and bounce, and the name is flat with no entry waiting:
      enter long at the next session open that exists

exit, checked at the close, filled at the next open that exists, in order:
  (1) close_t < SMA250_t
  (2) the open 20 evaluated sessions after the entry session (entry counts as 0)
  if both fill the same open, the reason is trend

book: equal weight of names held after that open; weights sum to 1, else 0
return: weight_prev * (open_D / open_prev - 1), minus 0.0005 * sum(|dw|)
```

- **Why 150 and 250:** the user specified those lengths. They were locked before the run and were not estimated on this sample.
- **Why 5 bp:** these are single stocks. The protocol's 1 bp is for QQQ/SPY-class ETFs. The cross-market rows use 1 bp.
- **Why this universe:** the 101 equity holdings of Invesco QQQ, CUSIP 46090E103, business date 2026-10-02. Published end-of-sample weights are not used.

### How it trades

| | |
|---|---|
| Completed trades | 734 full, 258 in-sample, 476 out-of-sample. 18 positions still open |
| Time in market | exposure 0.9840637450199203 full, 0.9633867276887872 in-sample, 1.0 out-of-sample. `sessions_held` 988 |
| Holding time | median 20.0 evaluated sessions, mean 15.520435967302452 |
| Exit mix, full sample | time 441 trades, trend 293 trades |
| Long / short | long only. Full-sample profit factor 1.5103001124563558 |
| Win rate | full 0.48501362397820164; average winner 79.57577096852957 bp, average loser −49.622175270255646 bp |
| Out-of-sample win rate | 0.4642857142857143; average winner 79.13775208081937 bp, average loser −47.55562221605253 bp |
| Average net trade | full 13.040588845558435 bp, in-sample 16.314080108031817 bp, out-of-sample 11.26630156463799 bp |

## 3. Hypothesis and predictions

The locked claim is that a constituent in an uptrend earns a positive open-to-open drift after a dip tags the band between its 150-session and 250-session averages and the same close finishes back above the faster average. The economic cousins named in the rules are Faber's 10-month trend filter (2007) and intermediate-horizon momentum (Jegadeesh and Titman, 1993). The lengths themselves are the user's.

The profit-and-loss tests passed. The mechanism is unconfirmed: prediction 1, the one about the exit, is not consistent.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Time-stop trades have a lower mean gross account return than trend-break trades | Full sample: time mean gross 0.005833819942488897 on 441 trades; trend mean gross −0.0051297740650633914 on 293 trades | Not consistent |
| 2. Mean daily net return is higher in QQQ-move quintiles 4 and 5 than in quintiles 1 and 2 | `upper_mean` 0.0028864257492959983, `lower_mean` −0.0010654485654402134 | Consistent |
| 3. The out-of-sample result is not only NVDA | Primary OOS total return 0.6545142383828009. Ex-NVDA OOS total return 0.6687768706515405, OOS Sharpe 1.0277057800733738, 466 trades | Consistent |

The ex-NVDA book is prediction 3 only. It is not a second primary. A failed prediction does not become a new rule.

## 4. Method

- **Data.** Split-adjusted daily bars from `agent-data/mdq.py` (`adjust=True`). Dividends are not adjusted, so a cash dividend is a price gap. The pre-lock coverage file is `counts.json`: 1,004 evaluated NYSE sessions, 2022-10-03 through 2026-10-02, after 251 warm-up sessions. A name trades only on sessions where it has a bar. Late listings inside the window are CEG, WBD, GEHC, ARM, ALAB, NBIS, SNDK, CRWV, FER, and HONA. Vendor holes with no bar include ALNY 2023-09-13, FER from 2023-08-02 through 2024-05-02, and SPCX including 2026-04-07 through 2026-05-22 and 2026-05-26 through 2026-06-11. 2025-01-09 is the day-of-mourning closure and is not a session. QQQ and SPY have no bar on 2026-10-02; that session's benchmark return is 0. Only ADBE and GOOGL have any 1-minute bars, and this study does not read them. HONA remains in the universe.
- **Membership.** This is the end-of-sample list, not a historical constituent tape. Index membership is look-ahead. The test is these stocks' prices, not a tradable point-in-time QQQ book.
- **Pre-registration.** `RULES.md` was locked at sha256 `a3303e9e97f39135c33d73a5b9a2e165dd742755962d4fdf5c8a6b2c777bba39` before the store run. `results.json` carries the same hash and `"kit_schema": 1`. Prior exposure, written before any return: constituent daily paths had not been studied. `spy-rsi2-dip-buy` was already a paper-trading candidate. `qqq-atr-band-dip-eod` was already rejected. The five house daily studies on the shared window through 2026-10-01 were not paper-trading candidates. QQQ intraday studies through 2026-09-25 already describe QQQ's path, including April 2025. The out-of-sample window is not unseen at the index level. The rules file was not committed. The user instruction was not to commit. `research/README.md` was not updated. The user instruction was not to edit that index, so the skill checklist item for the index fails on purpose.
- **Fills and costs.** The close is known at 16:00 ET. The primary fill is the next session's open. Base cost is 5 bp of notional per side. No quoted-spread tape was used. Cash earns 0.
- **Returns.** Daily simple returns. A flat day is 0. Compound equity. Sharpe is mean / sample standard deviation × √252, with a zero risk-free rate. A held name with no bar contributes 0 that session and stays in the equal-weight set.
- **Verification.** `backtest.py` runs the synthetic self-test and aborts before opening the store if a case fails. The cases cover entry, both exits, a missing bar, an already-open position, and a name without 250 closes. `python -m research.kit guard research/qqq-holdings-ma-bounce/research/verify.py` printed `ok`. `verify.py` does not import the study engine. It matched all 734 completed trades on side, entry time, entry price, exit time, and exit price.
- **Runs.** One store run, 2026-10-03T15:50:12+00:00, reason `initial pre-registered store run`. Git HEAD `738d3e7ec4340e05458317114dfe247d55a1ef16`, dirty. No later run. Seeds for the direction placebo, the timing placebo, and the bootstrap are each 20261003.

## 5. Results

![Growth of $1](figures/equity.svg)

The book ends the full sample below uncosted QQQ: total return 1.4550571919689181 against 1.7823093616702859. Out of sample it is above QQQ: 0.6545142383828009 against 0.5520444156874218. The in-sample gap is 0.4838537711035966 against 0.7926738007932346. The vertical mark is 2024-07-01.

![Drawdown](figures/drawdown.svg)

The book's full-sample maximum drawdown is −0.1948181551727206, and the out-of-sample maximum drawdown is −0.19481815517272105. The worst path is in the out-of-sample window. QQQ's full-sample maximum drawdown is −0.24200124703976167.

| Strategy (5 bp) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | 1.4550571919689181 / 1.1060682655093068 / −0.1948181551727206 | 1.2799232105820915 | 0.6545142383828009 / 1.0171774262432778 / −0.19481815517272105 |
| *QQQ close-to-close, uncosted* | 1.7823093616702859 / 1.294334133519726 / −0.24200124703976167 | 1.7942491656054071 | 0.5520444156874218 / 0.9654003957787437 / −0.24200124703976178 |

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | Benchmark |
|---|---:|---:|---:|---:|
| 2022 (from 2022-10-03) | 0.11541220295047516 | 1.9276016842222796 | −0.08781030014663604 | −0.0036668412781563076 |
| 2023 | 0.1659872508868656 | 0.8926184756456509 | −0.13805399769485327 | 0.5379299984978212 |
| 2024 | 0.1810976161772846 | 1.0287749445406256 | −0.10002817386755625 | 0.2483883571009935 |
| 2025 | 0.1842815331076897 | 0.7902770492542207 | −0.19469492428665824 | 0.2015100539863881 |
| 2026 (through 2026-10-02) | 0.3495583612061173 | 1.5896037995167929 | −0.11161059634330639 | 0.210562302608017 |

2022 and 2026 are partial evaluated windows. 2024 contains both in-sample and out-of-sample sessions. In 2023 the strategy return is 0.1659872508868656 and the QQQ return is 0.5379299984978212.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

**Direction.** Each draw keeps the completed trades' dates and flips every trade's gross account pieces by one sign. 2,000 draws, seed 20261003. Null mean gross Sharpe 0.011270428648953293, 95th percentile 0.7966022164109147. The actual completed-trade gross Sharpe is 1.1893580305038518. p = 0.005997001499250375. The full-path gross Sharpe, which still includes the 18 open positions, is 1.232077565576937. The direction test compares the completed-trade figure. It shows that the signed book beat a coin-flip of the same dates. It does not show that the entry calendar mattered.

**Timing.** 500 draws, seed 20261003, 500 attempts, so every attempt placed the full per-name count. Same completed-trade count per name, random entry sessions, the same trend-then-time exits, non-overlapping holds. The statistic is the zero-cost equal-weight gross Sharpe. Null mean 1.3346168037557307, 95th percentile 1.763801847101114. The actual 1.1893580305038518 gives p = 0.7245508982035929. The actual sits below the null mean. This check is not an acceptance line.

### Bootstrap

Circular 20-session blocks, 2,000 draws, seed 20261003, on the primary net path. The 95% interval of the full-sample Sharpe is 0.21618591222994615 to 1.9654424127006789. The share of draws at or below 0 is 0.007. The t-statistic of the mean daily net return is 2.2077430082486433 full sample, 1.6854829470388604 in sample, and 1.525766139364917 out of sample. The bootstrap is not an acceptance line. The interval extends below the 0.5 out-of-sample Sharpe threshold.

### Parameter plateau

![Parameter grid](figures/grid.svg)

| Hold | IS Sharpe | OOS Sharpe | Full return | Primary |
|---|---:|---:|---:|---|
| 10 | 1.368698182732263 | 1.0717531320037157 | 1.851609819895403 | |
| 15 | 0.8565319732715418 | 0.8259684808623812 | 0.9192723661264588 | |
| **20** | **1.2799232105820915** | **1.0171774262432778** | **1.4550571919689181** | yes |
| 30 | 1.6037186000588468 | 1.6724275777566233 | 2.7050417734981007 | |
| 40 | 1.7023520145320268 | 1.5703352135788382 | 2.459764835905482 | |

All five in-sample Sharpes are positive. The acceptance actual is `IS Sharpe 1.280, grid 1.000`. Hold 20 is below holds 10, 30, and 40 on the in-sample Sharpe. Hold 15 is the low cell on both samples. The in-sample maximum is hold 40. Its out-of-sample Sharpe is 1.5703352135788382. Hold 30's out-of-sample Sharpe is 1.6724275777566233. **(post hoc)** The rank correlation of the five in-sample Sharpes with the five out-of-sample Sharpes is `grid_is_oos_spearman` 0.8999999999999998. Nothing was selected from the grid. The 150 and 250 lengths were not gridded.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1× (5 bp)** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 1.232077565576937 | 1.1690665766777297 | **1.1060682655093068** | 0.9801236016236936 | 0.8542713313873612 |
| OOS Sharpe | 1.1281788033968327 | 1.0726727686558437 | **1.0171774262432778** | 0.9062260277206317 | 0.795338989608814 |
| Full-sample return | 1.750082521311855 | 1.5983951421042835 | **1.4550571919689181** | 1.1916201778776712 | 0.9563959099106201 |

Every row from 0× through 3× has a positive full-sample return. The script does not report a break-even cost between those points.

One extra session of delay, signal at close t filled at the open of t+2, has full Sharpe 1.0784231315223622, out-of-sample Sharpe 0.994477202104203, full return 1.3577098065840043, and out-of-sample return 0.6083284863657425.

The same-bar close fill is stored as `same_bar_close_upper_bound`, label `upper bound, not an acceptance input`. Its full Sharpe is 1.1702583814590046, out-of-sample Sharpe 0.9278265831796139, full return 1.588805313728061, and out-of-sample return 0.5886870857971336. The out-of-sample Sharpe of that upper bound is below the primary's 1.0171774262432778.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full return | Full profit factor |
|---|---:|---:|---:|---:|
| QQQ, 1 bp, 0-or-1 | 1.3348954798308705 | 1.0952340207222475 | 0.34339456792203116 | 3.1437243562236743 |
| SPY, 1 bp, 0-or-1 | −0.24029367971960497 | 0.5191617254208545 | 0.053650809434875546 | 1.5038055525753113 |

The acceptance actual is `QQQ=1.095, SPY=0.519`. Both out-of-sample Sharpes are positive. SPY's in-sample Sharpe is negative. These rows are single 0-or-1 positions. They are not the 101-name book.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

| QQQ move quintile | Sessions | Mean strategy net |
|---|---:|---:|
| 1 | 201 | −0.0015703183386970624 |
| 2 | 201 | −0.0005605787921833643 |
| 3 | 201 | 0.0013505485022686106 |
| 4 | 201 | 0.0034246920706478435 |
| 5 | 200 | 0.0023481594279441526 |

Quintiles 1 and 2 have negative means. Quintiles 3, 4, and 5 have positive means. Quintile 5's mean is below quintile 4's mean. Prediction 2 averages quintiles 4 and 5 against quintiles 1 and 2, and that comparison is the one that was scored.

Full-sample exits:

| Reason | Trades | Mean gross | Mean net | Profit factor |
|---|---:|---:|---:|---:|
| trend | 293 | −0.0051297740650633914 | −0.005271365623312923 | 0.019971414282405668 |
| time | 441 | 0.005833819942488897 | 0.005672764963479991 | 9.346380177422251 |

Out-of-sample exits:

| Reason | Trades | Mean gross | Mean net | Profit factor |
|---|---:|---:|---:|---:|
| trend | 205 | −0.005051576094933954 | −0.005164855161439896 | 0.019750455970503674 |
| time | 271 | 0.006022699257120457 | 0.0058858718176086604 | 13.034637739897178 |

The time-stop group carries the positive mean. The trend-break group has a profit factor below 1 on both samples. None of these breakdowns was used to filter the rule.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS edge | OOS Sharpe >= 0.50 and OOS profit factor >= 1.10 | Sharpe 1.017, profit factor 1.442 | ✅ |
| 2. Direction placebo | direction placebo p <= 0.05 | 0.005997001499250375 | ✅ |
| 3. IS plateau | IS Sharpe > 0.00 and at least 60% of IS grid cells have Sharpe > 0 | IS Sharpe 1.280, grid 1.000 | ✅ |
| 4. Cost 2× | full-sample total return > 0 at 2× cost | 1.1916201778776712 | ✅ |
| 5. Cross-market | OOS Sharpe > 0 on at least one cross-market instrument | QQQ=1.095, SPY=0.519 | ✅ |
| 6. OOS sample | at least 100 OOS trades | 476 | ✅ |

No line failed. The direction placebo is a sign flip of a long book. The timing placebo in §6 is the stricter check, and it is not one of these six lines.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

**(post hoc)** Trailing 126-session Sharpe over 879 windows. The minimum is −1.3556632258172787 on 2026-02-06. The maximum is 3.5106111845398438. The last window, ending 2026-10-02, is 2.88791470945826. Windows with a positive Sharpe: 704 of 879.

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | Prediction 1 is not consistent. Time-stop mean gross 0.005833819942488897. Trend-break mean gross −0.0051297740650633914. Median hold is 20.0, the time stop | Keep the locked exits. Do not drop the trend exit after seeing this table |
| Concentration in a few days | **(post hoc)** Removing the 10 largest daily nets leaves full return 0.3866737770955768 and full Sharpe 0.5022546091680096. Those 10 sessions' positive members are `best_10_share_of_positive_days` 0.10549033595872236 of the sum of positive days. The largest day is 2025-04-10 at 0.0904067708. 2025-04-08 is also in the ten | Judge the paper account over the out-of-sample length, 567 sessions. A negative 126-session window is not by itself a halt: the trailing Sharpe reached −1.3556632258172787 |
| Generalization | SPY in-sample Sharpe −0.24029367971960497. Timing placebo p 0.7245508982035929. Membership is the 2026-10-02 list | Paper-trade this list only. A point-in-time membership tape is a different study |
| Execution | Delay to t+2 leaves OOS Sharpe 0.994477202104203. At 3× cost, full return is 0.9563959099106201 and OOS Sharpe is 0.795338989608814. The same-bar upper bound's OOS Sharpe is 0.9278265831796139 | Market-on-open orders after the 16:00 signal. Log slippage against 5 bp per side |
| Short sample / regime coverage | OOS t-stat 1.525766139364917. Bootstrap full-sample Sharpe interval 0.21618591222994615 to 1.9654424127006789. The out-of-sample window was already read at the index level. Worst completed-trade net **(post hoc)** −0.05401778753721556 | The halt rule in §10 uses the stored maximum drawdown. Eighteen names are still open |

## 10. Deployment proposal (paper trading)

Paper trading, not live capital. One account. At each open the target is the set of these 101 names the previous close said to hold, equal weight, absolute weights summing to 1 when any name is held. Cash earns 0. No leverage, no shorts, no adds, no profit target, no ATR stop.

- **Signal.** After 16:00 ET, on the split-adjusted daily bar. SMA150 and SMA250 use the last 150 and 250 existing closes of that name. Enter only when the trend, the touch, and the bounce are all true and the name is flat.
- **Orders.** Market-on-open for the next session that has a bar. An exit ordered because the close is under SMA250 uses the next open. The time stop is the open 20 evaluated sessions after the entry session. If that session has no bar, wait for the next open. A missing price is not filled forward.
- **Early closes.** The open is still 09:30 ET.
- **Logging.** Record every fill in a terminal ledger. This study does not port the rule into `apps/terminal`.
- **Review.** After 567 evaluated sessions, the out-of-sample session count in `results.json`.
- **Halt.** Stop the paper test, and do not retune, if paper equity drawdown goes past the stored full-sample maximum drawdown of −0.1948181551727206, or if a replay of the same days disagrees with the logged fills on side, entry session, or exit session. A negative stretch the length of the 126-session rolling window is not by itself a halt. **(post hoc)** That trailing Sharpe was −1.3556632258172787 on 2026-02-06 inside a path that still met §8.

The 150 and 250 lengths and the 20-session hold stay fixed for the paper test. The ex-NVDA result stays a prediction check.

## 11. Post hoc (not part of the verdict)

`posthoc.json` is labelled `post hoc`. It holds the 879-point trailing 126-session Sharpe series, the rolling minimum, maximum, and last value, the 10 largest daily nets, the full-sample return and Sharpe with those 10 days set to 0, `best_10_share_of_positive_days`, the worst and best completed-trade nets (−0.05401778753721556 and 0.06894234539685741), and `grid_is_oos_spearman`. The ten largest days, in order, are 2025-04-10, 2026-07-31, 2022-12-13, 2026-02-09, 2025-04-08, 2026-03-10, 2026-08-05, 2025-03-03, 2026-04-01, and 2025-04-23. None of this changes §8.

### Ideas for a new study

A point-in-time QQQ constituent test needs a historical membership tape. This study did not have one, and the prices already used here cannot be the untouched sample for that test. The hold grid is reported above and was not used to pick a rule.

## 12. Reproduce

From the repo root. The published headlines are the single run logged at 2026-10-03T15:50:12+00:00. Running `backtest.py` again appends a new `RUNLOG.md` entry. On this store, with seed 20261003, the script is deterministic.

```bash
python research/qqq-holdings-ma-bounce/research/counts.py
```

```bash
python -m research.kit lock research/qqq-holdings-ma-bounce/research
```

```bash
python research/qqq-holdings-ma-bounce/research/backtest.py
```

```bash
python -m research.kit guard research/qqq-holdings-ma-bounce/research/verify.py
```

```bash
python research/qqq-holdings-ma-bounce/research/verify.py
```

```bash
python research/qqq-holdings-ma-bounce/research/posthoc.py
```

```bash
python research/qqq-holdings-ma-bounce/research/charts.py
```

```bash
python -m research.kit summary research/qqq-holdings-ma-bounce/research
```

`counts.py` writes bar counts only. The lock command writes `RULES.lock` and must not be rerun against an edited `RULES.md`. `backtest.py` writes `results.json`, `daily.csv`, `trades.csv`, the placebo `.npy` files, and one run-log entry. `verify.py` reprints the match count. `posthoc.py` writes `posthoc.json`. `charts.py` writes the SVGs under `report/figures/`. The summary command prints the table in §1.

`research/README.md` does not list this study. That index was left unchanged on instruction.

### References

- Faber, M. (2007). A Quantitative Approach to Tactical Asset Allocation. *Journal of Wealth Management*.
- Jegadeesh, N., and Titman, S. (1993). Returns to Buying Winners and Selling Losers. *Journal of Finance*.
