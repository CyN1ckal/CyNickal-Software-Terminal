# FX ETF time-series momentum: FXE, FXB, FXA, FXC, FXF, FXY

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Rejected.** Failed 4 of 5 scored tests. Line 6 passed, so the status is not Inconclusive. Line 5 was not scored. |
| Instruments | FXE, FXB, FXA, FXC, FXF, FXY. Monthly signal, next-open fill, weight ±1/6, hold until the signal flips, goes to zero, or the sample ends. |
| Data | Daily bars 2011-01-04 → 2026-10-01, read via `agent-data/mdq.py`. Evaluation 2011-05-02 → 2026-10-01. No bars on 2012-10-29, 2012-10-30, or 2018-12-05. No dividends stored. |
| Rules | [`research/fx-etf-momentum/research/RULES.md`](../research/RULES.md), locked 2026-10-02T19:54:13Z, sha256 `3277722ca193` |
| Code | [`research/fx-etf-momentum/research/`](../research/) · 2 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The rule is 63-session time-series momentum on six CurrencyShares ETFs, rebalanced at each month-end and filled at the next open, at 5 bp per side. Out of sample, 2024-07-01 through 2026-10-01, it returned +6.36% (Sharpe 0.4908, max drawdown −4.09%, 40 round trips, profit factor 3.026, t-stat 0.736). The full sample, 2011-05-02 through 2026-10-01, returned −11.65% (Sharpe −0.1240, max drawdown −21.60%). In sample the return was −16.93% (Sharpe −0.2439).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2011-05-02 → 2026-10-01 | 2011-05-02 → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,878 | 3,312 | 566 |
| Total return | −11.65% | −16.93% | +6.36% |
| CAGR | −0.80% | −1.40% | +2.78% |
| Annual volatility | 5.34% | 5.23% | 5.96% |
| Sharpe | −0.1240 | −0.2439 | 0.4908 |
| Max drawdown | −21.60% | −21.60% | −4.09% |
| Trades / profit factor | 311 / 0.843 | 271 / 0.741 | 40 / 3.026 |
| Avg net trade | −21.0 bp | −42.0 bp | +121.1 bp |
| *Benchmark Sharpe (max DD)* | −0.323 (−37.31%) | −0.422 (−37.31%) | 0.217 (−8.85%) |

The benchmark is an uncosted equal-weight long basket of the same six ETFs. Its total return was −30.52% full sample, −32.49% in sample, and +2.91% out of sample.

It failed 4 of the 5 scored acceptance tests written before the first run (§8).

**Why.**

1. There is no gross edge on the full sample. The separate zero-cost resimulation returned −6.85% (Sharpe −0.0597). At the pre-registered 5 bp the full sample returned −11.65%. At 10 bp it returned −16.20%. Costs make the loss larger. They do not create it.
2. In sample the Sharpe is −0.2439, and 0 of 5 lookback cells have an in-sample Sharpe above 0.
3. The direction placebo p-value is 0.602. The actual gross Sharpe is −0.0596.
4. Out-of-sample Sharpe is 0.4908, below the required 0.5. Out-of-sample profit factor is 3.026, which clears 1.10. The line requires both. The out-of-sample t-stat is 0.736. The block-bootstrap 95% interval of the full-sample net Sharpe is −0.579 to +0.337.

The long basket lost more than the strategy over the full sample because that basket is always long foreign currency. The strategy still lost. That comparison is not an acceptance line.

**Recommendation.** Do not trade it. The locked rules do not allow a substitute: not the short side, not FXY or FXE, not the 42-session cell, not the out-of-sample window, not a dividend repair, and not a dropped currency.

## 2. The strategy

### Rules

```
signal dates = last NYSE session of each month that has a SPY daily bar
               (2012-10-29, 2012-10-30, and 2018-12-05 are not sessions)
signal = sign(close[t] / close[t-63] - 1) on that ETF's own daily bars
sign(0) = 0; a missing bar on t sets the signal to 0 and is not forward-filled
weight = signal / 6          # denominator stays 6 when a name is flat
fill at the next session's open
old shares earn the gap into the open
new shares = weight * equity_open / open, sized before cost
cost = 5 bp * absolute notional traded
new shares earn open to close
a missing bar earns 0 that day
exit when the weight flips, goes to 0, or the sample ends
sample-end mark is the 2026-10-01 close, no exit cost
```

- **Why 63 sessions:** the short horizon in the FX momentum literature, fixed before any return on these series. The primary was not lengthened to 12 months.
- **Why the denominator stays 6:** a flat name is a zero weight, not a reason to lever the other five.
- **Why 5 bp:** CurrencyShares are thinner than SPY, and the store has no quotes. The rate was not changed after the P&L.

### How it trades

| | |
|---|---|
| Fill dates | 186 month-end signals, first fill 2011-05-02, last signal 2026-09-30 (filled 2026-10-01) |
| Position | In the market on all 3,878 evaluation sessions (time in market 1.0). Mean gross exposure 0.998. Mean net exposure −0.091. |
| Trades per year | 20.2 full sample; 17.8 out of sample |
| Holding time | median 63 sessions / mean 74.6 |
| Long / short | 158 long, profit factor 0.505, net dollars −0.216; 153 short, profit factor 1.324, net dollars +0.099 |
| Win rate | 32.8% full sample. Average winner +405 bp, average loser −229 bp. |

Three of 186 eligible dates produced an exact-zero signal, on three different ETFs. The book flips. It rarely goes flat. The short-side profit factor is a breakdown (§7). It is not a new rule.

## 3. Hypothesis and predictions

A currency ETF with a positive 63-session price return was predicted to continue over the next month, and one with a negative return was predicted to continue down. Long the ETF is long the foreign currency. The mechanism is time-series momentum (Moskowitz, Ooi, and Pedersen 2012) at the shorter horizon where FX momentum has been documented (Menkhoff, Sarno, Schmeling, and Schrimpf 2012). The other side is a hedger or a carry trader leaning against the spot move.

This is a contaminated spot-price test. No dividends are stored. CurrencyShares distribute foreign interest, so an ex-distribution drop sits in the price and looks like a spot decline. The study did not repair that. The result is not a carry test and not a total-return test.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Full-sample gross dollar P&L is positive in at least 4 of the 6 ETFs | Positive in FXE (+0.0275) and FXY (+0.1014). FXB, FXA, FXC, and FXF are negative. Two of six. | Not consistent |
| The 126-session cell does not have the highest in-sample Sharpe of the five cells | 126-session IS Sharpe is −0.250. The highest of the other four is the 42-session cell at −0.067. | Consistent |

Prediction 2 is a ranking among five negative in-sample Sharpes. The 42-session cell, the least negative in sample, has an out-of-sample Sharpe of −0.696. The primary stayed at 63. Neither prediction changes the verdict.

| ETF | Gross dollars | Net dollars | Trades | Profit factor | Avg net bp |
|---|---:|---:|---:|---:|---:|
| FXE | +0.0275 | +0.0201 | 48 | 1.180 | +29.3 |
| FXB | −0.0058 | −0.0132 | 48 | 0.872 | −17.7 |
| FXA | −0.1244 | −0.1342 | 63 | 0.400 | −131.1 |
| FXC | −0.0515 | −0.0602 | 56 | 0.536 | −66.9 |
| FXF | −0.0154 | −0.0234 | 52 | 0.780 | −26.1 |
| FXY | +0.1014 | +0.0945 | 44 | 2.426 | +142.3 |

The full-sample gross-dollar P&L on the costed share path is −0.0682. FXA is the large loser and FXY the large winner. Neither name is dropped, and FXY is not promoted.

## 4. Method

- **Data.** Stored daily bars through `mdq.bars`, default split adjustment. Each of the six, and SPY, has 3,959 bars from 2011-01-04 through 2026-10-01, identical dates, no bad OHLC, no stored corporate action, and no adjusted-versus-raw difference. Coverage recorded 4,108 rows: 4,103 complete, 4 missing, 1 partial. The partial session is 2021-12-31 and stays in the book. One missing row is 2025-01-09, which the calendar already treats as closed. The other three missing rows are 2012-10-29, 2012-10-30, and 2018-12-05. `nyse_sessions` includes those three dates; the book is SPY's bars, so they are skipped. 2026-10-01 is the first stored session of October. It is not a signal date. It is the fill of the 2026-09-30 signal and the last evaluation day. Dividends are not stored and were not imputed.
- **Pre-registration.** `RULES.md` fixed the universe, the 63-session lookback, the ±1/6 weights, the month-end calendar, the 5 bp cost, the 2024-07-01 split, both predictions, the grid, and lines 1–4 and 6 before any return. The hash in `RULES.lock` matches `results.json`. The lock time is 2026-10-02T19:54:13Z. The store run is 2026-10-02T20:02:27Z. `RULES.md` was not committed: the request forbade a git commit. Git HEAD at the run was `8068750d3850669df5441a076ec108499b4acd44`, dirty. Prior exposure: no earlier study read these six ETFs. `micro-futures-trend` excluded CME FX futures 6E, 6J, 6B, 6A, 6C, and 6S before any return, because the vendor rounds those prices to 2 decimals (`6E=F` had 26 distinct closes in 1,260 bars; `6J=F` had 2). That study's non-FX book, out of sample 2024-10-01 → 2026-09-25, returned −7.8% (Sharpe −0.19, profit factor 0.90, 81 trades). Full sample −21.5% (Sharpe −0.33, max drawdown −33.6%). Its 2024, 2025, and 2026-through-9/25 calendar returns were −10.7%, +3.4%, and +1.1%. Those months are not unseen, and they are not an FX ETF path. Equity studies on a similar calendar had QQQ and SPY up, including the April 2025 crash and rebound. The 63-session rule was not changed after reading that report. This study did not open the futures files.
- **Fills and costs.** The signal uses the close, which is known at the session close. The fill is the next session's open. Cost is 5 bp of notional traded per side, on every name. The store has no quotes, so the 5 bp figure was not checked against a spread.
- **Returns.** Daily simple returns on the SPY book. A flat day inside a window would count as 0; this book was not flat. Sharpe is the mean divided by the sample standard deviation (`ddof=1`), times √252, with a zero risk-free rate. A round trip is attributed entirely to the window of its entry fill. Daily Sharpe, return, CAGR, volatility, drawdown, and the t-stat use only the daily returns inside the window. The out-of-sample return is growth of $1 across that window. It is not the right edge of the continuous equity path.
- **Verification.** `backtest.py` runs the synthetic self-test before it opens the store: positive signal, negative signal, exact zero, a flat name that stays at 1/6, a missing bar that is not forward-filled, a same-sign resize that stays one trip, a flip with the cost split, a sample-end mark with no exit cost, and sizing off mark-to-open equity before cost. The store run completed, so those cases passed. `verify.py` shares no signal code with `backtest.py`. It matched all 311 trips on symbol, side, entry date and price, exit date and price, and exit reason, and all 3,878 daily `strategy_net` values (tolerance 1e-10). A seeded sample of 40 eligible signal dates (seed 20261034) covered 71 of those trips, which also matched.
- **Runs.** Two executions read the store and computed returns. The implementation run is 2026-10-02T20:02:27Z. The verify replay is 2026-10-02T20:06:00Z. The headlines agree (full Sharpe −0.1240, full return −11.65%, out-of-sample Sharpe 0.4908, out-of-sample return +6.36%). No bug fix changed a headline. `results.json` and the first run-log line record `reason: end`. That string is the last label of the exit-reason loop (`flat`, `flip`, `end`), which reuses the variable `reason` after `run_reason()` has returned `initial pre-registered run`. It is a log label from the one implementation run, not a second test and not an exit filter. The code was left as it ran. `counts.py` ran before the lock and did not compute a return. `posthoc.py` and `charts.py` read the output files and do not open the store.

## 5. Results

![Growth of $1](figures/equity.svg)

The continuous path, strategy and the uncosted long basket, ends at 0.884 against the basket's 0.695. The dashed line is 2024-07-01. The out-of-sample +6.36% is a fresh $1 inside that window (out-of-sample ending equity 1.064). It does not lift the full path back to 1.

![Drawdown](figures/drawdown.svg)

| Strategy (5 bp) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | −11.65% / −0.1240 / −21.60% | −0.2439 | +6.36% / 0.4908 / −4.09% |
| *Equal-weight long basket, uncosted* | −30.52% / −0.323 / −37.31% | −0.422 | +2.91% / 0.217 / −8.85% |

The drawdown figure is the continuous path, with the peak started at 1. The out-of-sample max drawdown of −4.09% restarts inside that window and is the table figure, not the trough of the chart.

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | Benchmark | Sessions |
|---|---:|---:|---:|---:|---:|
| 2011 | −1.16% | −0.193 | −7.41% | −5.90% | 170 |
| 2012 | −5.51% | −1.133 | −8.87% | +0.15% | 250 |
| 2013 | +2.40% | 0.527 | −3.29% | −5.35% | 252 |
| 2014 | +5.06% | 1.140 | −2.78% | −9.78% | 252 |
| 2015 | −3.53% | −0.570 | −8.22% | −7.54% | 252 |
| 2016 | +3.21% | 0.609 | −3.92% | −3.16% | 252 |
| 2017 | −4.13% | −0.862 | −4.70% | +7.31% | 251 |
| 2018 | −3.19% | −0.680 | −4.20% | −4.68% | 251 |
| 2019 | −5.07% | −1.546 | −6.54% | +1.14% | 252 |
| 2020 | −2.78% | −0.548 | −6.10% | +5.91% | 253 |
| 2021 | −3.70% | −0.925 | −3.92% | −5.04% | 252 |
| 2022 | +4.92% | 0.660 | −7.30% | −7.51% | 251 |
| 2023 | −1.41% | −0.274 | −5.14% | +2.23% | 250 |
| 2024 | −1.00% | −0.184 | −4.34% | −7.18% | 252 |
| 2025 | +3.51% | 0.513 | −3.58% | +7.90% | 250 |
| 2026 | +1.02% | 0.316 | −2.92% | −2.13% | 188 |

2011 starts on the first fill, 2011-05-02, and has 170 sessions. 2026 ends 2026-10-01 and has 188 sessions. 2024 contains both sides of the 2024-07-01 split, so its −1.00% is not an out-of-sample year. The pre-registered year rows are negative in 2011, 2012, 2015, 2017, 2018, 2019, 2020, 2021, 2023, and 2024.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trip's dates and multiplies that trip's gross dollar pieces by an independent ±1. Two thousand draws, seed 20261031. Gross return uses the actual prior net equity as the denominator. p = (1 + number of draws with gross Sharpe ≥ actual) / 2001. The actual gross Sharpe is −0.0596. The null mean is −0.0040 and the null 95th percentile is 0.336. p = 0.602. The actual result sits inside the sign-flip distribution, on the negative side. The placebo does not say the rule has a negative edge that is itself unusual. It says the gross path is what a coin flip of these trip dates produces.

The timing placebo is not an acceptance line. Five hundred draws, seed 20261033, permute each ETF's own signals across rebalance dates and preserve its count of positive, negative, and zero signals. The book is rebuilt at zero cost. p = 0.741. The actual zero-cost Sharpe is −0.0597. The null mean is +0.082 and the null 95th percentile is 0.433. Shuffling the signals did not do worse than the real timing.

### Bootstrap

Circular 20-session blocks, 2,000 draws, seed 20261032, on the full-sample net daily returns. The 2.5th and 97.5th percentiles of Sharpe are −0.579 and +0.337. The share of draws with Sharpe ≤ 0 is 0.7025. The out-of-sample t-stat of the mean daily return is 0.736.

### Parameter plateau

![Parameter grid](figures/grid.svg)

| Lookback | IS Sharpe | IS return | OOS Sharpe | OOS return |
|---:|---:|---:|---:|---:|
| 21 | −0.332 | −22.38% | −0.110 | −1.98% |
| 42 | −0.067 | −6.34% | −0.696 | −9.77% |
| **63** | **−0.244** | **−16.93%** | **0.491** | **+6.36%** |
| 84 | −0.303 | −21.05% | 0.067 | +0.49% |
| 126 | −0.250 | −17.21% | −1.403 | −17.62% |

Zero of five in-sample Sharpes are positive. The rank correlation of the five in-sample Sharpes with the five out-of-sample Sharpes is −0.10. The best in-sample cell is 42 sessions. Its out-of-sample Sharpe is −0.696. The primary, 63, is the highest out-of-sample cell. Nothing was selected from the grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 2.5 bp | **5 bp** | 10 bp | 15 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.0597 | −0.0919 | **−0.1240** | −0.1881 | −0.2520 |
| OOS Sharpe | 0.5415 | 0.5162 | **0.4908** | 0.4398 | 0.3886 |
| Full-sample return | −6.85% | −9.28% | **−11.65%** | −16.20% | −20.52% |

Out-of-sample return at those same costs is +7.09%, +6.73%, +6.36%, +5.64%, and +4.92%. The full sample is negative at zero cost, so the sweep has no cost at which the full sample turns positive.

Filling one session later (two sessions after the signal) gives a full-sample Sharpe of −0.105 and a full-sample return of −10.21%. Out-of-sample Sharpe is 0.426 and out-of-sample return is +5.47%. Cost booked before the window is 0. The labelled same-bar close fill, an upper bound and not a verdict input, gives a full-sample Sharpe of −0.119 and a full-sample return of −11.35% (out-of-sample Sharpe 0.390, out-of-sample return +4.95%). Cost booked before that window is 0.0005. The upper bound does not make the full sample positive.

### Other markets (identical rules)

Not applicable. No second CurrencyShares set is in the store. CME FX futures were already judged unusable for price precision, and this study did not read those files. Line 5 was recorded as not applicable before the lock. It is not a failure and not a pass.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Mean strategy return by quintile of the long basket's daily return. Bins are equal count as far as the length allows (776, 776, 776, 775, 775).

| Quintile | Basket mean return | Strategy mean return |
|---|---:|---:|
| Q1 (worst basket day) | −0.005702 | +0.000663 |
| Q2 | −0.001989 | +0.000226 |
| Q3 | −0.000148 | +0.000024 |
| Q4 | +0.001771 | −0.000206 |
| Q5 (best basket day) | +0.005651 | −0.000839 |

The strategy's average day is positive when the long basket is down and negative when the long basket is up. The same cut on SPY's close-to-close return is small: Q1 +0.000294, Q2 −0.000018, Q3 +0.000040, Q4 +0.000167, Q5 −0.000616. Neither quintile was used as a filter.

Full-sample trips by side: longs made −0.216 net dollars (gross −0.192, profit factor 0.505, win rate 27.8%, average net −84.5 bp, median hold 61 sessions). Shorts made +0.099 net dollars (gross +0.123, profit factor 1.324, win rate 37.9%, average net +44.5 bp, median hold 64 sessions). The short side was found on this data. It is not promoted.

By exit reason: 302 flips, net dollars −0.130, profit factor 0.821; 3 flats, net dollars +0.0076; 6 sample-end marks, one per ETF still open on 2026-10-01, net dollars +0.0063, profit factor 1.751, win rate 0.5. None of these cuts was used to change the rule.

Eligible-date signal counts (186 dates): FXE 84 / 102 / 0, FXB 88 / 97 / 1, FXA 90 / 95 / 1, FXC 78 / 108 / 0, FXF 94 / 92 / 0, FXY 73 / 112 / 1 (positive / negative / zero).

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. Out-of-sample Sharpe and profit factor | Sharpe ≥ 0.5 and profit factor ≥ 1.10 | Sharpe 0.4908, profit factor 3.026 | ❌ |
| 2. Direction placebo | p ≤ 0.05 | p = 0.602 | ❌ |
| 3. In-sample plateau | IS Sharpe > 0 and at least 3 of 5 IS grid Sharpes > 0 | IS Sharpe −0.2439, 0 of 5 | ❌ |
| 4. Double cost | Full-sample total return > 0 at 10 bp | −16.20% | ❌ |
| 5. Cross-market | Not applicable. No second FX ETF set. FX futures were not read. | Not scored | — |
| 6. Minimum sample | At least 24 OOS round trips with entry on or after 2024-07-01 | 40 | ✅ |

Line 6 held, and lines 1 through 4 failed, so the status is **Rejected**. A failed line 6 would have made it Inconclusive even with the other failures. Line 5 is not a failure. The profit-factor half of line 1 cleared 1.10. The Sharpe half did not clear 0.5. The line fails. The direction placebo is on gross Sharpe, so costs are not what the coin flip has to beat.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| No full-sample edge | Zero-cost full-sample return −6.85%, Sharpe −0.0597. Bootstrap interval of net Sharpe includes 0 and extends to −0.579. | Do not trade it. |
| In-sample failure at every lookback | IS Sharpe −0.2439. All five grid cells are negative in sample. The IS-best cell lost out of sample (Sharpe −0.696). | The primary stays 63. Nothing is selected. |
| Out-of-sample Sharpe below the line | 0.4908 versus 0.5, t-stat 0.736, 566 sessions, 40 trips. The window overlaps other studies' 2024–2026 results and is not unseen. | The count line passed. That does not move the Sharpe line. |
| Price series omit distributions | No dividends are stored. An ex-distribution drop looks like a spot decline. This is not a total-return test. | No repair was added after the result. |
| Execution | 5 bp was set because the store has no quotes. One extra session of delay leaves the full sample negative (Sharpe −0.105). | The cost was not lowered after the loss. |
| One name, one side | FXA gross dollars −0.124. Shorts +0.099 net dollars, longs −0.216. | Reported. Not used as a filter. |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A different rule, a short-only book, or a later sample is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

`posthoc.py` reads `daily.csv` and `results.json`. It does not open the store. None of the following enters §8.

**(post hoc)** Of the 16 calendar years in the pre-registered year table, 10 are negative and 6 are positive. 2011 and 2026 are partial.

**(post hoc)** Rolling 252-session Sharpe: 3,627 windows, minimum −2.279 on 2019-11-13, maximum +1.756 on 2015-03-13, 57.2% of windows at or below 0, last value +0.433 on 2026-10-01.

**(post hoc)** The last 252 sessions, 2025-10-01 through 2026-10-01, returned +1.82% (Sharpe 0.433). That is not the out-of-sample result.

**(post hoc)** Full-sample return is −11.65%. Replacing the best 10 daily returns with 0 leaves −23.81%. Replacing the worst 10 with 0 leaves +6.25%.

**(post hoc)** The worst session is 2015-01-15: strategy −2.75%, long basket +2.78%. The two prints have opposite signs and similar size. FXF was inside a short trip from 2014-06-02 (entry 108.95) to 2015-02-02 (exit 104.97). That row is one ETF. It is not the book's P&L for the day. The best session is 2025-04-10: strategy +2.21%, basket +2.21%. On 2025-04-03 both were up (strategy +1.88%). On 2025-04-04 both were down (strategy −1.78%). FXF's long trip in that month starts 2025-04-01. These days were not removed from the sample.

### Ideas for a new study

The short book's net dollars are positive on this sample and the long book's are not. That split was found here. A short-only rule would need its own `RULES.md` and data this study has not used. A total-return test would need CurrencyShares distributions, which this store does not have. This study does not repair dividends, drop FXA, or adopt the 42-session cell.

## 12. Reproduce

From the repo root:

```bash
python research/fx-etf-momentum/research/backtest.py
```

```bash
python research/fx-etf-momentum/research/verify.py
```

```bash
python research/fx-etf-momentum/research/posthoc.py
```

```bash
python research/fx-etf-momentum/research/charts.py
```

`backtest.py` writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`. It refuses to run when `results.json` already exists unless `--reason` is passed, and a rerun is allowed only for a rule the code does not implement, a crash or a bad read, or a verify mismatch. `verify.py` replays the store and appends a run-log entry. `posthoc.py` writes `posthoc.json`. `charts.py` writes the SVG files in `report/figures/`. Seeds: direction 20261031, bootstrap 20261032, timing 20261033, verify 20261034. The same store and the same seeds reproduce the same Sharpe and return figures. `20261032` is a seed, not a calendar date.

### Protocol checklist

- `RULES.md` hash `3277722ca193119b2d495e2fe04c3a1e0c29bd8a4d0bd3bd4caaa91ac1616191` matches `RULES.lock` and `results.json`. The file was not committed, because the request forbade the commit.
- Both run-log entries are permitted: the implementation run, and the independent verify replay. No headline moved between them.
- The self-test ran before the store open. `verify.py` matched every trip and every daily net return, including the 40-date sample.
- Status is **Rejected**, which is what lines 1–4 and 6 require.
- Every post-hoc number above is labelled, and none of them is an acceptance input.
- Prior exposure and the three no-bar closures are stated.
- Reproduce commands and seeds are stated. `research/fx-etf-momentum/README.md` has the status and the commands.
- Nothing was written to `data/`, and `ingest` was not run.

The index row in `research/README.md` was not added. The request forbade editing that file. That checklist item fails on purpose.

### References

- Moskowitz, T. J., Ooi, Y. H., & Pedersen, L. H. (2012). Time series momentum. *Journal of Financial Economics*, 104(2), 228–250.
- Menkhoff, L., Sarno, L., Schmeling, M., & Schrimpf, A. (2012). Currency momentum strategies. *Journal of Financial Economics*, 106(3), 660–684.
