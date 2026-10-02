# QQQ core with the studied strategies stacked on top

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** The primary (S) passed 5 of 6 pre-registered tests and failed the 2× cost line. The secondary (K) passed 6 of 6, but a secondary is never promoted when the primary fails, and K's sleeve set was chosen with out-of-sample knowledge. |
| Instruments | QQQ buy and hold at 1×, plus the net daily returns of twelve earlier studies, each scaled to 5% in-sample volatility and added on top |
| Data | 2021-10-25 → 2026-09-25, 1,235 sessions. Inputs are the published `daily.csv` and trade files of those studies. No bars were read. Missing sleeve rows count as 0: P1 and C on 2021-12-31, POP before 2021-12-21, M before 2022-10-03 and on four later sessions. `finviz-gap-up-fade` was excluded before the run: its book was ruined in 2020 and holds nothing in this window |
| Rules | [`research/qqq-return-stack/research/RULES.md`](../research/RULES.md), locked 2026-09-27 01:08 UTC, sha256 `38dae5d16f5a` |
| Code | [`research/qqq-return-stack/research/`](../research/) · 1 run, 1 verification, and 1 cost-formula deviation logged before the run (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Holding QQQ at 1× and stacking the seven strategies whose own studies had a positive in-sample Sharpe on top, each at 5% in-sample volatility, returned **+176.0%** out of sample (2024-07-01 → 2026-09-25) at a Sharpe of **1.70** and a max drawdown of **−21.7%**. QQQ alone returned +55.4% at a Sharpe of 1.01 and a drawdown of −22.9%. The portfolio passed the Sharpe, CAGR, drawdown, significance, and sample-size lines. It **failed line 5**: at twice each strategy's base cost its OOS Sharpe falls to **0.87**, below QQQ's 1.01. The verdict is Rejected.

| Portfolio S (1× base cost) | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-25 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,235 | 673 | 562 |
| Total return | +444.5% | +97.3% | **+176.0%** |
| CAGR | 41.3% | 29.0% | **57.7%** |
| Annual volatility | 29.6% | 30.0% | 29.1% |
| Sharpe | 1.31 | 1.00 | **1.70** |
| Max drawdown | −32.8% | −32.8% | **−21.7%** |
| t-stat of mean | 2.91 | 1.63 | 2.54 |
| *QQQ buy and hold: return / Sharpe / max DD* | *+99.0% / 0.72 / −35.6%* | *+28.1% / 0.51 / −35.6%* | *+55.4% / 1.01 / −22.9%* |

![Growth of $1](figures/equity.svg)

S and K pulled ahead of QQQ from 2023 and ended at $5.44 and $6.33 against QQQ's $1.99. ALL, which also stacks the five strategies with negative in-sample Sharpe, fell to about $0.40 in late 2022 and ended at $1.04, below QQQ.

**The impact of each strategy, in one chart.** Each bar below is QQQ at 1× plus that one sleeve at 5% risk, compared with QQQ alone, out of sample. Four sleeves raised QQQ's Sharpe and CAGR: the small-cap gap fade (GAP, +0.70 Sharpe, +19.6 pp CAGR), the ATR martingale (MART, +0.43, +12.0 pp), SPY RSI(2) (+0.13, +8.3 pp), and intraday trend (P1, +0.15, +5.2 pp). The other eight lowered both. The IGV minute fade (F) cost 12.1 pp of CAGR a year.

![Impact of each strategy](figures/impact_addone.svg)

**What has to sit next to the headline.**

1. **Costs decide it.** At 1× cost the seven sleeves in S pay about 19 pp of equity a year in trading costs at their 5% scales **(post hoc, §6)**. Doubling that costs S 0.83 of OOS Sharpe. At 3× cost S's OOS Sharpe is 0.09. The losing sleeves are mostly cost: with every cost set to zero, ALL's OOS Sharpe is 2.62, against 1.06 at base cost.
2. **Two of the seven sleeves in S made it worse, out of sample.** Removing the weekly reversal (REV) would have raised S's OOS Sharpe by 0.29, and removing the channel breakout (C) by 0.21. Both passed the in-sample screen and both were already known to have failed their own OOS tests. The screen was fixed in advance, so they stay in S (§7).
3. **The overnight book is not always implementable.** MART's position can reach 21× its capital. At its 0.20× scale, S's net overnight exposure ranged from **−3.3×** to **+5.6×** equity. It exceeded Reg-T's 2× overnight limit on 11 sessions and was net short on 14 **(post hoc)**. A broker would have liquidated or refused some of those positions (§9).
4. **The OOS result depends on a few days.** Zeroing S's 20 best OOS sessions takes its OOS Sharpe from 1.70 to −0.02 **(post hoc)**. QQQ's goes from 1.01 to −0.44 on the same test, so this is concentration the core already has, and S adds to it.
5. **None of this is blind.** I had read every component report, including its OOS numbers, before writing the rules. The OOS window was already seen (§4).

**Recommendation.** Do not run S. It failed a line fixed before the run. K is not a substitute: its sleeve set is the four studies that passed their own OOS tests. If the stack is pursued, the candidate for a new pre-registered study is QQQ plus the four paper-trading candidates, tested forward from 2026-09-26 with a cap on MART's notional written into the rules (§11).

## 2. The portfolio

### Rules

```
Core:    QQQ close-to-close, 1.0x equity, every session
Sleeves: each study's locked primary, net daily return at its own base cost
         S = {P1, T, C, MART, GAP, REV, RSI2}   (own-study IS Sharpe > 0)
Scale:   w_i = 5% / (IS annualized volatility of sleeve i)      no means used
Return:  r_S = r_QQQ + sum_i w_i r_i − rebalance − financing
         rebalance = 1 bp x |r_QQQ − gross|   (the core drifting off 1x)
         financing = 5%/252 x max(0, overnight net long − 1x)
Freeze the scales on IS; apply unchanged to every session.
```

| Sleeve | Study | Own IS Sharpe | IS vol (this window) | Scale w | Nominal notional at w | In S | In K |
|---|---|---:|---:|---:|---|:-:|:-:|
| P1 | qqq-intraday-trend | 1.34 | 8.7% | 0.574 | 0.57× intraday | ✓ | ✓ |
| T | qqq-15m-turtle-overnight | 0.81 | 16.4% | 0.304 | ±0.30×, held overnight | ✓ | |
| C | intraday-channel-trend | 0.19 | 10.1% | 0.497 | up to 0.50× intraday | ✓ | |
| F | igv-small-account-fade | −4.28 | 0.65% | 7.75 | 1.55× intraday | | |
| M | micro-futures-trend | −0.46 | 15.6% | 0.321 | 0.32× of the futures book | | |
| POP | index-opening-pop-fade | −0.57 | 6.5% | 0.772 | 0.77× intraday | | |
| SCALE | qqq-atr-scale-in | −1.53 | 4.8% | 1.036 | up to 1.04× intraday | | |
| MART | qqq-atr-martingale | 1.52 | 24.6% | 0.204 | up to 4.27× (21× observed max), overnight | ✓ | ✓ |
| BOLL | qqq-bollinger-adding | −1.98 | 6.0% | 0.833 | up to 0.83× intraday | | |
| GAP | small-cap-gap-up-fade | 1.74 | 49.3% | 0.102 | 0.10× short at the open | ✓ | ✓ |
| REV | low-liq-high-vol-mean-reversion | 0.01 | 18.5% | 0.271 | 0.27× long and 0.27× short, weekly | ✓ | |
| RSI2 | spy-rsi2-dip-buy | 0.64 | 8.5% | 0.589 | 0.59× SPY, overnight | ✓ | ✓ |

- **Why equal risk, and why 5%.** The earlier portfolio study fitted max-Sharpe weights on in-sample means and failed because the optimizer chased in-sample luck. Equal volatility budgets use no means. 5% is about a fifth of QQQ's in-sample volatility (24.0%), so each sleeve carries the risk of roughly a 20% QQQ position. It was fixed before the run. §6 shows the budget sweep as a diagnostic.
- **Why the core stays at 1×.** It is the return-stacking premise: the overlay is added on top of a fully invested core. It is not funded by selling QQQ.
- **The eligibility screen used each study's own in-sample Sharpe.** GAP and REV used 2016–2023 in-sample windows. On this study's in-sample window their Sharpes are −0.36 and −0.48. The screen follows the locked rule and does not use them.

## 3. Hypothesis and predictions

The hypothesis was that QQQ at 1× plus the seven in-sample-positive sleeves at equal risk has a higher OOS Sharpe and CAGR and a shallower OOS drawdown than QQQ. The mechanism is return stacking of weakly correlated overlays. The known counter-forces were the three eligible sleeves already known to have failed OOS, costs, financing, and MART's notional.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. S's overlay has an OOS correlation with QQQ below 0.30 in absolute value | 0.15 (IS 0.19) | Consistent |
| 2. On QQQ's worst 5% of days, S's overlay averages > 0 | 62 days, QQQ average −3.26%, overlay +75 bp | Consistent |
| 3. Mean pairwise OOS correlation of S's seven sleeves is below 0.20 | 0.04 (IS 0.02) | Consistent |
| 4. S's OOS max drawdown is shallower than QQQ's | −21.7% vs −22.9% | Consistent |

All four predictions held. The overlays behaved as diversifiers. The portfolio still failed, because its margin over QQQ is smaller than the extra trading cost that line 5 charges.

## 4. Method

- **Data.** No bars were read. Each sleeve is the net daily return its study published, at its own base cost, and none was recomputed. The master calendar is the Turtle study's 1,235 sessions. The session counts, missing rows, and blank values were checked before the lock with `counts.py`, which computes no return statistic.
- **MART's return** is P&L per unit of initial capital, the definition its own study uses: a sleeve with fixed capital whose units do not resize.
- **QQQ is a price return.** No dividends are stored. The same 1× core is in every portfolio, so this cancels in each comparison with QQQ.
- **Costs.** Each sleeve's 1× cost per session was rebuilt from its trade or daily file (RULES.md, Data table) and used only for the cost sweep and line 5. MART's and REV's costs are attributed to the exit session. The rebalance of the core costs 1 bp per side. Overlay sleeves' own drift is not charged.
- **Deviation, logged before the run.** The locked cost formulas for GAP and REV multiplied quantities that the trade files store in account-equity units. GAP's literal formula would charge 152 per session, which is impossible, so the code uses `weight × (gross_ret − net_ret)`. REV's literal formula counts `equity_at_entry` twice. The code uses `(cost + borrow) ÷ equity_{t−1}`. Line 5 fails under both readings: 0.873 as coded, and 0.938 under the literal REV formula, against 1.011.
- **Financing.** 5% a year on overnight net long notional above 1× equity. S paid 4.4% of equity in total over the sample, 2.0% of it out of sample. Short proceeds and idle cash earn nothing.
- **Pre-registration.** RULES.md fixed the screen, the scale, the financing, the split, and six acceptance lines before any portfolio return was computed. **Prior exposure:** I had read every component report, including OOS results. The OOS test is not blind. The screen used only each study's in-sample Sharpe, and it kept T, C, and REV, which I knew had failed OOS. RULES.md was not committed to git before the run.
- **Verification.** The self-test covers the scale, the live-session window, the composite arithmetic, net-short financing, the cost multiplier, the metrics, the bootstrap, and the Turtle's overnight position. `verify.py` is a pure-Python re-implementation with no NumPy. It reproduced all 12 scales, all 3,705 daily returns of S, K, and ALL (largest difference 5e-13), and the 2× cost OOS Sharpes of S and K.
- **Runs.** One run, and one verification. A timestamp typo and an encoding fault in RUNLOG.md were corrected in a later log entry. No number changed.

## 5. Results

| Portfolio (1× base cost) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / CAGR / Sharpe / max DD |
|---|---|---:|---|
| **S: QQQ + 7 IS-positive sleeves** | +444.5% / 1.31 / −32.8% | 1.00 | +176.0% / 57.7% / **1.70** / −21.7% |
| K: QQQ + 4 paper-trading candidates | +533.4% / 1.52 / −33.0% | 0.96 | +241.3% / 73.4% / **2.26** / −20.0% |
| *ALL: QQQ + all 12 sleeves (benchmark)* | *+3.6% / 0.16 / −61.2%* | *−0.50* | *+69.9% / 26.8% / 1.06 / −26.3%* |
| *Q: QQQ buy and hold* | *+99.0% / 0.72 / −35.6%* | *0.51* | *+55.4% / 21.9% / 1.01 / −22.9%* |

![Drawdown](figures/drawdown.svg)

The stack did not protect the 2022 bear market much. S's worst drawdown was −32.8%, against QQQ's −35.6%, because every sleeve adds risk on top of a full QQQ position. Out of sample, S's drawdowns were about the same depth as QQQ's.

| Year | S | Sharpe | K | Sharpe | ALL | Sharpe | QQQ | Sharpe |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 2021 (from Oct 25) | +18.7% | 3.65 | +13.9% | 3.23 | +6.9% | 1.42 | +7.0% | 1.95 |
| 2022 | −24.8% | −0.55 | −27.7% | −0.70 | −58.2% | −2.28 | −33.5% | −1.11 |
| 2023 | +88.8% | 2.71 | +80.5% | 2.77 | +32.7% | 1.34 | +53.8% | 2.52 |
| 2024 | +62.2% | 2.41 | +67.5% | 2.78 | +21.0% | 0.93 | +24.8% | 1.32 |
| 2025 | +50.0% | 1.35 | +64.9% | 1.84 | +8.2% | 0.44 | +20.2% | 0.90 |
| 2026 (to Sep 25) | +32.9% | 1.67 | +54.3% | 2.64 | +33.6% | 1.68 | +21.2% | 1.37 |

S returned more than QQQ in every calendar year. ALL returned less than QQQ in every full year from 2022 through 2025.

### Each sleeve alone and scaled

| Sleeve | Raw IS Sharpe (this window) | Raw OOS Sharpe | Scaled contribution, full (pp) | Scaled contribution, OOS (pp) | 1× cost drag at scale, pp a year **(post hoc)** |
|---|---:|---:|---:|---:|---:|
| QQQ core | 0.51 | 1.01 | +81.9 | +49.4 | 0 |
| GAP | −0.36 | 2.94 | +28.5 | +33.2 | 1.9 |
| MART | 1.52 | 1.02 | +41.7 | +21.3 | 6.7 |
| RSI2 | 0.64 | 1.29 | +26.4 | +17.8 | 0.2 |
| P1 | 1.34 | 0.82 | +27.6 | +9.8 | 2.5 |
| T | 0.81 | 0.09 | +11.8 | +0.9 | 0.6 |
| M | −0.56 | −0.04 | −6.4 | −0.4 | 0.3 |
| C | 0.26 | −0.56 | −2.6 | −6.1 | 2.8 |
| BOLL | −2.02 | −0.89 | −35.4 | −8.4 | 3.7 |
| SCALE | −1.57 | −0.57 | −30.0 | −9.0 | 4.4 |
| POP | −0.55 | −0.66 | −16.4 | −9.2 | 0.6 |
| REV | −0.48 | −0.91 | −20.1 | −13.7 | 4.7 |
| F | −4.28 | −3.82 | −80.5 | −23.3 | 19.8 |

Contributions are the sums of scaled daily returns, in percentage points of equity. They do not compound, so they do not add up to the portfolio's total return. GAP's raw OOS Sharpe here (2.94) differs from its study's 2.60 because this window starts on 2024-07-01, not 2024-01-02.

![Cumulative contribution](figures/contribution.svg)

The four sleeves that helped rose steadily through both windows. GAP was the exception: it lost about 8 pp through 2022 and 2023 before rising from 2024. F's losses were steady and large because the 5% budget levered a 0.65%-volatility sleeve 7.75×, and its costs scaled with it.

![Contribution by year](figures/contribution_by_year.svg)

**(post hoc)** MART, P1, and RSI2 added to the book in every calendar year. GAP lost 8.4 pp in 2022. REV lost 10.2 pp in 2026 so far. F lost in every year.

## 6. Is it real?

### Bootstrap (paired, 20-session blocks, 2,000 draws, seed 20260926)

| | Actual ΔSharpe vs QQQ | 95% interval | p (Δ ≤ 0) |
|---|---:|---|---:|
| S, full sample | +0.59 | +0.16 to +1.02 | **0.0015** |
| S, OOS | +0.69 | −0.13 to +1.35 | 0.044 |
| K, full sample | +0.80 | +0.47 to +1.14 | 0.0005 |
| K, OOS | +1.25 | +0.64 to +1.79 | 0.0005 |
| ALL, full sample | −0.56 | −1.02 to −0.10 | 0.993 |
| ALL, OOS | +0.05 | −0.71 to +0.70 | 0.475 |

S's Sharpe gain over QQQ passes the full-sample line (p = 0.0015). The scales used no means, so this test is less lenient than the earlier study's. The sleeve screen did use each study's in-sample Sharpe.

### Costs, stack size, and financing

![Sweeps](figures/sweeps.svg)

| Strategy cost | 0× | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| S OOS Sharpe | 2.43 | 2.09 | **1.70** | **0.87** | 0.09 |
| K OOS Sharpe | 2.71 | 2.52 | **2.26** | 1.63 | 0.98 |
| ALL OOS Sharpe | 2.62 | 1.87 | **1.06** | −0.55 | −1.86 |
| S full return | +1,291.6% | +771.8% | **+444.5%** | +110.1% | −20.3% |

QQQ's OOS Sharpe is 1.01 at any cost. S clears it up to somewhere between 1× and 2× cost. It does not clear it at the 2× the rules required. The zero-cost row shows that the rejected sleeves are not all gross losers at this scale: ALL at zero cost has an OOS Sharpe of 2.62. What they lose is mostly the cost of trading at the leverage a 5% budget requires.

| Risk budget per sleeve (diagnostic) | 0% | 2.5% | **5%** | 7.5% | 10% |
|---|---:|---:|---:|---:|---:|
| S OOS Sharpe / max DD | 1.01 / −22.9% | 1.47 / −22.1% | **1.70 / −21.7%** | 1.81 / −22.6% | 1.85 / −24.0% |
| K OOS Sharpe / max DD | 1.01 / −22.9% | 1.73 / −21.4% | **2.26 / −20.0%** | 2.59 / −19.1% | 2.80 / −18.4% |
| ALL OOS Sharpe / max DD | 1.01 / −22.9% | 1.10 / −24.4% | **1.06 / −26.3%** | 0.96 / −30.1% | 0.87 / −35.4% |

A larger budget scales returns and costs together, so the budget does not rescue line 5. The table is a diagnostic. The locked budget stays 5%.

Financing matters little. S's OOS Sharpe is 1.73 at a 0% rate and 1.69 at 7.5%.

### Not applicable

This study combines return streams and makes no trades of its own. There is no direction placebo, parameter grid, or cross-market test. Each sleeve's own report carries those. The paired bootstrap above is the substitute.

## 7. Where the result comes from

### Marginal effect inside the book

![Leave one out](figures/impact_leave_one_out.svg)

Inside S, out of sample, GAP added 0.51 of Sharpe and MART 0.42. P1 and RSI2 added 0.06 and 0.05. T was neutral (−0.03). C (−0.21) and REV (−0.29) subtracted. Inside ALL, F subtracted the most (−0.41).

### Diversification

![Correlations](figures/correlation.svg)

The sleeves in S were nearly uncorrelated with one another (mean pairwise OOS 0.04). The strongest links run among the QQQ intraday strategies. Out of sample, P1 correlated 0.59 with C and −0.76 with SCALE. Momentum and fades on the same tape are close to opposite bets. RSI2 correlated 0.57 with QQQ out of sample, because it is long SPY after dips.

![Tails](figures/tails.svg)

On QQQ's 62 worst days (average −3.26%), S's overlay added +75 bp. P1 (+47), T (+29), and C (+21) did the work. RSI2 (−30) and SCALE (−41) lost on those days, because both buy into falling markets. On QQQ's best days, S's overlay added +99 bp. RSI2 (+49) and P1 (+38) led.

![Risk and return shares](figures/risk_return.svg)

Out of sample, QQQ was 63% of S's variance and 45% of the sum of its daily returns. GAP was 30% of the return and 1% of the variance. It trades rarely, is small at 0.10×, and is uncorrelated with the rest. REV was 7% of the variance and −12% of the return.

### Recent period and concentration (post hoc)

- **Since 2025-05-01** (353 sessions): S +66.8% (Sharpe 1.66), K +112.1% (2.50), ALL +56.3% (1.42), QQQ +56.6% (1.84). S trailed QQQ on Sharpe in this stretch. C lost 9.9 pp and REV 11.3 pp, and P1 was flat.
- **Best days.** Without its 20 best OOS sessions, S's OOS Sharpe is −0.02 and K's is 0.77. QQQ's is −0.44.
- **April 2025.** Without that month, S's OOS Sharpe is 1.65 and QQQ's is 1.13.
- **Rolling 252-session Sharpe** was positive in 93% of windows for S, 92% for K, 87% for QQQ, and 75% for ALL.

![Rolling Sharpe](figures/rolling_sharpe.svg)

## 8. Acceptance tests (fixed before the run)

| Criterion | Required | S | | K | |
|---|---|---:|---|---:|---|
| 1. OOS Sharpe > QQQ's OOS Sharpe | > 1.011 | 1.702 | ✅ | 2.258 | ✅ |
| 2. OOS CAGR > QQQ's OOS CAGR | > 21.9% | 57.7% | ✅ | 73.4% | ✅ |
| 3. OOS max DD shallower than QQQ's | > −22.9% | −21.7% | ✅ | −20.0% | ✅ |
| 4. Paired bootstrap p, full sample | ≤ 0.05 | 0.0015 | ✅ | 0.0005 | ✅ |
| 5. OOS Sharpe > QQQ's at 2× strategy cost | > 1.011 | **0.873** | ❌ | 1.625 | ✅ |
| 6. Minimum OOS sessions | ≥ 250 | 562 | ✅ | 562 | ✅ |
| **Status** | | | **Rejected** | | *Passed, not promotable* |

Line 5 under the literal REV cost formula: 0.938, which also fails. Line 3 passed by 1.2 pp. A 1× QQQ core with any overlay carries at least QQQ's own drawdowns, so this line was never going to pass by much.

K passed every line. It cannot be promoted: the rules never promote a secondary over a failed primary, and K's membership is the four studies that passed their own OOS tests on this same OOS window.

## 9. Risks and weaknesses

| Risk | Evidence |
|---|---|
| Cost sensitivity | S pays about 19 pp of equity a year in overlay trading costs at base cost **(post hoc)**. Its OOS Sharpe falls from 1.70 to 0.87 at 2× cost and 0.09 at 3× |
| MART's notional | Up to 21× its capital, or 4.3× portfolio equity at its 0.20× scale. S's overnight net exposure ranged from −3.3× (2025-10-29) to +5.6× (2025-10-10). It was above 2× on 11 sessions and net short on 14 **(post hoc)**. A margin call would have forced a different rule |
| Borrow and locates | GAP shorts small caps at the open after a 5% gap. Borrow and locates are unpriced. Its own study flags this |
| Not blind | Every component's OOS result was known when the rules were written |
| Concentration | Zeroing the 20 best OOS days turns S's OOS Sharpe to −0.02 **(post hoc)** |
| Sleeves that failed their own tests | T, C, and REV are in S by the in-sample screen. C and REV cost S 0.21 and 0.29 of OOS Sharpe |
| Short sample | 1,235 sessions, one bear market (2022), in which S still lost 24.8% |
| Unmodelled items | Overlay rebalancing, intraday margin limits, and dividends (the last cancels against QQQ) |

## 10. No deployment

There is no paper-trading proposal for S. It failed a line written to decide that question.

## 11. Ideas for a new study

- **QQQ 1× plus the four paper-trading candidates, forward.** K's construction (P1, MART, GAP, RSI2 at 5% each), with a cap on MART's notional (for example, the campaign stops adding when the portfolio's overnight exposure would exceed 2×) fixed in its RULES.md. It must be tested on data after 2026-09-25, because this study's OOS window chose its members. Found on this data.
- **Cost-aware budgets.** Size each sleeve on risk net of its cost per unit of risk, still with no estimated means. That would have shrunk F's 7.75× scale, which multiplied its cost. It needs its own rules and unseen data.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-return-stack/research/backtest.py --reason "reproduce"
```

```bash
python research/qqq-return-stack/research/verify.py
```

```bash
python research/qqq-return-stack/research/posthoc.py
```

```bash
python research/qqq-return-stack/research/charts.py
```

- `backtest.py` checks the RULES.lock hash and runs the self-test. It writes `results.json` and `daily.csv`, and appends to `RUNLOG.md`, in under a second.
- `verify.py` re-derives every scale and daily return in pure Python.
- `posthoc.py` writes `posthoc.json`.
- `charts.py` writes the twelve figures in `report/figures/`.

The seed is 20260926. Reruns on the same input files give identical numbers.
