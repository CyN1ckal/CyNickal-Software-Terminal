# SPY pre-holiday: SPY

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Rejected.** Failed 2 of 6 pre-registered tests. |
| Instruments | SPY daily bars. Long the cash session on the last session before a full-day NYSE closure. Flat overnight and flat on every other session. QQQ is the cross-market line. IWM is the same rule, reported only. |
| Data | Evaluation 2011-01-14 through 2026-10-01, read via `agent-data/mdq.py`. Bars begin 2011-01-04. Sessions with no daily bar are absent: 2012-10-29, 2012-10-30, 2018-12-05. |
| Rules | [`research/spy-pre-holiday/research/RULES.md`](../research/RULES.md), locked 2026-10-02T21:12:37+00:00, sha256 `04497efa4c9f` |
| Code | [`research/spy-pre-holiday/research/`](../research/) · 2 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The rule buys SPY at the daily open of the last session before a full-day NYSE closure and sells at that session's daily close. Out of sample, 2024-07-01 through 2026-10-01, it returned **+0.45%** after 1 bp per side (Sharpe **0.122**, profit factor **1.090**, 23 trades, max drawdown **−2.11%**, t-stat 0.183). The mean net trade in that window was **+2.08 bp**. Over the same 566 sessions, uncosted SPY open-to-close returned **+25.14%** (Sharpe 0.709, max drawdown −18.22%) and uncosted SPY close-to-close returned **+40.52%** (Sharpe 0.985, max drawdown −19.88%). The full sample, 2011-01-14 through 2026-10-01, returned +12.36% (Sharpe 0.411, max drawdown −3.25%, ending equity 1.124, 146 trades). In sample the return was +11.87% (Sharpe 0.456).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2011-01-14 → 2026-10-01 | → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,951 | 3,385 | 566 |
| Total return | +12.36% | +11.87% | **+0.45%** |
| CAGR | 0.75% | 0.84% | 0.20% |
| Annual volatility | 1.85% | 1.87% | 1.74% |
| Sharpe | 0.411 | 0.456 | **0.122** |
| Max drawdown | −3.25% | −3.25% | −2.11% |
| Trades / profit factor | 146 / 1.425 | 123 / 1.504 | 23 / **1.090** |
| Avg net trade | 8.17 bp | 9.31 bp | 2.08 bp |
| *Close-to-close Sharpe (max DD)* | *0.751 (−34.10%)* | *0.712 (−34.10%)* | *0.985 (−19.88%)* |
| *Open-to-close Sharpe (max DD)* | *0.486 (−24.59%)* | *0.442 (−24.59%)* | *0.709 (−18.22%)* |

It failed 2 of the 6 acceptance tests written before the first run (§8). Line 6 holds (23 out-of-sample trades; the floor is 15), so the status is Rejected.

**Why.**

1. **Out of sample the pre-holiday cash session is not a 0.5 Sharpe book, before or after cost.** The gross mean open-to-close on the 23 pre-holiday sessions is 4.08 bp. The gross mean on the other 543 out-of-sample sessions is 4.47 bp. Prediction 2 fails. At zero cost the out-of-sample Sharpe is 0.239 and the return is +0.91%. At the locked 2 bp round trip the Sharpe is 0.122, the profit factor is 1.090, and the mean net trade is 2.08 bp. The Sharpe bar is 0.5. The profit-factor bar is 1.10. Both halves of line 1 fail. The zeros on ordinary days are in the Sharpe because the book is flat then. They are not why the profit factor fails: the profit factor uses the 23 trades.
2. **The same rule loses money on QQQ out of sample.** QQQ returned −0.92% (Sharpe −0.149, profit factor 0.892, max drawdown −3.43%). The correlation of daily `strategy_net` between SPY and QQQ is 0.904. Line 5 fails. IWM, on the same dates, returned +4.03% out of sample (Sharpe 0.827). IWM is not line 5.
3. **The full-sample +12.36% is a few sessions (post hoc).** The best 10 sessions sum to 1.102 times the sum of every daily `strategy_net`. Setting those ten to zero leaves a compound return of −1.39% (Sharpe −0.052). The block-bootstrap 95% interval of the full-sample Sharpe is −0.064 to 0.904. It includes zero. This does not change §8. Lines 1 and 5 had already failed.

**Recommendation.** Do not trade it. The notional grid, IWM, the weekday table, and the holiday table stay diagnostics. The locked list stays closed: the overnight gap, the session after the holiday, every Friday, a single holiday, QQQ or IWM as the primary, a lower cost, and the 1-minute print.

## 2. The strategy

### Rules

```
On an NYSE session t that has a SPY daily bar, if the next weekday is a full-day NYSE holiday:
    buy 100% of equity at the daily open of t
    sell 100% at the daily close of t
Otherwise:
    flat

strategy return on t =
    close[t] / open[t] - 1 - 0.0002    if t is a pre-holiday
    0                                   otherwise

Exit reason: session_close.
No overnight hold. A Friday before an ordinary Monday is not a trade.
An early close is a trade only when the next weekday is a full-day holiday.
```

- **Why the cash session.** The claim is the pre-holiday session itself (Ariel 1990; Lakonishok and Smidt 1988), not the gap into the closure. The existing overnight study is flat through this session. This rule is flat through that gap.
- **Why the daily open and close.** The 1-minute tape starts in 2021-09 and is a different series. The rule uses the stored daily open and the stored daily close.
- **Why 1 bp per side.** Protocol default for SPY, fixed before any holiday return in this study. The round trip is 2 bp of notional on a pre-holiday session. It was not changed after the P&L.

### How it trades

| | |
|---|---|
| Sessions with a trade | 146 of 3,951 evaluation sessions (exposure 3.70%) |
| Trades per year | 9.31 full sample; 10.24 out of sample |
| Time in market | The cash session on a pre-holiday date. Flat overnight. Flat on every other session. |
| Holding time | median 6.5 hours, mean 6.34 hours. Eight trades are stamped 13:00 (3.5 hours). |
| Long / short | 146 long, 0 short. Exit reason `session_close` on all 146. |
| Win rate | 56.2% full sample. Average winner 47.3 bp, average loser −41.9 bp. Out of sample, win rate 43.5%, average winner 53.7 bp, average loser −37.6 bp. |

Eighty of the 146 trades fall on a Friday, because many holidays are Mondays. That is the calendar. It is not a Friday filter.

## 3. Hypothesis and predictions

The long side holds SPY from the open to the close on the last session before a closure. The other side flattens before the closure and buys the cash session back only after the holiday (Ariel 1990; Lakonishok and Smidt 1988). If that is why the session is special, its gross open-to-close mean should beat the gross open-to-close mean of ordinary sessions, in the full evaluation window and again out of sample.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Full-sample mean gross open-to-close on pre-holiday sessions is higher than on the other evaluation sessions. | 10.17 bp on 146 sessions versus 2.24 bp on 3,805 sessions. Difference +7.93 bp. | Consistent |
| 2. The same inequality inside the out-of-sample window. | 4.08 bp on 23 sessions versus 4.47 bp on 543 sessions. Difference −0.39 bp. | Not consistent |

Prediction 1 holds on the written comparison. Prediction 2 does not. The out-of-sample pre-holiday sessions were not richer than the other cash sessions in a window that was already known to be a rising market. The acceptance table failed. The full-sample gap does not repair it. The mechanism is not confirmed.

## 4. Method

- **Data.** `bars(symbol, "1d", start="2011-01-04", end="2026-10-01")` for SPY, QQQ, and IWM, split adjustment left on. Each name has 3,959 daily bars, first 2011-01-04, last 2026-10-01, identical dates, no duplicates. `corporate_action` is empty, so the adjustment changes nothing. Dividends are not in the open-to-close move. The book is flat overnight, so an ex-dividend gap is not held. The close-to-close benchmark does contain that gap and does not receive the dividend cash. The dates cannot be listed from this store. No outside calendar was added. The three NYSE sessions with no bar are 2012-10-29, 2012-10-30, and 2018-12-05. None of them is a pre-holiday. 2025-01-09 is a holiday in `mdq` and has no bar. The session before it, 2025-01-08, is a trade. 2021-12-31 has a bar and is not a pre-holiday: the next weekday is 2022-01-03, and that Monday is not a holiday. Early closes stay in the sample. Eight pre-holiday sessions are in `mdq.EARLY_CLOSES` and are stamped 13:00. `EARLY_CLOSES` starts in 2019, so a pre-2019 early close that is a pre-holiday is stamped 16:00. The fill is still that session's daily close. The stamp does not change a return.
- **Pre-registration.** `RULES.md` fixed the holiday function, the open-to-close hold, notional 1.0, 1 bp per side, the 2024-07-01 split, the five notionals, the 15-trade floor, the seeds, and the six lines before any return. The pre-lock look was coverage, `corporate_action`, and the calendar (`counts.py`, `counts.json`). `bars()` was not called before the lock. The calendar count was 146 pre-holiday sessions, 123 before 2024-07-01 and 23 from 2024-07-01 on. The floor stayed 15. The out-of-sample window was already used by `spy-overnight-premium`, including the open-to-close and close-to-close benchmarks on these same 566 sessions. From that report, open-to-close was +25.14% (Sharpe 0.709) and close-to-close was +40.52% (Sharpe 0.985). This run's benchmarks match those two numbers. The pre-holiday subset was not known. The every-night gap study was Rejected (out-of-sample Sharpe 0.053 after the same 1 bp per side). That book is flat during this study's holding period. `RULES.md` was not committed. The request forbade a commit. `git_head` at the lock and at both runs was `577dd38c7b98928f8fe9a696aff046425df9dafa`, dirty.
- **Fills and costs.** Buy the daily open. Sell the daily close. Entry stamp 09:30 America/New_York. Exit stamp 13:00 when the date is in `mdq.EARLY_CLOSES`, otherwise 16:00. A one-session delay was not run. It would buy the session after the holiday, which the rules exclude. There is no same-bar close entry. That would hold the overnight gap. Cost is 1 bp of notional per side, both sides, on a pre-holiday session, including a zero open-to-close. This report does not remeasure the spread.
- **Returns.** Daily simple returns on the evaluation sessions. Ordinary sessions are 0. Sharpe is the mean divided by the sample standard deviation (`ddof = 1`) times √252, risk-free rate 0. The acceptance Sharpe is that series. CAGR uses a 252-session year. Profit factor is dollar P&L compounded from equity 1 inside the slice, trades in date order. Slice metrics restart at equity 1. The mean net trade is the mean of the trade `net` values, so the zeros do not hide it. It is not the acceptance statistic.
- **Look-ahead.** The decision is the published holiday calendar, used at the open. The open is the fill, not a signal. The close is the exit fill, known at the closing auction, not at the daily bar's 09:30 timestamp. No rolling statistic enters the book. Quintile bins are a table after the run.
- **Verification.** The self-test ran before the store opened and passed: a Thursday before Good Friday, a Friday before a Monday holiday, an ordinary Friday, an early close that is not a trade, an early close that is a trade, the Juneteenth weekend skip, 2021-12-31 left out of the evaluation window, the 2025-01-08 mourning eve, cost on a flat session and on both sides of an up day and a down day, notional scaling, and the Sharpe and block-index formulas. `verify.py` does not import `backtest.py`. It matched all 146 SPY trades on side, entry time, entry price, exit time, exit price, gross, net, and reason, and it matched `strategy_net` on all 3,951 sessions. The explicit sample of 40 (seed 20261094) matched.
- **Runs.** One pre-registered store run, 2026-10-02T21:17:55+00:00, reason `initial pre-registered run`. One verification replay, 2026-10-02T21:21:01+00:00. No bug fix. No second result. `posthoc.py` and `charts.py` read the output files and do not open the store. `counts.py` ran before the lock and did not read a price.

## 5. Results

![Growth of $1](figures/equity.svg)

The strategy ends at 1.12, close-to-close at 5.96, and open-to-close at 2.38. The dashed line is 2024-07-01. After that line the strategy is almost flat. The full sample is on the figure.

![Drawdown](figures/drawdown.svg)

The strategy's deepest hole is −3.25%, and it is in sample. Out of sample, restarted, the hole is −2.11%. Close-to-close's full-sample hole is −34.1%. The strategy's hole is shallow because the book is invested on 3.70% of sessions.

| Strategy (1 bp/side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **SPY primary, notional 1** | +12.36% / 0.411 / −3.25% | 0.456 | +0.45% / 0.122 / −2.11% |
| SPY, zero cost | +15.69% / 0.509 / — | 0.551 | +0.91% / 0.239 / — |
| QQQ, same rule | +9.00% / 0.239 / −6.06% | 0.307 | −0.92% / −0.149 / −3.43% |
| IWM, same rule | +17.19% / 0.438 / −5.06% | 0.380 | +4.03% / 0.827 / −1.12% |
| *SPY close-to-close* | *+495.7% / 0.751 / −34.10%* | *0.712* | *+40.52% / 0.985 / −19.88%* |
| *SPY open-to-close* | *+137.8% / 0.486 / −24.59%* | *0.442* | *+25.14% / 0.709 / −18.22%* |

![Calendar-year return](figures/by_year.svg)

Four calendar years are below zero for the strategy: 2017, 2018, 2020, and 2025. In 2025 the strategy returned −1.26% while close-to-close returned +16.4%. In 2022 the strategy returned +1.69% while close-to-close returned −19.5%. 2024 contains both sides of the sample split. 2011 starts on 2011-01-14. 2026 ends on 2026-10-01 and has 188 sessions.

| Year | Strategy | Sharpe | Max DD | Close-to-close | Trades |
|---|---:|---:|---:|---:|---:|
| 2011 | +0.67% | 0.32 | −1.79% | −2.24% | 9 |
| 2012 | +2.47% | 1.17 | −0.31% | +13.47% | 9 |
| 2013 | +1.40% | 1.31 | −0.54% | +29.69% | 9 |
| 2014 | +0.003% | 0.01 | −1.34% | +11.29% | 9 |
| 2015 | +0.74% | 0.45 | −1.36% | −0.81% | 9 |
| 2016 | +2.37% | 1.70 | −0.55% | +9.64% | 9 |
| 2017 | −1.02% | −1.02 | −1.60% | +19.38% | 9 |
| 2018 | −1.29% | −0.54 | −2.92% | −6.35% | 9 |
| 2019 | +0.94% | 0.78 | −0.62% | +28.79% | 9 |
| 2020 | −0.55% | −0.38 | −1.85% | +16.16% | 9 |
| 2021 | +2.48% | 1.84 | −0.29% | +27.04% | 8 |
| 2022 | +1.69% | 0.48 | −2.04% | −19.48% | 10 |
| 2023 | +1.73% | 0.86 | −1.44% | +24.29% | 10 |
| 2024 | +1.39% | 1.00 | −0.76% | +23.40% | 10 |
| 2025 | −1.26% | −0.70 | −2.11% | +16.37% | 11 |
| 2026 | +0.07% | 0.06 | −0.69% | +12.05% | 7 |

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each pre-holiday session in place and multiplies that session's gross strategy return by a random ±1. Ordinary sessions stay 0. Two thousand draws, seed 20261091. The actual gross Sharpe of the daily series is 0.509. The null mean is 0.002. Thirty-one draws were at least as large. p = 0.0160. That passes line 2. It is a statement about the full-sample gross series, not about the out-of-sample Sharpe.

The timing placebo picks 146 sessions at random from the 3,951 evaluation sessions and takes their open-to-close return. Five hundred draws, seed 20261093. The null mean is 0.096. Twenty-five draws were at least as large as 0.509. p = 0.0519. This check was not an acceptance line. A random long cash-session book of the same length has a positive average gross Sharpe in this sample, which is the cash-session path already reported by the overnight study.

### Bootstrap

Circular blocks of 20 sessions, 2,000 draws, seed 20261092, on the daily net series including the zeros. The 95% interval of the full-sample Sharpe is −0.064 to 0.904. The median draw is 0.424. The interval includes zero. The out-of-sample t-stat of the mean daily return is 0.183.

### Parameter plateau

![Parameter grid](figures/grid.svg)

Notional in {0.5, 0.75, 1.0, 1.25, 1.5} at base cost. The daily returns scale by the notional and the zeros stay zero, so the five in-sample Sharpes are the same value, 0.456. The stored span is 1.1×10⁻¹⁶. Five of five cells are above zero. The in-sample / out-of-sample rank correlation is undefined. `is_best_notional` is null. Nothing was selected. The primary stays at 1.0. Out-of-sample Sharpe is 0.122 at every cell. Out-of-sample compound return runs from +0.23% at half notional to +0.64% at 1.5. Line 3 passes because the shared in-sample Sharpe is above zero. The grid does not show five different parameter values.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.509 | 0.460 | **0.411** | 0.312 | 0.211 |
| OOS Sharpe | 0.239 | 0.181 | **0.122** | 0.005 | −0.113 |
| Full-sample return | +15.69% | +14.01% | **+12.36%** | +9.13% | +5.99% |

Line 4 uses the full-sample return at 2 bp per side, +9.13%, which is above zero. Out of sample at that same cost the return is −0.015% and the Sharpe is 0.005. Those out-of-sample cells are not line 4. The full-sample compound return crosses to zero at 4.99 bp per side. A one-session fill delay was not run.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|
| SPY | 0.456 | 0.122 | +12.36% / 1.425 |
| QQQ | 0.307 | −0.149 | +9.00% / 1.214 |
| IWM | 0.380 | 0.827 | +17.19% / 1.447 |

Pearson correlation of `strategy_net`: SPY–QQQ 0.904, SPY–IWM 0.787, QQQ–IWM 0.667. Line 5 is QQQ. IWM's out-of-sample Sharpe does not replace it.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Quintiles are equal counts of the full evaluation sample, sorted by the same session's SPY close-to-close return. The mean below is the mean of `strategy_net` on every session in the bin, including zeros. The restarted return compounds those sessions as their own book. The strategy is long the cash session, so the down bins lose and the up bins gain. Pre-holiday sessions are not piled into the largest up days: quintile 3 has 44 of the trades, quintile 5 has 19, and quintile 1 has 16.

| Quintile | Sessions | Trades | Mean strategy net | Restarted return | Mean close-to-close |
|---|---:|---:|---:|---:|---:|
| 1 (worst) | 791 | 16 | −0.000169 | −12.59% | −1.37% |
| 2 | 790 | 27 | −0.000081 | −6.19% | −0.28% |
| 3 | 790 | 44 | +0.000035 | +2.78% | +0.07% |
| 4 | 790 | 40 | +0.000164 | +13.75% | +0.46% |
| 5 (best) | 790 | 19 | +0.000202 | +17.20% | +1.38% |

Weekday rows are not the acceptance series. Each row drops the other weekdays, so its restarted return is not the book's return. Friday has 80 trades and a restarted return of +7.77%. Monday has 6 trades and a restarted return of −0.18%. Wednesday has 24 trades and a restarted return of +0.02%. Thursday has 27 and +2.62%. Tuesday has 9 and +1.77%. No weekday was used as a filter. There is no short side. Every exit is `session_close`.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS Sharpe and profit factor | Sharpe ≥ 0.5 and PF ≥ 1.10 | Sharpe 0.122, PF 1.090 | ❌ |
| 2. Direction placebo p | ≤ 0.05 on the full-sample gross Sharpe | 0.0160 | ✅ |
| 3. IS Sharpe and IS grid | IS Sharpe > 0 and at least 60% of cells > 0 | IS Sharpe 0.456, 5 of 5 | ✅ |
| 4. Full-sample return at 2× cost | > 0 | +9.13% | ✅ |
| 5. QQQ, identical rules | OOS Sharpe > 0 | −0.149 | ❌ |
| 6. Out-of-sample trades | ≥ 15 | 23 | ✅ |

Line 2 passing does not mean the out-of-sample test passed. The placebo does not use costs, and it is not computed on the out-of-sample window. The five grid Sharpes are one number, so line 3 does not show a plateau of distinct choices. IWM is not line 5.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | IS Sharpe 0.456, OOS Sharpe 0.122, OOS t-stat 0.183. Prediction 2 fails by 0.39 bp. Trailing 252-session Sharpe 0.269 **(post hoc)** against open-to-close Sharpe 1.04 over the same sessions. | None in the rule. The sample split stays. |
| Concentration | Best 10 sessions are 1.10 of the sum of daily `strategy_net` **(post hoc)**. Zeroing them leaves −1.39%. Bootstrap interval −0.064 to 0.904. | None. The book has 146 trades in 15 years. |
| Generalization | QQQ OOS Sharpe −0.149. SPY–QQQ correlation 0.904. IWM OOS Sharpe 0.827 is the same 23 dates. | QQQ was the pre-registered line. IWM is not promoted. |
| Execution | Full-sample break-even is 4.99 bp per side. OOS Sharpe is 0.239 at zero cost and 0.005 at 2 bp per side. No delay test. The stored daily open and close can differ from the 09:30 and 15:59 prints. Eight exits are stamped 13:00. Pre-2019 early closes are stamped 16:00. | Cost stays at 1 bp. The daily bar stays the fill. |
| Short sample | 23 out-of-sample trades. The floor of 15 was met. The t-stat is 0.183. One closure in the window, 2025-01-09, contributes one trade. | The floor was not lowered to 23, and it was not raised after the count. |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question: the out-of-sample Sharpe, the out-of-sample profit factor, and the QQQ line. A different hold, a different symbol, or data after 2026-10-01 is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

The rolling 252-session Sharpe ends at 0.269 on 2026-10-01. Of 3,700 windows, 33.3% are negative. The minimum is −1.67 and the maximum is 2.33.

The trailing 252 sessions, 2025-10-01 through 2026-10-01, returned +0.41% (Sharpe 0.269, max drawdown −0.84%). Open-to-close over those sessions returned +13.10% (Sharpe 1.039). Close-to-close returned +15.05% (Sharpe 1.115).

The ten largest `strategy_net` sessions are 2012-12-31, 2022-05-27, 2015-01-16, 2011-07-01, 2022-07-01, 2023-01-13, 2026-04-02, 2023-05-26, 2024-12-24, and 2016-02-12. Their nets run from +0.0195 to +0.0088. Two of the dates, 2024-12-24 and 2026-04-02, are out of sample. Their sum is 0.1315. The sum of every daily `strategy_net` is 0.1193. The share is 1.102. With those ten set to zero, compound return is −1.39% and Sharpe is −0.052.

Holiday names below are labels on the next weekday. They were not a pre-registered split. `holiday_other` is 0. Mean net is the mean trade, in bp, after the locked cost.

| Next holiday | Trades | Win rate | Mean net | Sum of net |
|---|---:|---:|---:|---:|
| New Year | 14 | 0.43 | −3.87 bp | −0.0054 |
| MLK Day | 16 | 0.75 | +36.58 bp | +0.0585 |
| Presidents Day | 16 | 0.63 | +14.86 bp | +0.0238 |
| Good Friday | 16 | 0.63 | +14.20 bp | +0.0227 |
| Memorial Day | 16 | 0.56 | +15.97 bp | +0.0256 |
| Juneteenth | 5 | 0.60 | −14.69 bp | −0.0073 |
| Independence Day | 16 | 0.75 | +27.22 bp | +0.0435 |
| Labor Day | 16 | 0.25 | −35.18 bp | −0.0563 |
| Thanksgiving | 15 | 0.53 | +4.17 bp | +0.0063 |
| Christmas | 15 | 0.53 | +7.25 bp | +0.0109 |
| Carter mourning | 1 | 0.00 | −29.64 bp | −0.0030 |

Labor Day and MLK Day are not a new rule. Dropping one holiday was on the list of things this study does not do.

### Ideas for a new study

IWM's out-of-sample Sharpe of 0.827 was found on these dates. It is not a candidate. A study that makes IWM the primary needs its own `RULES.md` and data this study did not use. The session after the holiday, and the overnight gap into the holiday, were excluded before any return. They are not results of this file.

## 12. Reproduce

From the repo root:

```bash
python research/spy-pre-holiday/research/backtest.py
```

```bash
python research/spy-pre-holiday/research/verify.py
```

```bash
python research/spy-pre-holiday/research/posthoc.py
```

```bash
python research/spy-pre-holiday/research/charts.py
```

`backtest.py` checks the rules hash, runs the self-test, then writes `results.json`, `daily.csv`, `trades.csv`, `placebo_direction.npy`, and `placebo_timing.npy`. `verify.py` replays SPY from the store. `posthoc.py` writes `posthoc.json` and `rolling_sharpe.csv`. `charts.py` writes the eight SVG files. Seeds: direction 20261091, bootstrap 20261092, timing 20261093, verify 20261094. A rerun on the same store and the same rules gives the same numbers. The bootstrap and the placebos are seeded.

Checklist: the `RULES.md` hash matches `RULES.lock` and `results.json`. Both run-log entries have a permitted reason. The self-test passed and `verify.py` matched every trade. The status is Rejected, which is what two failed lines and a met sample floor mean. Post-hoc numbers are labelled and are not in §8. Prior exposure and the three missing sessions are stated. `research/README.md` does not list this study. The request forbade any edit outside `research/spy-pre-holiday/`. `RULES.md` was not committed. Nothing was written to `data/`. `ingest` was not run. The strategy was not ported into `apps/terminal`.

### References

- Ariel, Robert A. (1990). High stock returns before holidays: Existence and evidence on possible causes. *Journal of Finance*.
- Lakonishok, Josef, and Seymour Smidt (1988). Are seasonal anomalies real? A ninety-year perspective. *Review of Financial Studies*.
