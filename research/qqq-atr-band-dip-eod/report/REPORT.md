# QQQ daily-ATR-band dip buy, flat at the close: QQQ

| | |
|---|---|
| Date | 2026-09-30 |
| Status | **Rejected.** Failed 4 of 6 pre-registered tests. |
| Instruments | QQQ primary; SPY cross-market line; IGV identical-rules report. Long one shot per session, flat every night. |
| Data | 2021-10-07 → 2026-09-25, 1,247 sessions, read via `agent-data/mdq.py`. Excluded: 2021-12-31 (no 1-minute tape; return 0). 2025-01-09 not a session. 10 early closes flattened at 13:00. |
| Rules | [research/RULES.md](../research/RULES.md), locked 2026-09-30 (before the first run, committed at `77e5ebe`), sha256 `f48243b68f9e` |
| Code | [research/](../research/) · 4 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Buying QQQ whenever the tape touches the session open minus one prior-day Wilder ATR(14), and holding to the 16:00 close, lost money on every sample slice: **out-of-sample return −4.6%, Sharpe −0.40** (56 trades), full sample −10.7% and Sharpe −0.46 (124 trades), in-sample −0.51, at 1 bp per side. The benchmark QQQ buy-and-hold over the same OOS window returned +55.4% (Sharpe 0.97). The loss is not a costs story: the strategy is negative at **zero** cost (full-sample Sharpe −0.36, −8.4% return; mean gross trade −6.6 bp, [results.json]/[posthoc.json]).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-07 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,247 | 685 | 562 |
| Total return | −10.66% | −6.34% | −4.61% |
| CAGR | −2.25% | −2.38% | −2.10% |
| Annual volatility | 4.69% | 4.49% | 4.93% |
| Sharpe | **−0.46** | **−0.51** | **−0.40** |
| Max drawdown | −17.17% | −9.94% | −10.12% |
| Trades / profit factor | 124 / 0.75 | 68 / 0.72 | 56 / 0.78 |
| Avg net trade | −8.6 bp | −9.2 bp | −7.9 bp |
| *Benchmark Sharpe (max DD)* | 0.74 (−35.6%) | 0.55 (−35.6%) | 0.97 (−24.2%) |

It failed 4 of the 6 acceptance tests written before the first run (§8): OOS Sharpe −0.40 vs ≥ 0.5 required; direction-placebo p = 0.79 vs ≤ 0.05; IS Sharpe −0.51 with 1 of 5 grid cells positive vs > 0 and ≥ 3 of 5; 2 bp cost return −12.8% vs > 0. Lines 5 (SPY OOS Sharpe +0.03) and 6 (56 ≥ 40 trades) passed.

**Why (failure causes).**
1. **No gross edge.** Zero-cost full Sharpe −0.36 and mean gross trade −6.6 bp (§5, §6). The rebound from a full-ATR-intraday dip to the close did not pay before costs, so cost, latency, or split choices cannot rescue it; the next-bar-open fill changes nothing (−0.45 full / −0.41 OOS, §6).
2. **The big-loss tail swamps the small-win head.** By quintile of the day's |open→close|, mean net trade runs +62 bp → −110 bp; the worst 10 trades sum to −17.1% of capital, the best 10 only +16.6% (§7, §11). This is adverse selection, not noise.
3. **Touch days are trending-down days.** On touch sessions QQQ's own open→close averaged −1.88% vs +0.24% on non-touch sessions (§7): the rule buys precisely the sessions that kept going. Consistent with this store's prior results — intraday momentum worked OOS on QQQ (`qqq-intraday-trend`, +0.82) and single-shot fades lost (`qqq-intraday-reversion`, −0.76).
4. **Decay is not the story — it never worked here.** In-sample −0.51 is worse than out-of-sample −0.40 (§5).

**Recommendation.** Do not trade it. The locked rules forbid promoting any of the variants that look better in the breakdowns (§11, *Ideas for a new study*).

## 2. The strategy

### Rules

```
A = Wilder ATR(14) fixed at the close of a strictly earlier session (daily bars, split-adjusted)
B = session_open(09:30) − 1.0 × A                    # the band; known at 09:30:00
At 09:30:00 place one day limit-buy at B for 100% of start-of-day equity.
Fill at the first 1-minute bar whose low ≤ B, price min(B, that bar's open). One fill per session.
Flatten everything at the last 1-minute bar's close (15:59/12:59 NY). Cancel at session end.
Cost 1 bp of notional per side. No stop, no target, no filter, no overnight.
```

- **Why the resting limit at the band:** the band is computable before the market opens once the open prints, so the fill needs no after-the-fact signal — a look-ahead-free limit, already the realistic execution (§6 of the audit in RULES.md).

### How it trades

| | |
|---|---|
| Sessions with a trade | 124 / 1,247 (9.9%) |
| Trades per year | ≈ 25 |
| Time in market (exposure) | 4.8% of 1-minute bars |
| Holding time | median 194 min, mean 188 min |
| Long / short | 124 / 0, PF 0.75 long |
| Win rate | 42.7% (avg loser outweighs avg winner: PF 0.75) |

## 3. Hypothesis and predictions

The claim: on a session that trades a full prior-day ATR below its open, price-insensitive selling (leveraged-fund rebalancing, vol/option hedging) leaves a reversion fee for whoever warehouses it to the close (Cheng & Madhavan 2009; Baltussen et al. 2021; Hendershott & Seasholes 2007). Counter-forces named before the run: adverse selection (Glosten & Milgrom 1985) and this store's own intraday-momentum results.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Early touches rebound more than late ones (≤11:30 vs >14:00 mean net trade) | −10.3 bp vs −29.5 bp: ordering as predicted, both negative | Consistent (ordering), but both groups lose |
| 2. Edge rises with ATR/open quintile | top +17.4 bp vs bottom −6.7 bp: ordering as predicted, overall mean negative | Consistent (ordering) |
| 3. SPY mean net trade positive under identical rules | −0.9 bp (full, 124 trades) | **Not consistent** |

The two consistent predictions establish only that reversion, where it exists, is larger in calmer-into-the-close and higher-vol conditions; the unconditional mean is negative everywhere. **The mechanism is not confirmed.**

## 4. Method

- **Data.** `agent-data/mdq.py`, read-only, split-adjusted; 1-minute bars for signals/fills, daily bars for ATR (a daily close is used only from 16:00 of its session onward). QQQ/SPY have no corporate actions; IGV split-adjusted (5:1, 2024-03-07). Dividends not stored; book flat overnight so no dividend is held. Bar-count check found every QQQ session exactly 390/211; SPY 2026-03-03 has 389 bars (one untraded minute) and IGV has many short sessions — recorded in `results.json/odd_bar_sessions`, treated as complete tape (deviation from the locked hard-check wording, logged in RUNLOG). Benchmark marks: 2 sessions (2026-09-24/25) used last-minute closes.
- **Pre-registration.** RULES.md written and locked before the first return; committed at `77e5ebe` before the first store run. The OOS window is **not unseen data** — six earlier studies report on it (see RULES.md §Prior exposure); nothing known about that window changed any parameter. Pre-lock looks were coverage, structure, and touch counts only. The user chose the open-anchored 1×ATR band and the no-filter, no-stop specification before any look.
- **Fills and costs.** Resting limit at the band; 1 bp/side (wider than QQQ's ~1-tick cost for a small order). Repriced at 0–3 bp; fill at touch-bar-plus-one-open as the conservative variant.
- **Returns.** Daily simple, NYSE calendar, flat days 0; Sharpe = mean/sd×√252.
- **Verification.** 17-case synthetic self-test passes before the store opens (all paths: bar-1 touch, last-bar touch, gap-through fill, missing ATR/tape, early-close flatten, both fill modes, cost identity, first-touch-only). `verify.py` (independent naive re-implementation) replayed **all 1,247 sessions: 124/124 trades match** on side, entry/exit time and price, reason.
- **Runs.** 4 store runs: two crashes before any output (a data-check assertion written too strictly for SPY; a numpy broadcast bug in the placebo); the first complete run (Rejected, headline set above); one rerun solely to add the pre-registered touch-vs-non-touch breakdown the code had omitted — every headline field verified identical. Two verify.py reruns fixed bugs in verify.py itself (a frozen-field crash; an ATR-seeding bug in the independent implementation that made it mismatch). No fix moved any headline number.

## 5. Results

![Growth of $1](figures/equity.svg)

The strategy ends the 5 years below $1 (−10.7%) while buy-and-hold roughly doubles; the IS/OOS split changes nothing about the sign, and the whole gap widens steadily rather than in one episode.

![Drawdown](figures/drawdown.svg)

The strategy's max drawdown (−17.2%) is smaller than the benchmark's only because it is flat 90% of the time; its own drawdowns are pure trading losses.

| Strategy (1 bp/side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary (QQQ)** | −10.66% / −0.46 / −17.17% | −0.51 | −4.61% / −0.40 / −10.12% |
| Fill +1 bar | −10.34% / −0.45 | — | OOS Sharpe −0.41 |
| SPY identical rules | −1.52% / −0.06 | −0.13 | +0.07% / +0.03 / −5.84% |
| IGV identical rules | −21.81% / −1.12 | −1.32 | −7.94% / −0.87 / −12.18% |
| *QQQ buy & hold* | +105.11% / 0.74 / −35.62% | 0.55 | +55.39% / 0.97 / −24.20% |

![Calendar-year return](figures/by_year.svg)

Negative in 5 of 6 calendar years; the one positive year (2026, +4.9%, Sharpe 1.7) is 7 months of an otherwise losing book — ex-2026 the full-sample Sharpe is −0.77 **(post hoc)**.

| Year | Strategy | Sharpe | Max DD | Buy & hold |
|---|---:|---:|---:|---:|
| 2021 (from Oct) | −1.26% | −1.20 | −2.68% | +9.61% |
| 2022 | −2.81% | −0.42 | −6.90% | −33.07% |
| 2023 | −2.32% | −1.13 | −3.24% | +53.79% |
| 2024 | −5.32% | −1.29 | −6.54% | +24.84% |
| 2025 | −4.06% | −0.70 | −5.16% | +20.15% |
| 2026 (to Sep) | +4.92% | 1.72 | −2.06% | +21.20% |

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

Direction placebo (flip each trade's sign at random, 2,000 draws, seed 20260930): actual gross Sharpe −0.36 sits at **p = 0.79** inside the coin-flip null (null mean +0.01) — the entry level/timing pair is indistinguishable from a random side and slightly on the null's bad side. Timing placebo (the 124 entries re-fired at uniformly random minutes, same EOD exits, 500 draws, seed 20260931): actual gross −0.36 vs null mean −2.49, p = 0.002 — the band-touch minute beats a random minute, but the whole comparison sits on the losing side of zero.

### Bootstrap

Circular 20-session block bootstrap, 2,000 draws, seed 20260932: full-sample Sharpe 95% interval **[−1.36, +0.36]**; **85.4%** of draws are ≤ 0. OOS t-stat −0.60.

### Parameter plateau

![Parameter grid](figures/grid.svg)

IS Sharpe by band width K ∈ {0.5, 0.75, 1.0, 1.25, 1.5}: −1.34, −1.21, −0.51, +0.23, −0.03 — **1 of 5 cells positive**, and the primary ranks 3rd of 5 in IS. OOS: all five cells negative (−1.04, −0.92, −0.40, −0.78, −0.52). Nothing was selected from the grid; the K = 1 primary is the user's locked specification.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.36 | −0.41 | **−0.46** | −0.57 | −0.67 |
| Full-sample return | −8.41% | −9.54% | −10.66% | −12.85% | −14.98% |

The break-even cost does not exist: the book is already losing gross money (zero-cost return −8.4%). Filling one bar after the touch instead of at the band leaves the picture unchanged (−0.45 full / −0.41 OOS Sharpe, same 124 trades): execution is not the issue.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|
| QQQ (primary) | −0.51 | −0.40 | −10.7% / 0.75 |
| SPY (line 5) | −0.13 | **+0.03** | −1.5% / 0.96 |
| IGV | −1.32 | −0.87 | −21.8% / 0.49 |

Daily strategy-return correlations: QQQ–SPY 0.80, QQQ–IGV 0.45, SPY–IGV 0.42. SPY's +0.03 OOS Sharpe clears line 5 numerically but is economically nil: OOS return +0.07%, PF 1.00, average net trade +0.5 bp — inside noise (its own 0 bp figure is +0.07 Sharpe).

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Mean net trade by quintile of the session's |open→close|: **+62.4, +24.5, +5.5, −29.3, −110.4 bp**; win rate 100%, 64%, 32%, 12%, 4%. The strategy wins where the day finished near where it opened and loses catastrophically where it kept trending — the dislocation the rule buys is, on its worst days, information in flight. No breakdown here was used to filter the rule.

- **Touch vs non-touch days (pre-registered descriptive):** mean open→close −1.88% on the 124 touch sessions vs +0.24% on the other 1,123 — the entry is a strong filter *for* down days.
- **Entry-time table** (net bp, half-hour buckets) is noisy in every cell (1–15 trades per bucket); early (≤11:30) −10.3 bp vs late (>14:00) −29.5 bp, both negative.
- **By vol quintile** (ATR/open): −6.7, −8.8, −13.9, −30.1, +17.4 bp — non-monotone; prediction 2's ordering holds only head-to-top.
- All 124 trades exit at the session close (single exit rule), by construction.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Actual | |
|---|---|---|---|
| 1. OOS Sharpe / OOS profit factor | ≥ 0.5 / ≥ 1.10 | −0.40 / 0.78 | ❌ |
| 2. Direction placebo p (full, gross) | ≤ 0.05 | 0.79 | ❌ |
| 3. IS Sharpe > 0 and ≥ 3/5 grid cells > 0 | — | −0.51; 1/5 | ❌ |
| 4. Full return at 2 bp cost | > 0 | −12.85% | ❌ |
| 5. SPY OOS Sharpe, identical rules | > 0 | +0.03 | ✅ |
| 6. OOS trades | ≥ 40 | 56 | ✅ |

4 of 6 failed, line 6 holds → **Rejected** (not Inconclusive). What this does not mean: line 5's pass does not make SPY a candidate — it is one identical-rules number, economically zero (§6), and the locked rules forbid promoting it.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| No gross edge | Zero-cost Sharpe −0.36; mean gross −6.6 bp | None available at this band definition |
| Tail/adverse selection | Q5 move-quintile −110 bp/trade, win rate 4%; worst-10 trades −17.1% of capital **(post hoc)** | — |
| Decay / regime coverage | Never worked in any year but 2026; ex-2026 Sharpe −0.77 **(post hoc)**; OOS ex-April-2025 −0.56, worse than with it **(post hoc)** | — |
| Generalization | IGV −1.12, SPY ~0 full-sample | — |
| Sample length | 124 trades, 56 OOS; bootstrap spans [−1.36, +0.36] | — |
| Selection honesty | Grid K=1.25 positive IS but −0.78 OOS; breakdowns not filtered on | Locked rules bar promotion |

## 10. No deployment

There is no paper-trading proposal. The primary failed 4 of the 6 tests written to decide that question. Any different band width, anchor, filter, or exit is a different rule and needs its own RULES.md and data this study has not used.

## 11. Post hoc (not part of the verdict)

- **(post hoc)** Concentration: best 10 trades +16.6%, worst 10 −17.1% of run capital; trimming both 5% tails *worsens* the mean gross (−6.6 → −9.3 bp) — the book's arithmetic is made in its extreme days, in the wrong direction.
- **(post hoc)** Ex-2026 full-sample Sharpe −0.77; 2026 alone +4.9% / 1.72 Sharpe (the best 6 months the book ever had, still tiny vs the market).
- **(post hoc)** OOS excluding April 2025: −0.56 vs −0.40 with it — the crash-rebound month was the strategy's *friend*, unlike in `qqq-intraday-trend`.
- **(post hoc)** Rolling 63-session Sharpe spends nearly the whole sample below zero.
- **(post hoc)** Direction-placebo draws regenerated in `posthoc.py` reproduce results.json's p exactly (seed check).

### Ideas for a new study

- The mirror trade — sell at open + 1×ATR, cover at the close — is the same rule with the opposite sign. It was suggested by this data (the move-quintile and touch-vs-drift tables); it needs its own RULES.md and untouched data. The counter-evidence is already on file: `qqq-atr-scale-in` ran a symmetric fade of half-ATR moves and lost OOS.
- A band anchored at the **prior close** rather than the open, motivated by the observation here that touch days are continuation days — a new pre-registration, not a patch to this one.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-atr-band-dip-eod/research/backtest.py
python research/qqq-atr-band-dip-eod/research/verify.py
python research/qqq-atr-band-dip-eod/research/posthoc.py
python research/qqq-atr-band-dip-eod/research/charts.py
```

`backtest.py` writes results.json, daily.csv, trades.csv and appends a RUNLOG entry (it refuses to run if RULES.md no longer matches RULES.lock); verify.py replays all 1,247 sessions; posthoc.py and charts.py read only the output files. Runtime ≈ 2–5 min (store reads dominate). Seeds 20260930/31/32; reruns on the same store give identical numbers.

### References

- Baltussen, Da, Lammers, Martens (2021). "Hedging Demand and Market Intraday Momentum." *J. Financial Economics*.
- Cheng & Madhavan (2009). "The Dynamics of Leveraged and Inverse Exchange-Traded Funds." *J. Investment Management*.
- Gao, Han, Li, Zhou (2018). "Market Intraday Momentum." *J. Financial Economics*.
- Glosten & Milgrom (1985). *J. Financial Economics*.
- Hendershott & Seasholes (2007). "Market Maker Inventories and Stock Prices." *AER P&P*.
- Wilder (1978). *New Concepts in Technical Trading Systems*. Trend Research.
