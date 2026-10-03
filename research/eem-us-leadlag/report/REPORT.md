# EEM US lead-lag: EEM, signal from SPY

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Rejected.** Failed 5 of 6 pre-registered tests. The minimum sample was met. |
| Instruments | EEM, open to close, flat overnight. Signal is the sign of the prior SPY close-to-close return. EFA is the same rule and does not replace EEM. |
| Data | Evaluation 2011-01-06 through 2026-10-01, read via `agent-data/mdq.py`. EEM and EFA also have a bar on 2026-10-02; it is not used. No bars on 2012-10-29, 2012-10-30, 2018-12-05, or 2025-01-09. |
| Rules | [`research/eem-us-leadlag/research/RULES.md`](../research/RULES.md), locked 2026-10-02T21:13:40+00:00, sha256 `fed01671ea16` |
| Code | [`research/eem-us-leadlag/research/`](../research/) · 2 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The locked rule buys EEM at the open and sells at the close when the prior SPY session rose, and shorts that same open-to-close when the prior SPY session fell. Out of sample, 2024-07-01 through 2026-10-01, it returned **−55.6%** after 1 bp per side (Sharpe **−2.005**, profit factor **0.689**, 566 trades, max drawdown **−60.2%**). Over the full evaluation, 2011-01-06 through 2026-10-01, it returned −75.7% (Sharpe −0.617, max drawdown −82.0%). In sample, through 2024-06-28, it returned −45.1% (Sharpe −0.300). Uncosted EEM open-to-close returned +59.5% out of sample (Sharpe 1.286). Uncosted SPY close-to-close returned +40.5% (Sharpe 0.985).

Percentages in the table below are the stored fractions rounded to one decimal. Sharpes are the stored values rounded to three decimals. Section 8 quotes the acceptance values as stored.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2011-01-06 → 2026-10-01 | → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,957 | 3,391 | 566 |
| Total return | −75.7% | −45.1% | **−55.6%** |
| CAGR | −8.6% | −4.4% | −30.4% |
| Annual volatility | 13.2% | 12.3% | 17.3% |
| Sharpe | −0.617 | −0.300 | **−2.005** |
| Max drawdown | −82.0% | −58.7% | **−60.2%** |
| Trades / profit factor | 3,946 / 0.897 | 3,380 / 0.950 | 566 / **0.689** |
| Avg net trade | −3.23 bp | −1.47 bp | −13.76 bp |
| *EEM open-to-close Sharpe (max DD)* | *0.335 (−23.2%)* | *0.116 (−23.2%)* | *1.286 (−17.3%)* |
| *SPY close-to-close Sharpe (max DD)* | *0.752 (−34.1%)* | *0.714 (−34.1%)* | *0.985 (−19.9%)* |

It failed 5 of the 6 acceptance tests written before the first run (§8). Line 6 holds (566 out-of-sample trades), so the status is Rejected, not Inconclusive.

**Why.**

1. **Out of sample there is no gross edge.** At 0 bp the OOS Sharpe is −1.713 and the OOS return is −50.3%. At 1 bp they are −2.005 and −55.6%. A lower cost does not turn the out-of-sample test positive. **(post hoc)** The average out-of-sample gross trade is −11.76 bp.
2. **In sample the gross trade is smaller than the round trip, and the net Sharpe is negative.** The zero-cost in-sample Sharpe is +0.107 and the zero-cost in-sample return is +7.9%. After the locked 2 bp per trade the in-sample Sharpe is −0.300 and the return is −45.1%. **(post hoc)** The average in-sample gross trade is +0.53 bp. None of the five in-sample deadzone cells has a positive Sharpe.
3. **The full-sample gross Sharpe does not clear a sign flip or a shuffled signal.** Actual gross Sharpe is −0.236. The direction-placebo null mean is +0.008 and p = 0.834. The timing-placebo p is 0.862. The block-bootstrap 95% interval of the full-sample net Sharpe is −1.132 to −0.115.
4. **The same rule loses on EFA.** EFA out-of-sample Sharpe is −1.667 and the out-of-sample return is −42.8%. The failure is not an EEM-only print.
5. **Both mechanism predictions fail.** The full-sample gross sum is +0.086 on the long trades and −0.573 on the short trades. The mean net trade in the top half of `|prior SPY return|` is −0.000685. In the bottom half it is +0.000038. The large-move half is the worse half.

**Recommendation.** Do not trade it. The deadzone grid, the long side, the short side, EFA, and the one-session-delayed fill stay closed. Holding EEM overnight was excluded before the run and is not a result of this study.

## 2. The strategy

### Rules

```
On each SPY session t from the third SPY bar through 2026-10-01:
    r = SPY close[t-1] / SPY close[t-2] - 1
    signal = sign(r), with sign(0) = 0
    if signal > 0: buy EEM at the open of t, sell at the close, weight +1
    if signal < 0: short EEM from the open to the close, weight -1
    if signal == 0: flat
    do not use SPY's close on t
    do not hold across the close

strategy_net = weight * (EEM close / EEM open - 1) - 0.0002
               on a traded session, else 0
```

- **Why the prior session, not the same day.** The signal is known at the prior SPY close. EEM's same-day close prints at the same time as SPY's and is not an input. The self-test includes a long trade on a session whose own SPY close is down.
- **Why open to close.** The request fixed the hold. An EEM dividend that lands in the overnight gap is outside the trade. No dividend is stored. The book is flat overnight, so that gap is not a P&L term either way.
- **Why 1 bp per side.** Protocol default for SPY-class liquidity. The request put EEM and EFA in that class. The round trip is 2 bp. It was not changed after the P&L.

### How it trades

| | |
|---|---|
| Sessions with a trade | 3,946 of 3,957 evaluation sessions |
| Flat sessions | 11, equal to the count of zero signals |
| Trades per year | 251.3 |
| Time in market | 0.997 of evaluation sessions during the cash session; 0 overnight |
| Holding time | median 6.5 hours, mean 6.49 hours |
| Long / short | 2,163 long (profit factor 0.943), 1,783 short (profit factor 0.852) |
| Win rate | 49.0% full sample. Average winner 57.4 bp, average loser −61.4 bp |
| Exit reason | `session_close` on all 3,946 trades |

The 11 flat sessions are prior SPY moves of exactly zero. Every evaluation date has an EEM bar. Out of sample the book trades all 566 sessions.

## 3. Hypothesis and predictions

Rapach, Strauss, and Zhou (2013) find that lagged US returns predict monthly returns in other industrialized markets, and they read that lag as slow diffusion of US news. Their test is not a one-session sign rule, not an ETF, and not an open-to-close hold. This study applies only the sign of the previous SPY session to EEM's next cash session. The other side, as locked, is a trader who marks EEM off its own prior close and does not finish adjusting to the US session until after the next open.

The cash session is a hard place for that story. EEM and SPY close together. The next overseas session, which is closer to the paper's object, can show up in EEM's overnight gap. This rule does not hold the gap. The out-of-sample SPY path was already known to be up, including the cash session. The short side is the check against a long bias. It is the side that lost.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Full-sample gross sum positive on long trades and on short trades | Long gross sum +0.0857 (2,163 trades). Short gross sum −0.5730 (1,783 trades). In sample both gross sums are positive (+0.1681 and +0.0101). Out of sample both are negative (−0.0824 and −0.5831). | Not consistent |
| Mean net trade higher when `\|prior SPY return\|` is in the top half than in the bottom half | Median absolute move 0.004938. Both halves have 1,973 trades. Top-half mean net −0.000685. Bottom-half mean net +0.000038. | Not consistent |

The acceptance table also failed. The failed predictions are not a separate rescue. They say the lead the rule was built to see is not in these trades: the short side's gross sum is negative, and the larger SPY moves were the worse trades.

## 4. Method

- **Data.** `data/market-data.sqlite` through `agent-data/mdq.py`, schema user_version 6. The pre-lock script `counts.py` wrote `counts.json`. EEM and EFA each have 3,960 daily bars, 2011-01-04 through 2026-10-02, no duplicate dates, no 1-minute bars, and an empty `corporate_action` table. SPY has 3,959 daily bars, 2011-01-04 through 2026-10-01, also with no corporate actions. Through 2026-10-01 the three date sets are the same. The only extra EEM and EFA session is 2026-10-02, and it is not an evaluation date. The three `needs_attention` sessions, 2012-10-29, 2012-10-30, and 2018-12-05, have no bars and are not inserted. 2025-01-09 is missing and is already a holiday in mdq. 2021-12-31 is the only partial daily row and is included; it has a bar. Prices are split-adjusted by default. There are no stored splits, so the adjustment changes nothing. Dividends are not stored and are not adjusted. The open-to-close trade would have excluded an overnight ex-date gap anyway. Stored SPY daily opens and closes from late 2024 can differ from the regular-hours minute print (`spy-rsi2-dip-buy`). This study uses the stored daily prints. EEM and EFA have no minute tape to compare. The out-of-sample SPY close-to-close return, +40.5%, and Sharpe, 0.985, are the same window already reported by `spy-overnight-premium`.
- **Pre-registration.** `RULES.md` fixed the sign rule, the open-to-close hold, the cost, the split, the deadzone grid, the predictions, and the acceptance lines before any EEM or EFA return was computed. The hash is `fed01671ea166b48e7c8b3d959843858a8e1392414c960fb1b80657858bb1d50`. `results.json` carries the same hash. The user asked for the study to be run and forbade a git commit, so `RULES.md` was not committed. Logged `git_head` is `577dd38c7b98928f8fe9a696aff046425df9dafa` with `dirty=yes`. Prior exposure is the SPY and QQQ studies named in `RULES.md`. No earlier study in this repo uses EEM or EFA. The out-of-sample window was not unseen: those studies already describe 2024-07-01 through 2026-10-01 as a rising US equity market, and `spy-overnight-premium` had already split SPY's stored daily path into the overnight gap and the cash session.
- **Fills and costs.** Enter at EEM's daily open, exit at EEM's daily close. One bp per side, charged only when the weight is nonzero. The one-session delay is a reported check. There is no same-close fill: the signal prints at the same time as the previous EEM close.
- **Returns.** Daily simple returns. A flat day counts as 0. There are 11 of them. Sharpe is the mean divided by the sample standard deviation (`n − 1`), times √252, with a zero risk-free rate. Each window compounds from 1 on its first session. CAGR uses a 252-session year. Max drawdown is the minimum of equity over its running peak, minus 1, with the peak starting at 1.
- **Verification.** `backtest.py` runs the synthetic cases before it opens the store: a long after an up SPY session on a day whose own SPY close is down, a short that loses because EEM rises, a zero SPY move with no cost, a negative signal with no EEM bar, the last session stamped 13:00 because 2024-07-03 is an early close, a bar past the sample end left unused, and a 0.15 deadzone that keeps only the short. The delayed weight on the session after the missing bar is the missing bar's signal. `verify.py` does not import `backtest.py`. It matched 3,946 of 3,946 trades on side, entry time and price, exit time and price, and gross return. Seed 20261104 drew 40 evaluation sessions; that sample had no mismatches. The 11 flat sessions match.
- **Runs.** Two store runs. The 21:18:09Z run is the initial backtest. The 21:21:54Z run is the verify replay. No bug fix, no second strategy run, and no change in the headline Sharpe or return. No deviation from the locked rule was found.

## 5. Results

![Growth of $1](figures/equity.svg)

Growth of $1 over the full sample, with the 2024-07-01 split marked. The strategy ends at 0.24. Uncosted EEM open-to-close ends at 1.74. Uncosted SPY close-to-close ends at 5.99. Past the dashed line the strategy's out-of-sample return is −55.6%.

![Drawdown](figures/drawdown.svg)

The strategy's full-sample max drawdown is −82.0%. Terminal equity is 0.24, so the path does not end back at a high-water mark. EEM open-to-close max drawdown is −23.2%. SPY close-to-close max drawdown is −34.1%.

| | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary, 1 bp** | −75.7% / −0.617 / −82.0% | −0.300 | −55.6% / −2.005 / −60.2% |
| EFA, same rule | −60.0% / −0.464 / −64.0% | −0.198 | −42.8% / −1.667 / −46.8% |
| *EEM open to close* | +74.5% / 0.335 / −23.2% | 0.116 | +59.5% / 1.286 / −17.3% |
| *SPY close to close* | +499.1% / 0.752 / −34.1% | 0.714 | +40.5% / 0.985 / −19.9% |

The t-stat of the mean daily strategy return is −2.445 full sample, −1.100 in sample, and −3.004 out of sample.

![Calendar-year return](figures/by_year.svg)

The strategy's calendar-year return is negative in 11 of 16 years. 2011 starts on 2011-01-06. 2026 ends on 2026-10-01 and has 188 sessions. Calendar 2024 straddles the split. 2025 and 2026 are entirely out of sample: the strategy returns −18.8% and −42.5% while EEM open-to-close returns +25.1% and +35.0%.

| Year | Strategy | Sharpe | Max DD | EEM open to close | SPY close to close |
|---|---:|---:|---:|---:|---:|
| 2011 | −2.3% | −0.03 | −16.0% | −14.5% | −1.7% |
| 2012 | +18.0% | 1.44 | −6.6% | +13.3% | +13.5% |
| 2013 | +2.7% | 0.28 | −12.6% | −7.8% | +29.7% |
| 2014 | −13.0% | −1.26 | −16.3% | −9.3% | +11.3% |
| 2015 | +12.8% | 1.01 | −8.9% | +2.1% | −0.8% |
| 2016 | −21.7% | −1.99 | −24.8% | +11.7% | +9.6% |
| 2017 | −6.6% | −0.93 | −12.3% | +5.4% | +19.4% |
| 2018 | +24.4% | 1.56 | −11.5% | −13.9% | −6.3% |
| 2019 | −4.2% | −0.43 | −12.7% | +11.6% | +28.8% |
| 2020 | −38.1% | −3.26 | −39.3% | −3.6% | +16.2% |
| 2021 | −8.8% | −0.92 | −11.5% | +3.3% | +27.0% |
| 2022 | +1.8% | 0.19 | −17.2% | −2.2% | −19.5% |
| 2023 | −7.2% | −0.79 | −11.7% | +17.1% | +24.3% |
| 2024 | −3.5% | −0.36 | −12.9% | −3.0% | +23.4% |
| 2025 | −18.8% | −1.31 | −24.6% | +25.1% | +16.4% |
| 2026 | −42.5% | −3.15 | −47.6% | +35.0% | +12.0% |

Year returns, Sharpes, and drawdowns are the stored values rounded for display. The positive strategy years are 2012, 2013, 2015, 2018, and 2022.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trade's session and multiplies that trade's gross simple return by an independent ±1. Flat days stay 0. Two thousand draws, seed 20261101. Costs are not in this comparison. Actual gross Sharpe −0.236. Null mean +0.008. Null 95th percentile +0.418. Of 2,000 draws, 1,667 are at least as high as the actual. p = (1 + 1667) / 2001 = 0.834. The actual gross Sharpe sits below the null mean. The line was p ≤ 0.05.

The timing placebo is not an acceptance line. Five hundred draws, seed 20261103, permute the signal across the 3,957 evaluation sessions and keep the 2,163 positive, 1,783 negative, and 11 zero signals. Actual gross Sharpe is the same −0.236. Null mean +0.026. Null 95th percentile +0.413. p = 0.862.

### Bootstrap

Circular 20-session blocks of the full-sample net daily returns, 2,000 draws, seed 20261102. The 2.5th and 97.5th percentiles of Sharpe are −1.132 and −0.115. The median is −0.612. The fraction of draws with Sharpe ≤ 0 is 0.9865. The interval does not include a positive Sharpe. The OOS t-stat of the mean daily return is −3.004.

### Parameter plateau

![Parameter grid](figures/grid.svg)

The deadzone is the absolute prior SPY return below which the book is flat. The cells are 0, 0.0025, 0.005, 0.01, and 0.015. The primary is 0. In sample, 0 of 5 Sharpes are positive. Nothing was selected.

| Deadzone | IS Sharpe | OOS Sharpe | IS return | OOS return | IS trades | OOS trades |
|---|---:|---:|---:|---:|---:|---:|
| **0** | **−0.300** | **−2.005** | **−45.1%** | **−55.6%** | **3,380** | **566** |
| 0.0025 | −0.493 | −1.922 | −54.6% | −50.1% | 2,356 | 402 |
| 0.005 | −0.522 | −2.188 | −51.5% | −48.8% | 1,673 | 282 |
| 0.01 | −0.291 | −1.230 | −28.0% | −24.5% | 814 | 114 |
| 0.015 | −0.362 | −1.420 | −26.4% | −23.2% | 387 | 54 |

**(post hoc)** The Spearman rank correlation of the five in-sample Sharpes with the five out-of-sample Sharpes is 0.7. The least negative in-sample cell is deadzone 0.01, at −0.291. Its out-of-sample Sharpe is −1.230. That cell was found on this grid. It is not the primary, and it is not a candidate. Every out-of-sample cell is below zero.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.236 | −0.426 | **−0.617** | −0.999 | −1.380 |
| OOS Sharpe | −1.713 | −1.859 | **−2.005** | −2.296 | −2.588 |
| Full-sample return | −46.4% | −63.9% | **−75.7%** | −88.9% | −95.0% |

There is no break-even cost inside this sweep. Out of sample the Sharpe is −1.713 at 0 bp. The full-sample return is −46.4% at 0 bp, which is why line 4 fails at 2 bp per side as well.

Filling one evaluation session later, with the same 1 bp per side, gives a full-sample Sharpe of −0.570 and a full-sample return of −73.1% (3,945 trades). Out of sample that delayed book has Sharpe +0.226 and return +5.6% (566 trades). The full sample is still a loss. The delayed fill is not the primary, it is not an acceptance line, and the locked list does not allow it to replace the primary. There is no same-bar-close upper bound. The signal is not known in time to trade EEM's previous close.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|
| EEM (primary) | −0.300 | −2.005 | −75.7% / 0.897 |
| EFA | −0.198 | −1.667 | −60.0% / 0.919 |

EFA out of sample: return −42.8%, profit factor 0.727, 566 trades, max drawdown −46.8%, t-stat −2.498. Line 5 required an EFA out-of-sample Sharpe above 0.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Quintiles sort the 3,946 primary trades by the signed prior SPY return. Quintile 1 is the most negative fifth. The figure's bars are those mean net trades in basis points. The stored means are below. None of this was used as a filter.

| Quintile | Trades | Mean prior SPY return | Mean gross | Mean net |
|---|---:|---:|---:|---:|
| 1 | 790 | −0.01370 | −0.000851 | −0.001051 |
| 2 | 789 | −0.00282 | +0.000009 | −0.000191 |
| 3 | 789 | +0.00072 | +0.000221 | +0.000021 |
| 4 | 789 | +0.00458 | +0.000128 | −0.000072 |
| 5 | 789 | +0.01380 | −0.000123 | −0.000323 |

Quintile 1, the large SPY down days and therefore the shorts, has the worst mean net. Quintile 5, the large SPY up days, is also negative. The only positive mean net is quintile 3, at +0.000021.

| Side | Trades | Gross sum | Net sum | Mean net | Win rate | Profit factor |
|---|---:|---:|---:|---:|---:|---:|
| Long | 2,163 | +0.0857 | −0.3469 | −1.60 bp | 51.6% | 0.943 |
| Short | 1,783 | −0.5730 | −0.9296 | −5.21 bp | 45.8% | 0.852 |

Out of sample the long gross sum is −0.0824 (312 trades, profit factor 0.867) and the short gross sum is −0.5831 (254 trades, profit factor 0.550). The out-of-sample loss is in both sides, and the short side is the larger gross loss. Every trade exits at `session_close`. The long side is not a replacement rule. Its full-sample net sum is negative, and the rules do not allow it to be split off.

## 8. Acceptance tests (fixed before the first run)

The locked status rule: if line 6 fails, the status is Inconclusive. If line 6 passes and any of lines 1 through 5 fails, the status is Rejected. If all six pass, the status is Paper-trading candidate.

| Criterion | Required | Result | |
|---|---|---|---|
| 1 OOS Sharpe and profit factor | ≥ 0.5 and ≥ 1.10 | −2.004699564508105 and 0.6887471391885135 | ❌ |
| 2 Direction placebo p, full-sample gross Sharpe | ≤ 0.05 | 0.8335832083958021 | ❌ |
| 3 IS Sharpe, and IS grid cells with Sharpe > 0 | > 0 and ≥ 3 of 5 | −0.2999522212293542 and 0 of 5 | ❌ |
| 4 Full-sample total return at 2 bp per side | > 0 | −0.8894993141561425 | ❌ |
| 5 EFA OOS Sharpe | > 0 | −1.6668676120068147 | ❌ |
| 6 OOS trades | ≥ 100 | 566 | ✅ |

Five lines failed and one passed. The out-of-sample sample is 566 trades, so the negative point estimate is not an inconclusive sample. A direction placebo on a negative gross Sharpe is not a low bar that the book somehow cleared. The actual gross Sharpe is below the null mean.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

Trailing 252-session Sharpe **(post hoc)** is positive in 40.9% of 3,706 windows. The range is −3.503 to +2.010. The window ending 2026-10-01 is −2.209.

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | IS net Sharpe −0.300, OOS net Sharpe −2.005. OOS gross Sharpe −1.713. OOS profit factor 0.689. | None in this study. A later window needs its own `RULES.md`. |
| Concentration in a few days | **(post hoc)** Zeroing the 20 best days changes the full-sample return from −75.7% to −87.8% and the Sharpe from −0.617 to −1.007. Those 20 days are 6.3% of the sum of positive daily returns. **(post hoc)** Zeroing the 20 worst days leaves a full-sample return of −47.5% and a Sharpe of −0.268. **(post hoc)** Zeroing the 10 best OOS days leaves an OOS return of −67.2% and a Sharpe of −3.062. | Shown, not removed. The loss is still there without the worst 20 days. |
| Generalization | EFA OOS Sharpe −1.667. **(post hoc)** Full-sample correlation of the strategy with EEM open-to-close is −0.078, and with SPY close-to-close is −0.064. Out of sample those correlations are −0.179 and −0.155. | EFA was the pre-registered second market. It failed. |
| Execution | OOS Sharpe −1.713 at 0 bp. One session of extra delay: full-sample Sharpe −0.570, OOS Sharpe +0.226. The full-sample delayed book still returns −73.1%. No dividends are stored. The trade does not hold the overnight gap. | Cost stays at 1 bp. The delayed path is not adopted. |
| Sample and regime | 566 OOS trades, t-stat −3.004. The bootstrap interval on the full-sample net Sharpe is entirely below zero. The OOS window was already known to be a rising US equity market. EEM open-to-close rose +59.5% in that window while this rule fell −55.6%. | The floor of 100 OOS trades was met and was not lowered. The rising-market exposure was written down before the lock. |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A deadzone, a single side, EFA as the primary, a one-session delay, an overnight hold, or a sample that starts after 2026-10-01 is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

- **(post hoc)** Rolling 252-session Sharpe: n = 3,706, minimum −3.503, maximum +2.010, last −2.209 on 2026-10-01, fraction positive 0.409.
- **(post hoc)** Zeroing the 20 best `strategy_net` days: total return −0.878, Sharpe −1.007. Sum of those days 0.704. Share of the sum of positive days 0.063.
- **(post hoc)** Zeroing the 20 worst days: total return −0.475, Sharpe −0.268. Sum of those days −0.753.
- **(post hoc)** Zeroing the 10 best OOS days: OOS return −0.672, Sharpe −3.062.
- **(post hoc)** Correlation of `strategy_net` with `eem_otc` and `spy_c2c`: full sample −0.078 and −0.064; out of sample −0.179 and −0.155.
- **(post hoc)** Average gross trade: −1.23 bp full sample, +0.53 bp in sample, −11.76 bp out of sample.
- **(post hoc)** Spearman rank correlation of the five in-sample grid Sharpes with the five out-of-sample grid Sharpes: 0.7.

None of these figures enters §8.

### Ideas for a new study

Holding EEM from the close to the next open, which is where a slow overseas reaction would have to show up in this ETF, was named and excluded before any return was computed. This file does not measure that gap. A test of it needs its own rules and a window that starts after 2026-10-01. The one-session delay was already run on this sample. Its full-sample return is −73.1%. It is not a candidate and it should not be refit here.

## 12. Reproduce

From the repo root:

```bash
python research/eem-us-leadlag/research/backtest.py
```

```bash
python research/eem-us-leadlag/research/verify.py
```

```bash
python research/eem-us-leadlag/research/posthoc.py
```

```bash
python research/eem-us-leadlag/research/charts.py
```

`backtest.py` checks the rules hash, runs the synthetic cases, then writes `results.json`, `daily.csv`, `trades.csv`, `placebo_direction.npy`, `placebo_timing.npy`, and appends `RUNLOG.md`. `verify.py` replays the trades and appends `RUNLOG.md`. `posthoc.py` writes `posthoc.json` and `rolling_sharpe.csv` and does not open the store. `charts.py` writes the eight SVGs in `report/figures/` from those files. Seeds: direction 20261101, bootstrap 20261102, timing 20261103, verify 20261104. A later run on the same store, with the same code, is expected to match the headlines. Nothing was written to `data/`. Ingest was not run.

Phase 6 of the protocol, as run:

- `RULES.md` matches `RULES.lock`, and `results.json` carries the same hash.
- The only strategy run is the initial run. The verify replay matched. Both reasons are permitted.
- The self-test is required before the store open, and the completed run passed it. `verify.py` matched every trade.
- The status is Rejected, which is what the locked table requires when line 6 passes and any earlier line fails.
- Every post-hoc number above is labelled, and none is in the acceptance table.
- Prior exposure and the excluded sessions are in §4.
- The reproduce commands are above. Seeds are recorded.
- This folder's `README.md` has the status, the links, and the commands. `research/README.md` was not updated. The invoking request forbade edits outside `research/eem-us-leadlag/` and forbade a git commit.
- Nothing was written to `data/`, and ingest was not run.

### References

- Rapach, D. E., Strauss, J. K., and Zhou, G. (2013). International Stock Return Predictability: What Is the Role of the United States? *Journal of Finance*, 68(4), 1633–1662.
