# QQQ plus studied strategies, max-Sharpe portfolio: pre-registered rules

Written 2026-09-26, before any portfolio return was computed. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `mdq.py actions QQQ` (no rows: no splits or dividends stored) and `mdq.py coverage QQQ --tf 1d` (1,260 sessions with bars, last 1d session 2026-09-23). This study reads no bars. Its inputs are the published `daily.csv` and trade files of five earlier studies.
- **Data-alignment checks run before this file (no outcome statistics):**
  - Session lists of the five `daily.csv` files. P1 has 1,239 sessions (2021-10-18 → 2026-09-25) and lacks 2021-12-31. The turtle and IGV fade files have 1,235 sessions (2021-10-25 → 2026-09-25), including 2021-12-31. The channel file starts 2021-09-27. The micro-futures file has 1,253 dates with futures-only dates, and its evaluation flag starts 2022-10-03.
  - The QQQ close-to-close columns `qqq-intraday-trend/daily.csv:hold` and `qqq-15m-turtle-overnight/daily.csv:bench_qqq_ret` agree on all 1,234 shared sessions to within 5e-9. Only the maximum absolute difference was printed.
- **Earlier studies whose outputs are the inputs here:** I have read every report in full or in summary, including their out-of-sample numbers. **The OOS window 2024-07-01 → 2026-09-25 is not unseen data.** What I already know about each sleeve:
  - `qqq-intraday-trend` (P1): paper-trading candidate. IS Sharpe 1.34, OOS 0.82. Daily correlation with QQQ 0.05. Flat since May 2025 apart from April 2025.
  - `qqq-15m-turtle-overnight` (T): rejected. IS Sharpe 0.81, OOS 0.09.
  - `intraday-channel-trend` (C): rejected. IS Sharpe 0.19, OOS −0.56.
  - `igv-small-account-fade` (F): rejected. IS Sharpe −4.28, OOS −3.82.
  - `micro-futures-trend` (M): rejected. IS Sharpe −0.46 (its own IS, 2022-10-03 → 2024-09-30), OOS −0.19.
  - QQQ buy and hold: IS Sharpe 0.53, OOS 1.01, max drawdown −35.6% (in 2022).
- **Consequence.** Because I know these numbers, I can roughly predict the OOS result of any weighting. Pre-registration here cannot make the OOS test blind. What it can do is fix the weighting method, the eligibility screen, and the constraints before the portfolio is scored, so none of them can be tuned to the OOS outcome. The eligibility screen below uses in-sample information only, and it deliberately keeps T and C, which I know failed OOS.
- **Where the parameters came from:** reasoned a priori below. The leverage caps come from standard US margin rules (Reg-T 2:1). The volatility cap is "no more risk than the buy-and-hold holder already takes." None was chosen by looking at a portfolio return.

## Hypothesis

A portfolio that holds QQQ and adds the studied strategies, with weights set to maximize **in-sample** Sharpe, has a higher **out-of-sample** Sharpe and a shallower OOS drawdown than QQQ buy and hold.

**Mechanism.** QQQ's return is the equity risk premium, and its Sharpe is limited by crash risk. P1 is flat overnight, and its daily returns correlated 0.05 with QQQ. It profits from large intraday moves in either direction, including QQQ's worst days. Adding an uncorrelated positive-mean return stream raises Sharpe by diversification. Because P1 is flat every night, it can be overlaid on a fully invested QQQ position within ordinary intraday buying power, without overnight margin.

**Known counter-forces.**
- **Estimation error.** Max-Sharpe weights are notoriously sensitive to estimated means. In-sample Sharpes of the sleeves (P1 1.34, T 0.81) are higher than their OOS Sharpes (0.82, 0.09), so an optimizer fitted in-sample will overweight the sleeves whose in-sample luck was greatest.
- **P1's edge has been flat since May 2025**, according to its own report.
- **Shared exposure.** T and C trade QQQ, and C also trades SPY and IGV. They may be correlated with Q or with P1.

## Predictions beyond P&L

If the mechanism is right, then:

1. P1's daily correlation with Q stays below 0.20 in absolute value OOS.
2. On Q's worst 5% of days in the full sample, the portfolio's non-Q sleeves contribute a positive average return, i.e. the average of Σ_{i≠Q} w_i r_i > 0.
3. The portfolio's OOS max drawdown is shallower than Q's.
4. Estimation error: the portfolio's OOS Sharpe is lower than its IS Sharpe. This is scored, but it is a prediction of optimizer optimism, not of the mechanism.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- **Inputs** (read-only, never recomputed; each is the locked primary's net output at its own base cost):

  | Sleeve | File | Column | Base cost |
  |---|---|---|---|
  | Q, QQQ buy and hold | `research/qqq-15m-turtle-overnight/research/daily.csv` | `bench_qqq_ret` | none (see rebalance cost) |
  | P1, noise-boundary momentum 1× | `research/qqq-intraday-trend/research/daily.csv` | `p1` | 1 bp per side |
  | T, 15-minute Turtle held overnight | `research/qqq-15m-turtle-overnight/research/daily.csv` | `qqq_ret` | 1 bp per side |
  | C, intraday channel book (QQQ/SPY/IGV) | `research/intraday-channel-trend/research/daily.csv` | `r_book` | 1 bp per side |
  | F, IGV minute fade | `research/igv-small-account-fade/research/daily.csv` | `strategy` | 2 bp per side |
  | M, micro-futures trend | `research/micro-futures-trend/research/daily.csv` | `ret` | $1 + 1 tick |

- **Q is a price return.** The store holds no dividends, so Q understates QQQ's total return by roughly its dividend yield (about 0.6% a year). This biases the comparison slightly *against* Q, and the report states it.
- **Calendar.** The master calendar is the 1,235 sessions of the turtle file, 2021-10-25 → 2026-09-25, which includes 2021-12-31. A sleeve with no row on a master session contributes 0 that session: P1 on 2021-12-31. Rows outside the master calendar are ignored.
- **Checks the script must pass before it writes results:**
  - 1,235 master sessions: 673 IS and 562 OOS.
  - No blank or non-finite value in any used column on a master session, apart from P1's missing 2021-12-31.
  - `bench_qqq_ret` equals `qqq-intraday-trend` `hold` within 1e-8 on every shared master session.

## Primary rule (portfolio A)

Parameters, all fixed:
- `GRID_STEP = 0.01`: the weight-direction search grid.
- `OVERNIGHT_CAP = 1.0`: no overnight borrowing.
- `INTRADAY_CAP = 2.0`: Reg-T 2:1. A pattern-day-trader account allows 4×; 2× is the conservative a priori choice.
- `VOL_CAP`: the IS annualized volatility of Q.
- `REBAL_COST_BPS = 1`: QQQ-class cost per side.

1. **Eligibility (in-sample information only).** A sleeve is eligible if the IS Sharpe reported by its own study is > 0. Q is always eligible. From the reports: P1 1.34, T 0.81, and C 0.19 are eligible. F −4.28 and M −0.46 are not. The eligible set is **{Q, P1, T, C}**.
2. **Weights are notional multiples of equity,** reset every session: the portfolio holds w_i × equity in each sleeve. The portfolio day return is `r_p,t = Σ_i w_i r_i,t − rebal_t`.
3. **Direction.** Over the grid d ∈ {0, 0.01, …, 1}^4 with Σd = 1, pick the d that maximizes the IS Sharpe of `Σ d_i r_i,t`, before rebalance cost, over the IS sessions. Sharpe is mean ÷ sample SD (ddof 1) × √252. Ties go to the first maximizer in lexicographic order of (d_Q, d_P1, d_T, d_C).
4. **Scale.** w = s × d, where s is the largest value satisfying all three caps:
   - overnight exposure s × (d_Q + d_T) ≤ 1.0;
   - intraday gross s × (d_Q + d_P1 + d_T + d_C) ≤ 2.0;
   - IS annualized volatility of `Σ w_i r_i,t` ≤ `VOL_CAP`.

   P1 and C are flat every night, so only Q and T count overnight.
5. **Freeze.** w is computed once, on IS data only, and applied unchanged to IS and OOS. Nothing is re-estimated.
6. **Rebalance cost.** Holding a constant weight in sleeves that carry positions overnight requires daily trading. `rebal_t = REBAL_COST_BPS / 10,000 × Σ_{i ∈ {Q, T}} w_i × |r_i,t − r_p,t|`, with `r_p,t` taken before rebalance cost. The intraday sleeves size themselves from equity every session, so they need no rebalance.
7. **Costs of the strategy sleeves** are already inside their net returns at 1× their base cost. At k× base cost, sleeve i's return is `r_i − (k − 1) × x_i`, where x_i is its cost per session at 1×:
   - P1: 2 bp × the number of trades whose `session` is t, from `trades_p1.csv`.
   - T: `qqq_ret − qqq_2bp_ret`, both from its `daily.csv`.
   - C: Σ over that session's trades in `trades.csv` of 2 bp ÷ 3, the equal-weight book share. Trades are matched on `session`.

   The rebalance cost also scales by k. Q itself has no cost.

## Secondary candidate (portfolio B)

Portfolio B is identical to A, with the sleeve set restricted to **{Q, P1}**. The grid is d_Q ∈ {0, 0.01, …, 1}, d_P1 = 1 − d_Q. **Disclosure:** restricting to P1 uses the fact that P1 is the only study that passed its own acceptance table, and that fact depends on OOS results. B is therefore not a clean test, and it is never promoted if A fails. It is reported because it is the portfolio the repo's rules would otherwise allow.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Sleeve daily returns r_i,t | close of session t | the portfolio return of session t (no forecast) |
| IS mean and covariance of the sleeves | 2024-06-28 close | the weights applied from 2021-10-25, and OOS from 2024-07-01 |
| Eligibility (IS Sharpe > 0) | 2024-06-28 close | the sleeve set |
| VOL_CAP (Q's IS vol) | 2024-06-28 close | the scale |

Applying IS-fitted weights to IS sessions is look-ahead by construction. **IS metrics of A and B are therefore optimistic and are not evidence.** Only OOS is scored.

## Samples

- No warm-up. The sleeves carry their own.
- **In-sample:** 2021-10-25 → 2024-06-28, 673 sessions.
- **Out-of-sample:** 2024-07-01 → 2026-09-25, 562 sessions.
- This is the same split as the P1, T, C, and F studies, so no sleeve's OOS window overlaps this study's IS. For M the windows differ, and M is excluded.

## Benchmarks

On the same sessions:
- **Q**, QQQ buy and hold at 1×.
- **N**, the naive overlay Q 1× + P1 1×. This is the "diversifier alongside a long QQQ book" suggested in the qqq-intraday-trend report. Overnight 1×, intraday 2×. The volatility cap is not applied.
- **EW**, equal direction over {Q, P1, T, C}, scaled by step 4.

Each benchmark carries the rebalance cost where it applies.

## Reported checks

All of these appear in the report whatever they show.

- **Metrics** for full, IS, and OOS, for A, B, Q, N, EW, and each of the six sleeves alone. The metrics are the protocol defaults for daily returns: total return, CAGR (252), annualized volatility, Sharpe, max drawdown of compounded equity, and the t-stat of the mean. No trades are counted: the sleeves' trade statistics are in their own reports.
- **Weights:** d and w for A, B, and EW, and which cap bound.
- **Correlation matrix** of the six sleeves, IS and OOS.
- **Calendar-year returns and Sharpe** for A, B, Q, and N.
- **Cost sweep:** 0, 0.5, 1, 2, and 3× base cost for A, B, and N, with weights held fixed.
- **Paired circular block bootstrap** of the full-sample Sharpe difference, A − Q and B − Q: 20-session blocks, 2,000 draws, the same block indices for both series, seed 20260926. p = (1 + #draws with Δ ≤ 0) ÷ 2001. The same is reported for the OOS window.
- **Weight stability:** re-run step 3 on 500 circular block bootstrap resamples of the IS window (20-session blocks, seed 20260927). Report the mean, 5th, and 95th percentile of each d_i, and the share of draws in which each sleeve gets d_i > 0.
- **Crash days:** the average of each sleeve, and of A's non-Q part, on Q's worst 5% of days (full sample).
- **Verification:** `verify.py` is a pure-Python re-implementation with no NumPy. It loads the files, rebuilds the IS grid search for A and B, and recomputes the daily portfolio returns. It must reproduce d, w, and every daily return within 1e-10.

## Acceptance

Portfolio A is a **paper-trading candidate** only if every line holds at 1× base cost:

1. OOS Sharpe(A) > OOS Sharpe(Q).
2. OOS max drawdown(A) is shallower than OOS max drawdown(Q).
3. Paired bootstrap p ≤ 0.05 for Sharpe(A) − Sharpe(Q) > 0 on the full sample.
4. At 2× base cost, OOS Sharpe(A) > OOS Sharpe(Q).
5. Minimum sample: 250 OOS sessions, or the verdict is **Inconclusive**. There are 562.

B is scored on the same five lines, with its own verdict, and is never promoted over a failed A.

**Changes from the protocol defaults, and why.** This study combines return streams and has no trades of its own, so the trade-based lines (profit factor, direction placebo, IS grid plateau, cross-market) do not apply. The benchmark here is not zero but QQQ, which is the portfolio the user already holds, so each line asks whether the portfolio beats QQQ. The paired bootstrap on the Sharpe difference replaces the direction placebo as the significance test. Line 3 uses the full sample because the IS part of the full sample is where the weights were fitted. That makes line 3 lenient, and the report says so. The OOS bootstrap p is reported beside it.

A failed line fails the portfolio.

## Not done in this study

The report will not promote any of these in place of A:
- weights fitted on the full sample or on OOS, the "in-hindsight optimum";
- a sleeve set chosen from OOS results, other than B as disclosed;
- a different cap, volatility target, or leverage;
- re-estimated or rolling weights;
- dropping 2022 or April 2025;
- a benchmark (N or EW) that happens to score better.

Any of these may appear **(post hoc)** as a diagnostic, and under *Ideas for a new study*.
