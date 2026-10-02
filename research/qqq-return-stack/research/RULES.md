# QQQ core with the studied strategies stacked on top: pre-registered rules

Written 2026-09-26, before any portfolio return was computed. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** none. This study reads no bars. Its inputs are the published `daily.csv` and trade files of twelve earlier studies.
- **Data-alignment check run before this file** (`counts.py`, session counts and blank counts only, no statistic of any return column):
  - The master calendar, the turtle study's `daily.csv`, has 1,235 sessions, 2021-10-25 → 2026-09-25: 673 through 2024-06-28 and 562 from 2024-07-01.
  - P1 and C have no row for 2021-12-31 (no bars in the store). POP starts 2021-12-21, so it has no row on the first 40 master sessions. M has no row on 2025-06-18, 2025-07-03, 2025-08-29, and 2025-11-03, and has in-window rows on two non-NYSE dates (2023-11-23, 2025-01-09). M's evaluation flag is 1 on 995 master sessions, from 2022-10-03. Every other file covers every master session. No used column has a blank or non-finite value on a master session.
  - The Finviz gap-fade book has **0 sessions with a position on the master calendar**. Its locked account was ruined on 2020-07-30 and opened nothing afterwards.
- **Earlier studies whose outputs are the inputs here.** I have read every report, including its out-of-sample numbers. **The OOS window 2024-07-01 → 2026-09-25 is not unseen data.** What I already know, as each study reported it (own windows, own costs):

  | Key | Study | Verdict | IS Sharpe | OOS Sharpe |
  |---|---|---|---:|---:|
  | P1 | qqq-intraday-trend | Paper-trading candidate | 1.34 | 0.82 |
  | T | qqq-15m-turtle-overnight | Rejected | 0.81 | 0.09 |
  | C | intraday-channel-trend | Rejected | 0.19 | −0.56 |
  | F | igv-small-account-fade | Rejected | −4.28 | −3.82 |
  | M | micro-futures-trend | Rejected | −0.46 | −0.19 |
  | POP | index-opening-pop-fade | Rejected | −0.57 | −0.66 |
  | SCALE | qqq-atr-scale-in | Rejected | −1.53 | −0.57 |
  | MART | qqq-atr-martingale | Paper-trading candidate | 1.52 | 1.02 |
  | BOLL | qqq-bollinger-adding | Rejected | −1.98 | −0.89 |
  | GAP | small-cap-gap-up-fade | Paper-trading candidate | 1.74 | 2.60 |
  | REV | low-liq-high-vol-mean-reversion | Rejected | 0.01 | −1.13 |
  | RSI2 | spy-rsi2-dip-buy | Paper-trading candidate | 0.64 | 1.29 |
  | — | finviz-gap-up-fade | Inconclusive (ruined 2020) | 0.92 | undefined |

  GAP and REV used an IS window ending 2023-12-29 and M one ending 2024-09-30, so parts of their own OOS fall inside this study's IS window. QQQ buy and hold: IS Sharpe about 0.51, OOS about 1.01, max drawdown −35.6% in 2022.
- **The earlier portfolio study** (`qqq-strategy-portfolio`, rejected) fitted max-Sharpe weights on in-sample means and failed because the optimizer underweighted QQQ and overweighted in-sample luck. Its post-hoc section found that holding QQQ at 1× and overlaying P1 raised OOS Sharpe, and it proposed risk-only weighting as a new study. This study is that design: QQQ fixed at 1×, and every overlay sized on in-sample volatility only, with no estimated means.
- **Consequence.** Because I know every sleeve's OOS Sharpe, I can roughly predict the OOS result of any combination. Pre-registration cannot make this test blind. It fixes the sleeve screen, the sizing, the financing, and the acceptance lines before a portfolio return is computed, so none of them can be tuned to the outcome. The screen uses in-sample information only, and it keeps T, C, and REV, which I know failed OOS.
- **Where the parameters came from:** reasoned a priori below. None was chosen by looking at a portfolio return.

## Hypothesis

A portfolio that holds QQQ at 1× and stacks each strategy whose own study reported a positive in-sample Sharpe on top of it, each sized to the same in-sample volatility, has a higher out-of-sample Sharpe and CAGR and a shallower out-of-sample drawdown than QQQ alone.

**Mechanism.** Return stacking: the core keeps the full equity premium, and each overlay adds its own return on top, financed by margin or intraday buying power rather than by selling QQQ. The overlays are mostly flat overnight and trade intraday or on dips and gaps, so their returns should be weakly correlated with QQQ and with each other. Equal risk budgets use no estimated means, so they cannot overweight a sleeve's in-sample luck, which was the failure of `qqq-strategy-portfolio`.

**Known counter-forces.**
- Three of the seven eligible sleeves (T, C, REV) failed their own OOS tests. Equal risk gives each loser the same budget as each winner.
- Overlays add costs and, where held overnight, financing. QQQ alone pays neither.
- Several sleeves trade QQQ intraday (P1, C, MART), so they may be correlated with each other.
- MART's notional reached 21× its capital. Its daily risk is not stationary.

## Predictions beyond P&L

If the mechanism is right, then:

1. The overlay of S (S's return minus the Q core) has an OOS daily correlation with Q below 0.30 in absolute value.
2. On Q's worst 5% of days in the full sample, S's overlay averages > 0.
3. The average pairwise OOS correlation among S's seven sleeves is below 0.20.
4. S's OOS max drawdown is shallower than Q's.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- **Inputs** (read-only, never recomputed; each is the locked primary's net daily output at its own base cost):

  | Key | File (`research/…/research/`) | Return column | 1× cost per session, `x_i` |
  |---|---|---|---|
  | Q | `qqq-15m-turtle-overnight/daily.csv` | `bench_qqq_ret` | none |
  | P1 | `qqq-intraday-trend/daily.csv` | `p1` | 2 bp × trades in `trades_p1.csv` whose `session` is t |
  | T | `qqq-15m-turtle-overnight/daily.csv` | `qqq_ret` | `qqq_ret − qqq_2bp_ret` |
  | C | `intraday-channel-trend/daily.csv` | `r_book` | 2 bp ÷ 3 × trades in `trades.csv` whose `session` is t |
  | F | `igv-small-account-fade/daily.csv` | `strategy` | Σ over trades with `session` t of (`gross_dollars − net_dollars`) ÷ (25,000 × F's `equity` on t−1, 1 on the first session) |
  | M | `micro-futures-trend/daily.csv` | `ret`, only where `in_window = 1` | `gross_ret − ret`, same rows |
  | POP | `index-opening-pop-fade/daily.csv` | `primary_net` | `primary_gross − primary_net` |
  | SCALE | `qqq-atr-scale-in/daily.csv` | `qqq_ret` | Σ over campaigns with `session` t of `n_units` ÷ 3 × 2 bp |
  | MART | `qqq-atr-martingale/daily.csv` | `pnl` (P&L per 1 of initial capital) | Σ over campaigns with `exit_session` t of 2 bp × Σ `leg_notionals` |
  | BOLL | `qqq-bollinger-adding/daily.csv` | `qqq_net` | `qqq_gross − qqq_net` |
  | GAP | `small-cap-gap-up-fade/daily.csv` | `ret` | Σ over trades with `entry_session` t of `weight × cost` |
  | REV | `low-liq-high-vol-mean-reversion/daily.csv` | `ret` | Σ over trades with `exit_session` t of (`cost + borrow`) × `equity_at_entry` ÷ REV's `equity` on t−1 |
  | RSI2 | `spy-rsi2-dip-buy/daily.csv` | `strategy_net` | `strategy_gross − strategy_net` |

  MART's return is P&L per unit of initial capital, as its study defines it: a sleeve given fixed capital, whose units are fractions of that capital and do not resize. The MART and REV cost attributions (whole campaign at its exit session) and the F and REV equity denominators are approximations. They are used only on the cost sweep and acceptance line 5, never at 1×.
- **Excluded before the run:** `finviz-gap-up-fade`, because its locked book has no position on any master session. A sleeve with zero variance cannot be risk-scaled. `qqq-strategy-portfolio` is a portfolio of these inputs, not a sleeve. `qqq-intraday-reversion` has no published daily file.
- **Calendar.** The 1,235 master sessions above. A sleeve with no row on a master session contributes 0 that session. Rows off the master calendar are ignored (M loses two in-window dates). A sleeve is **live** on a master session if it has a row there, and for M only if `in_window = 1`. Live sessions matter only for the IS volatility.
- **Q is a price return.** The store holds no dividends, so Q understates QQQ's total return by roughly its yield, about 0.6% a year. The composite holds the same 1× core, so this cancels in every comparison with Q.
- **Checks the script must pass before it writes results:**
  - 1,235 master sessions: 673 IS and 562 OOS.
  - Missing master rows are exactly those listed under Prior exposure.
  - No blank or non-finite value in any used column on a master session.
  - `bench_qqq_ret` equals `qqq-intraday-trend` `hold` within 1e-8 on every shared session.

## Primary rule (portfolio S)

Parameters, all fixed:
- `SLEEVE_VOL = 0.05`: each overlay's IS annualized volatility. About one-fifth of QQQ's IS volatility (roughly 24%), so each sleeve carries the risk of about a 20% QQQ position. With seven sleeves, the overlay would match QQQ's risk only if the sleeves were almost perfectly correlated. A round number, fixed a priori.
- `Q_WEIGHT = 1.0`: the core is always 1× equity.
- `REBAL_COST_BPS = 1`: QQQ-class cost per side on the core's daily drift.
- `FIN_RATE = 0.05`: annual rate on overnight net long notional above 1× equity, charged per session as `FIN_RATE / 252`. The store holds no rate history. 5% is a flat broker margin rate: above the policy rate in 2021–22 and near it in 2023–26, so it is conservative early in the sample. Short proceeds earn nothing, and unused cash earns nothing (the protocol's zero rate).

1. **Eligibility (in-sample information only).** A sleeve is eligible if the IS Sharpe reported by its own study is > 0. The eligible set is **{P1, T, C, MART, GAP, REV, RSI2}**. F, M, POP, SCALE, and BOLL are not eligible.
2. **Scale.** For each eligible sleeve i, `σ_i` = sample SD (ddof 1) × √252 of r_i over the IS master sessions on which i is live. The weight is `w_i = SLEEVE_VOL / σ_i`, a notional multiple of the sleeve's own sizing. Computed once, on IS data only, and frozen for IS and OOS. Nothing is re-estimated.
3. **Composite return** each session t:
   `r_S,t = r_Q,t + Σ_i w_i r_i,t − rebal_t − fin_t`
   - `rebal_t = REBAL_COST_BPS / 10,000 × |r_Q,t − g_t|`, with `g_t = r_Q,t + Σ_i w_i r_i,t` (the core drifts from 1× when the overlays move equity). The overlay sleeves size themselves from their own equity or capital, and their drift is not charged.
   - `fin_t = FIN_RATE / 252 × max(0, L_t − 1)`, where `L_t = 1 + Σ_i w_i e_i,t` is the net long notional held through the close of t, in units of equity. `e_i,t` at the close of t: T, +1 or −1 when a trade in its `trades.csv` has an entry date ≤ t and an exit date > t, else 0; MART, `side × gross_notional`; RSI2, `held_at_close`; every other sleeve 0 (flat overnight, dollar-neutral, or futures).
4. **Costs at k× base.** Sleeve i's return is `r_i − (k − 1) × x_i`. The rebalance cost also scales by k. Financing does not.

## Secondary candidate (portfolio K)

Identical to S, with the sleeve set restricted to the four studies whose verdict is paper-trading candidate: **{P1, MART, GAP, RSI2}**. **Disclosure:** those verdicts depend on OOS results, so K is not a clean test. It is scored on the same lines and is never promoted if S fails.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Sleeve daily returns r_i,t | close of session t | the composite return of session t (no forecast) |
| σ_i (IS volatility) | 2024-06-28 close | weights applied from 2021-10-25 |
| Eligibility (own IS Sharpe > 0) | each study's IS end | the sleeve set |
| e_i,t (overnight exposure) | close of t | financing for the night after t |

Applying IS-fitted scales to IS sessions uses IS volatility in IS. Scales use no means, so the bias is small, but IS metrics of S and K are still not evidence. OOS is scored.

## Samples

- No warm-up. The sleeves carry their own.
- **In-sample:** 2021-10-25 → 2024-06-28, 673 sessions.
- **Out-of-sample:** 2024-07-01 → 2026-09-25, 562 sessions.
- The same split as P1, T, C, F, and the earlier portfolio study, so results are comparable. The OOS window is exposed (Prior exposure).

## Benchmarks

On the same sessions:
- **Q**, QQQ buy and hold at 1×, no cost.
- **ALL**, the same construction with all twelve sleeves, eligible or not. It shows what stacking every study does.

## Reported checks

All of these appear in the report whatever they show.

- **Metrics** for full, IS, and OOS, for S, K, ALL, Q, and each of the twelve sleeves alone (raw and scaled): total return, CAGR (252), annualized volatility, Sharpe (mean ÷ sample SD × √252, zero rate), max drawdown of compounded equity, t-stat of the mean.
- **Scales:** σ_i, w_i, and each sleeve's nominal notional at its own sizing times w_i. MART's observed maximum `gross_notional` times w_MART.
- **Impact of each sleeve** (the purpose of the visuals):
  - *Add-one:* Q plus sleeve i alone at w_i, with the same rebalance and financing rules, for all twelve sleeves: ΔSharpe, ΔCAGR, and Δmax drawdown against Q, full, IS, and OOS.
  - *Leave-one-out:* S without sleeve i (seven cases) and ALL without sleeve i (twelve cases): ΔSharpe against S or ALL, full and OOS.
  - *Contribution:* Σ_t w_i r_i,t per sleeve, per window, and its cumulative path.
  - *Risk contribution:* `w_i cov(r_i, r_S) / var(r_S)` for S, and the same for ALL, full and OOS.
- **Correlation matrix** of Q and the twelve sleeves, IS and OOS.
- **Crash and boom days:** the average of each scaled sleeve on Q's worst 5% and best 5% of days (full sample).
- **Calendar-year** return and Sharpe for S, K, ALL, and Q.
- **Cost sweep:** 0, 0.5, 1, 2, and 3× base cost for S, K, and ALL, scales fixed.
- **Risk-budget sweep:** SLEEVE_VOL of 0, 0.025, 0.05, 0.075, and 0.10 for S, K, and ALL. A diagnostic. The primary stays at 0.05.
- **Financing sweep:** FIN_RATE of 0, 0.025, 0.05, and 0.075 for S, K, and ALL.
- **Leverage:** mean and maximum L_t, the share of sessions with L_t > 1, and total financing paid, for S, K, and ALL.
- **Paired circular block bootstrap** of the Sharpe difference against Q, for S, K, and ALL, full and OOS: 20-session blocks, 2,000 draws, the same block indices for both series, seed 20260926. p = (1 + #draws with Δ ≤ 0) ÷ 2001.
- **Verification:** `verify.py` is a pure-Python re-implementation with no NumPy. It reloads the files, recomputes σ_i and w_i, and recomputes the daily returns of S, K, and ALL. It must reproduce every scale and every daily return within 1e-10.

## Acceptance

Portfolio S is a **paper-trading candidate** only if every line holds at 1× base cost:

1. OOS Sharpe(S) > OOS Sharpe(Q).
2. OOS CAGR(S) > OOS CAGR(Q). Return stacking exists to add return on top of the core.
3. OOS max drawdown(S) is shallower than OOS max drawdown(Q).
4. Paired bootstrap p ≤ 0.05 for Sharpe(S) − Sharpe(Q) > 0 on the full sample.
5. At 2× base strategy cost, OOS Sharpe(S) > OOS Sharpe(Q).
6. Minimum sample: 250 OOS sessions, or the verdict is **Inconclusive**. There are 562.

K is scored on the same six lines, with its own verdict, and is never promoted over a failed S.

**Changes from the protocol defaults, and why.** This study combines return streams and makes no trades of its own, so the trade-based lines (profit factor, direction placebo, grid plateau, cross-market) do not apply; each sleeve's own report carries them. The benchmark is QQQ, the core the user already holds, so each line asks whether stacking beats the core alone. The paired bootstrap on the Sharpe difference is the significance test. Line 4 uses the full sample, as the earlier portfolio study did; the OOS p is reported beside it.

A failed line fails the portfolio.

## Not done in this study

The report will not promote any of these in place of S:
- K, ALL, or any add-one or leave-one-out combination;
- a sleeve set chosen from OOS results, other than K as disclosed;
- a different SLEEVE_VOL, FIN_RATE, or per-sleeve budget, including weights fitted on means;
- re-estimated or rolling scales;
- dropping 2022, April 2025, or any sleeve's worst period;
- a sleeve reconstructed from its trades (for example, the Finviz book without its ruin).

Any of these may appear **(post hoc)** as a diagnostic, and under *Ideas for a new study*.
