# QQQ plus the studied strategies: a max-Sharpe portfolio

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** The primary (A) failed 3 of 5 pre-registered tests. The secondary (B) failed 2 of 5. |
| Instruments | QQQ buy and hold, plus the net daily returns of five earlier studies: qqq-intraday-trend (P1), qqq-15m-turtle-overnight (T), intraday-channel-trend (C), igv-small-account-fade (F), and micro-futures-trend (M) |
| Data | 2021-10-25 → 2026-09-25, 1,235 sessions. Inputs are the published `daily.csv` and trade files of those studies. No bars were read. 2021-12-31 (no bars in the store) counts as 0 for P1 and C |
| Rules | [`research/qqq-strategy-portfolio/research/RULES.md`](../research/RULES.md), locked 2026-09-26 17:09:52 UTC, sha256 `d26504d65904` |
| Code | [`research/qqq-strategy-portfolio/research/`](../research/) · 1 run (after 1 attempt that stopped at a data check before computing any return), 1 verification (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Weights fitted to maximize in-sample Sharpe put 0.20× in QQQ, 1.42× in P1, and 0.38× in the Turtle strategy. Out of sample (2024-07-01 → 2026-09-25), that portfolio returned **+37.6%** at a Sharpe of **0.91**. QQQ alone returned +55.4% at a Sharpe of 1.01. The portfolio's drawdown was shallower (−16.6% vs −22.9%), but it did not beat QQQ on Sharpe, and its full-sample Sharpe gain was not significant (bootstrap p = 0.13).

| Portfolio A (1× base cost) | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-25 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,235 | 673 | 562 |
| Total return | +152.8% | +83.7% | **+37.6%** |
| CAGR | 20.8% | 25.6% | 15.4% |
| Annual volatility | 16.5% | 15.7% | 17.4% |
| Sharpe | 1.23 | 1.53 *(fitted here)* | **0.91** |
| Max drawdown | −16.6% | −9.5% | −16.6% |
| t-stat of mean | 2.72 | 2.50 | 1.36 |
| *QQQ buy and hold: Sharpe (max DD)* | *0.72 (−35.6%)* | *0.51 (−35.6%)* | *1.01 (−22.9%)* |

The weights were fitted on the in-sample window, so A's in-sample figures are optimistic by construction and are not evidence. It failed 3 of the 5 acceptance tests written before the run (§8). Secondary B, restricted to QQQ + P1, reached an OOS Sharpe of 1.05 with a −12.5% drawdown. It still failed the significance and 2× cost tests.

**Why it failed:**

1. **The optimizer underweighted QQQ.** QQQ's in-sample Sharpe was 0.51, because in-sample includes 2022 (−33.5%). P1's was 1.34. Out of sample the order reversed: QQQ 1.01, P1 0.82. Max-Sharpe weights follow the in-sample means, so they sized P1 at 1.42× and QQQ at 0.20×.
2. **It bought the Turtle's in-sample luck.** T's Sharpe fell from 0.81 in-sample to 0.09 out of sample. Its correlation with P1 rose from 0.19 to 0.42. The 0.38× in T added risk and no return (§5).
3. **Leverage on P1 multiplies its costs.** At 2× cost, A's OOS Sharpe falls to 0.53 and B's to 0.63. QQQ alone pays no strategy costs (§6).
4. **P1 has been flat since May 2025, and A is mostly P1.** From 2025-05-01, A returned +4.2% (Sharpe 0.29) and QQQ returned +56.6% **(post hoc)**.

**What did work, post hoc and not promotable.** Keeping QQQ at 1× and adding P1 as an intraday overlay raised OOS Sharpe at every overlay size from 0.1× to 2× (§11). The naive overlay N, QQQ 1× + P1 1×, was a pre-registered *benchmark*. Out of sample it returned +82.2% at a Sharpe of 1.22, with a −21.1% drawdown. Its full-sample Sharpe gain over QQQ had bootstrap p = 0.005. The rules forbid promoting a benchmark that happens to score better. N was also not blind: I knew the component returns when I wrote the rules. It is proposed below as a new study, to be tested on data this one has not seen.

**Recommendation.** Do not run portfolio A or B. The rejected strategies T, C, F, and M add nothing: weights fitted in hindsight on the OOS window give each of them 0 **(post hoc)**. The only candidate combination left is QQQ held at 1× with P1 overlaid intraday at about 1×. It needs its own pre-registered paper-trading test on forward data (§11).

## 2. The portfolio

### Rules

```
Sleeves: each study's locked primary, net daily return at its own base cost
         Q = QQQ close-to-close, P1, T, C (eligible: own IS Sharpe > 0)
         F (IS -4.28) and M (IS -0.46) excluded before the run
Direction: d on the 0.01 simplex over (Q, P1, T, C) maximizing IS Sharpe of sum d_i r_i
Scale: w = s*d, s = largest value with
         overnight (Q + T) <= 1.0x equity          (no overnight margin)
         intraday gross       <= 2.0x equity       (Reg-T)
         IS volatility        <= QQQ's IS volatility (24.0%)
Freeze w; apply to every session. Daily rebalance of Q and T costs 1 bp per side on the drift.
```

| | Q | P1 | T | C | Binding cap | Overnight | Intraday gross |
|---|---:|---:|---:|---:|---|---:|---:|
| **A** direction d | 0.10 | 0.71 | 0.19 | 0.00 | | | |
| **A** weights w | **0.20** | **1.42** | **0.38** | **0.00** | intraday | 0.58× | 2.0× |
| **B** weights w (d = 0.11, 0.89) | 0.22 | 1.78 | | | intraday | 0.22× | 2.0× |
| *N (benchmark)* | *1.00* | *1.00* | | | | *1.0×* | *2.0×* |
| *EW (benchmark)* | *0.50* | *0.50* | *0.50* | *0.50* | *overnight* | *1.0×* | *2.0×* |

- **Why weights are multiples of equity, not fractions summing to 1:** P1 and C are flat every night, so they can run on the same capital that holds QQQ overnight, using intraday buying power. The caps keep the book inside ordinary Reg-T margin, with no overnight borrowing.
- **Why a volatility cap:** "no more risk than holding QQQ." It did not bind. The 2× intraday cap bound first for A and B.
- **Weight stability.** Re-fitting A on 500 block-bootstrap resamples of the in-sample window gave a P1 direction weight between 0.26 and 0.95 (5th to 95th percentile), QQQ between 0 and 0.28, and T between 0 and 0.43. The fitted weights are a wide estimate, not a precise optimum.

## 3. Hypothesis and predictions

The hypothesis was that P1 is almost uncorrelated with QQQ and makes money on QQQ's worst days, so adding it (and the other positive in-sample sleeves) at max-Sharpe weights raises Sharpe out of sample. The known counter-force was estimation error in the fitted means.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. P1's OOS correlation with QQQ stays below 0.20 in absolute value | 0.07 (IS 0.03) | Consistent |
| 2. On QQQ's worst 5% of days, A's non-QQQ sleeves average > 0 | 62 days, QQQ avg −3.26%. Non-QQQ part of A +1.53%. P1 +0.82%, T +0.97%, C +0.43% | Consistent |
| 3. A's OOS max drawdown is shallower than QQQ's | −16.6% vs −22.9% | Consistent |
| 4. A's OOS Sharpe is lower than its IS Sharpe (optimizer optimism) | 0.91 vs 1.53 | Consistent |

All four predictions held. The diversification mechanism is real: the crash-day payoff and the low correlation appeared out of sample. The portfolio still failed, because the fitted weights overweighted the sleeves whose in-sample Sharpe was inflated and underweighted QQQ. Prediction 4 is the counter-force that decided the result.

## 4. Method

- **Data.** No bars were read. The six sleeves are the net daily returns that the earlier studies published. Each is its locked primary at its own base cost, and none was recomputed. The master calendar is the Turtle study's 1,235 sessions. P1 and C have no row for 2021-12-31, the session with no bars in the store, and count as 0 there. M has no row on four master sessions (2025-06-18, 2025-07-03, 2025-08-29, 2025-11-03), which count as 0. M is excluded from every portfolio and appears only in the sleeve tables. The two QQQ benchmark columns from the P1 and Turtle studies agree to 5e-9.
- **QQQ is a price return.** The store holds no dividends, which understates QQQ by roughly its yield, about 0.6% a year. This biases the comparison slightly against QQQ, and more so against N than against A or B, which hold less QQQ.
- **Cash earns nothing.** This follows the protocol's zero-rate default. A holds 42% of equity in cash overnight and B holds 78%. Neither earns short-term interest in this model. That omission biases line 1 against A and B, and it was not computed. For A it may be of the same order as the 0.10 Sharpe margin by which line 1 failed. The verdict follows the locked rules.
- **Pre-registration.** RULES.md fixed the sleeve screen, the search grid, the caps, the split, the costs, and five acceptance lines before any portfolio return was computed. **Prior exposure:** I had read every component report, including the OOS results, so the OOS test is not blind. The rules deliberately kept T and C, which I knew had failed OOS, because the eligibility screen used in-sample information only. RULES.md was not committed to git before the run.
- **Costs.** The sleeves carry their own base costs: 1 bp per side for P1, T, and C, and 2 bp per side for F. At k× cost, each sleeve's per-session cost is rebuilt from its trade file. For T it comes from its 2 bp column. The daily rebalance of the overnight sleeves (Q, T) costs 1 bp per side on the drift.
- **Returns.** Daily simple returns. Sharpe is mean ÷ sample SD × √252, at a zero rate.
- **Verification.** The self-test covers 15 synthetic cases: grid size and order, the analytic tangency portfolio, the tie-break, each cap, the rebalance cost, the cost multiplier, and the metrics. `verify.py` is a pure-Python re-implementation with no NumPy. It reproduced A's and B's weights exactly, every daily return of A, B, and N within 5e-11, and the 2× cost OOS Sharpes.
- **Runs.** The first attempt stopped at a data check, before any return was computed. The check expected only P1 to lack 2021-12-31, and C lacks it too. The rules' calendar clause already counts a missing row as 0, so the check was widened to allow exactly that session for C. This is logged as a deviation from the literal data check. Then there was one full run, and one verification.

## 5. Results

![Growth of $1](figures/equity.svg)

A and B built their lead over QQQ in 2022. They finished at $2.53 and $2.63 against QQQ's $1.99, but they have gained little since early 2025, while QQQ kept rising. The naive overlay N, which was never optimized, finished highest at $3.15. It also carried most of QQQ's 2022 drawdown.

![Drawdown](figures/drawdown.svg)

| Portfolio (1× base cost) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **A: IS max-Sharpe, {Q, P1, T, C}** | +152.8% / 1.23 / −16.6% | 1.53 | +37.6% / **0.91** / −16.6% |
| B: IS max-Sharpe, {Q, P1} | +162.8% / 1.24 / −12.5% | 1.41 | +46.0% / **1.05** / −12.5% |
| *N: Q 1× + P1 1× (benchmark)* | *+214.7% / 1.05 / −27.6%* | *0.92* | *+82.2% / 1.22 / −21.1%* |
| *EW: 0.5× each of Q, P1, T, C (benchmark)* | *+109.4% / 0.93 / −17.3%* | *1.13* | *+28.4% / 0.70 / −15.6%* |
| *Q: QQQ buy and hold* | *+99.0% / 0.72 / −35.6%* | *0.51* | *+55.4% / 1.01 / −22.9%* |

Sleeves alone, on the same calendar:

| Sleeve | IS Sharpe | OOS Sharpe | OOS return | OOS max DD |
|---|---:|---:|---:|---:|
| Q | 0.51 | 1.01 | +55.4% | −22.9% |
| P1 | 1.34 | 0.82 | +17.4% | −9.8% |
| T | 0.81 | 0.09 | +0.5% | −24.8% |
| C | 0.26 | −0.56 | −12.5% | −21.2% |
| F | −4.28 | −3.82 | −3.0% | −3.0% |
| M | −0.56 | −0.04 | −3.7% | −20.1% |

C's in-sample Sharpe on this calendar is 0.26. Its own study reported 0.19 on a window starting 2021-09-27. M's is −0.56 here and −0.46 on its own window. Both are on the same side of zero, so the screen is unaffected.

**Correlations of daily returns, IS → OOS:** P1–Q 0.03 → 0.07, T–Q −0.03 → 0.03, C–Q 0.04 → 0.19, P1–T 0.19 → 0.42, P1–C 0.38 → 0.59. The three trend-following sleeves became more alike out of sample, so T and C diversified P1 less than the in-sample fit assumed.

| Year | A | Sharpe | B | Sharpe | N | Sharpe | QQQ | Sharpe |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 2021 (from Oct 25) | +5.2% | 2.45 | +4.7% | 2.17 | +9.0% | 2.60 | +7.0% | 1.95 |
| 2022 | **+18.7%** | 0.94 | +15.4% | 0.75 | −24.1% | −0.63 | −33.5% | −1.11 |
| 2023 | +34.7% | 2.43 | +36.0% | 2.59 | +73.1% | 2.91 | +53.8% | 2.52 |
| 2024 | +26.5% | 1.95 | +24.5% | 1.94 | +37.7% | 1.87 | +24.8% | 1.32 |
| 2025 | +7.7% | 0.45 | +15.4% | 0.76 | +27.2% | 0.98 | +20.2% | 0.90 |
| 2026 (to Sep 25) | +10.3% | 1.06 | +11.2% | 1.18 | +25.6% | 1.54 | +21.2% | 1.37 |

A beat QQQ in 2022 and, narrowly, in 2024. It trailed QQQ in 2021, 2023, 2025, and 2026.

## 6. Is it real?

### Bootstrap (paired, 20-session blocks, 2,000 draws, seed 20260926)

| | Actual ΔSharpe vs QQQ | 95% interval | p (Δ ≤ 0) |
|---|---:|---|---:|
| A, full sample | +0.51 | −0.39 to +1.31 | **0.127** |
| A, OOS | −0.10 | −1.63 to +1.05 | 0.570 |
| B, full sample | +0.52 | −0.40 to +1.32 | **0.122** |
| B, OOS | +0.04 | −1.57 to +1.23 | 0.489 |

The full-sample test is lenient, because it includes the window the weights were fitted on, and it still did not reach p ≤ 0.05. A and B hold little QQQ, so their difference from QQQ is noisy. The 95% interval of A's own full-sample Sharpe is 0.58 to 1.85.

### Costs

![Cost sensitivity](figures/costs.svg)

| Strategy cost | 0× | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| A full Sharpe | 1.65 | 1.44 | **1.23** | 0.81 | 0.38 |
| A OOS Sharpe | 1.29 | 1.10 | **0.91** | 0.53 | 0.15 |
| B OOS Sharpe | 1.47 | 1.26 | **1.05** | 0.63 | 0.22 |
| *N OOS Sharpe* | *1.39* | *1.31* | *1.22* | *1.05* | *0.88* |
| A full return | +255.6% | +199.8% | **+152.8%** | +79.7% | +27.7% |

QQQ's OOS Sharpe is 1.01 at any cost. A needs costs at or below about half of base to clear it out of sample. B clears it at base cost, but only by 0.04. N's costs are lower because it holds only 1× of P1, so it degrades more slowly.

### Not applicable

This study combines return streams and makes no trades of its own. There is no direction placebo, parameter grid, or cross-market test. Each sleeve's own report carries those. The substitutes are the paired bootstrap above and the weight-stability resample (§2).

## 7. Where the result comes from

- **Crash days.** On QQQ's worst 5% of days (62 sessions, QQQ average −3.26%), A averaged +0.88% and B +0.74%. A's non-QQQ sleeves averaged +1.53%.
- **2022.** A made +18.7% while QQQ lost 33.5%. A's cumulative lead over QQQ was built in that year. In 2023, 2025, and 2026 it returned less than QQQ.
- **Since May 2025 (post hoc).** Over 353 sessions, P1 returned −1.7% (Sharpe −0.15), A +4.2% (0.29), B +7.1% (0.45), N +54.1% (1.71), and QQQ +56.6% (1.84). A portfolio that is mostly P1 inherits P1's flat spells in full.

None of these breakdowns was used to change the weights.

## 8. Acceptance tests (fixed before the run)

| Criterion | Required | A | | B | |
|---|---|---:|---|---:|---|
| 1. OOS Sharpe > QQQ's OOS Sharpe | > 1.011 | 0.907 | ❌ | 1.049 | ✅ |
| 2. OOS max DD shallower than QQQ's | > −22.9% | −16.6% | ✅ | −12.5% | ✅ |
| 3. Paired bootstrap p, full sample | ≤ 0.05 | 0.127 | ❌ | 0.122 | ❌ |
| 4. OOS Sharpe > QQQ's at 2× strategy cost | > 1.011 | 0.526 | ❌ | 0.632 | ❌ |
| 5. Minimum OOS sessions | ≥ 250 | 562 | ✅ | 562 | ✅ |
| **Status** | | | **Rejected** | | **Rejected** |

A passing drawdown line on its own does not make a better portfolio. Any mix that holds less QQQ than 1× has a shallower QQQ drawdown.

## 9. Risks and weaknesses

| Risk | Evidence |
|---|---|
| Estimation error in the weights | The bootstrap range of P1's direction weight is 0.26–0.95. The in-sample optimum was 0.10 QQQ / 0.71 P1, the full-sample hindsight optimum 0.19 / 0.73 (plus 0.08 T), and the OOS hindsight optimum 0.35 / 0.65 **(post hoc)** |
| Dependence on P1 | P1 is the only sleeve with an edge that survived its own study, and it has been flat since May 2025. Its study found that the identical rules lost money out of sample on SPY and IGV |
| Concentration in few days | Without its 10 best sessions, A's full-sample Sharpe is 0.68, and QQQ's is 0.30 **(post hoc)** |
| Costs and execution | P1 at 1.4–1.8× leverage doubles or triples the cost drag. A's OOS Sharpe halves at 2× cost |
| Unmodelled cash interest and dividends | Both are omitted (§4). Cash interest favours A and B, and dividends favour QQQ and N. Neither size was computed |
| Short sample | 1,235 sessions, one full bear market (2022) |

## 10. No deployment

There is no paper-trading proposal for A or B. Both failed the tests written to decide that question.

## 11. Post hoc (not part of the verdict)

![Overlay frontier](figures/frontier.svg)

**QQQ held at 1×, with P1 overlaid at 0–2× (post hoc).** OOS Sharpe rose from 1.01 with no overlay to 1.14 at 0.5×, 1.22 at 1×, and 1.26 at 1.8×. At 2× cost it peaked at 1.06 near 0.7× and fell to 0.98 at 2×. The in-sample curve kept rising all the way to 2×, which is why an in-sample optimizer pushes P1's weight to the leverage cap. Full-sample max drawdown was −27.6% at 1× overlay and −35.6% with no overlay.

**Benchmark N against the acceptance lines (post hoc).** N had an OOS Sharpe of 1.22 vs 1.01, an OOS drawdown of −21.1% vs −22.9%, and a full-sample bootstrap p of 0.005 (ΔSharpe +0.33, 95% interval +0.08 to +0.57; OOS alone p = 0.16). Its 2× cost OOS Sharpe was 1.05 vs 1.01. It would have met all five lines. It is a benchmark scored after the fact, on data whose component results I already knew, so it cannot be promoted.

**Hindsight optima (post hoc).** Max-Sharpe directions fitted on the data they are scored on:
- full sample: QQQ 0.19, P1 0.73, T 0.08, C 0 (Sharpe 1.30);
- OOS only: QQQ 0.35, P1 0.65, T 0, C 0 (Sharpe 1.26).

Even with hindsight, the rejected strategies get almost nothing.

**Rolling 252-session Sharpe (post hoc).** It was positive in 96% of windows for A, 96% for B, 93% for N, and 87% for QQQ. The latest values are A 0.59, B 0.85, N 1.35, and QQQ 1.22.

### Ideas for a new study

- **QQQ 1× + P1 1× overlay, paper-traded forward.** Hold QQQ fully invested, and run P1 intraday on the same equity at 1× notional. That is 1× overnight and 2× intraday, and it needs a margin account with at least $25k for pattern-day-trader status. The 1× overlay size comes from the qqq-intraday-trend report, which suggested P1 "alongside a long QQQ book" before this study. It was not picked from the frontier above. Pre-register it with forward data only, from the first session after the lock. Include the stop rules of qqq-intraday-trend §10, and QQQ buy and hold as the benchmark. Found on this data; needs its own RULES.md.
- **Robust weighting.** Fit weights on risk only (inverse volatility or equal risk contribution), with no estimated means, which removes the failure mode in reason 1 above. This also needs data this study has not used.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-strategy-portfolio/research/backtest.py --reason "reproduce"
```

```bash
python research/qqq-strategy-portfolio/research/verify.py
```

```bash
python research/qqq-strategy-portfolio/research/posthoc.py
```

```bash
python research/qqq-strategy-portfolio/research/charts.py
```

- `backtest.py` checks the RULES.lock hash and runs the self-test. It writes `results.json` and `daily.csv`, and appends to `RUNLOG.md`, in about 5 s.
- `verify.py` re-derives the weights and daily returns in pure Python.
- `posthoc.py` writes `posthoc.json`.
- `charts.py` writes `report/figures/*.svg`.

Seeds are 20260926 (bootstrap) and 20260927 (weight stability). Reruns on the same input files give identical numbers.
