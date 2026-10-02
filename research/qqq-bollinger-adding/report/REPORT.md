# QQQ intraday Bollinger reversion with adding

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** Failed 5 of 6 pre-registered tests. The minimum sample was met. |
| Instruments | QQQ, regular hours, 5-minute bars, flat every night. SPY is the cross-market test. IGV is reported under the same rules. |
| Data | 2021-09-27 → 2026-09-25 (1,255 NYSE sessions), read via `agent-data/mdq.py`. 2021-12-31 has no bars and is a zero day. 2025-01-09 is a closure and is not in the calendar. |
| Rules | [`research/qqq-bollinger-adding/research/RULES.md`](../research/RULES.md), locked 2026-09-26 19:09 UTC, sha256 `48b0332c189f` |
| Code | [`research/qqq-bollinger-adding/research/`](../research/) · 1 store run plus 1 verification replay (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Fading a 5-minute QQQ close outside a 20-bar, 2-SD Bollinger band, adding up to two more units as price moved further away, and exiting at the middle band lost money. Out of sample, from 1 July 2024 through 25 September 2026, the account returned **−9.90%** after 1 bp per side (Sharpe **−0.89**, profit factor 0.81, 970 campaigns). Over the full window, 27 September 2021 through 25 September 2026, it returned **−35.14%** (Sharpe **−1.53**). With costs set to zero, the out-of-sample Sharpe was still **−0.08** and the full-sample Sharpe was **−0.75**.

The trades had the shape of classic reversion: 67.3% of campaigns won, and per-unit campaign returns had a skewness of −2.57. The account lost anyway, because the campaigns that added a third unit, and the campaigns still open at the close, lost more than all the middle-band exits made.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-09-27 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,255 | 693 | 562 |
| Total return | **−35.14%** | −28.02% | **−9.90%** |
| CAGR | −8.33% | −11.27% | −4.57% |
| Annual volatility | 5.57% | 5.93% | 5.09% |
| Sharpe | **−1.53** | −1.98 | **−0.89** |
| Max drawdown | −36.72% | −28.79% | −13.28% |
| Trades / profit factor | 2,217 / 0.73 | 1,247 / 0.70 | 970 / 0.81 |
| Avg net trade (per unit) | +3.04 bp | +2.92 bp | +3.20 bp |
| *QQQ buy and hold Sharpe (max DD)* | *0.71 (−35.6%)* | *0.51 (−35.6%)* | *0.97 (−24.2%)* |

QQQ buy and hold returned +101.1% over the full window and +55.4% out of sample. The average campaign is positive per unit because most campaigns are one-unit wins. Dollars are weighted by units, and the three-unit campaigns are the large losers (§7). The strategy failed 5 of the 6 acceptance tests written before the first run (§8). The one it passed was sample size.

**Why.**

1. **There is no gross edge, even before adding.** At zero cost the full-sample return is −19.32% (Sharpe −0.75) and the out-of-sample return is −1.15% (Sharpe −0.08). The first unit of every campaign averaged **−1.42 bp** gross. The no-adding diagnostic D1 returned −22.49% over the full window and −7.22% out of sample (Sharpe −1.18).
2. **Adding bought the worst trades, not the best.** Added legs averaged **−2.90 bp** gross, against −1.42 bp for first legs. One-unit campaigns made +0.446 on starting equity of 1. Two-unit campaigns lost 0.073, and three-unit campaigns, 252 of them, lost 0.725. Prediction 2 fails (§3).
3. **Trend days wipe out the range days.** In the three quietest quintiles of the open-to-close move, the book made +2.40, +1.77, and +2.41 bp a session. In the quintile with the biggest move it lost **−21.04 bp** a session. Campaigns still open at the close, 350 of them, lost 0.777. The 1,867 middle-band exits made 0.425.
4. **The side of the trade is worse than a coin flip.** The full-sample gross Sharpe is −0.75. Flipping each campaign's side at random gave a null mean of 0.00 and a 95th percentile of 0.71: p = 0.954.
5. **The same rule loses on SPY and IGV, everywhere on the grid, and in every full year.** SPY's out-of-sample Sharpe is −1.37 and IGV's is −0.84. None of the 27 in-sample grid cells is above zero. Each calendar year from 2021 through 2025 lost money. The bootstrap puts the full-sample Sharpe between −2.12 and −0.85 at 95%.

**Recommendation.** Do not trade it. The no-adding book, the continuous-band book, the long side, any grid cell, a trend filter, and a stop are diagnostics or new ideas. The rules do not allow any of them to replace the primary.

## 2. The strategy

### Rules

```
Bars: 5-minute, 09:30-anchored, regular hours. Each session starts flat.
M, S = mean and population SD of the last 20 closes of THIS session
       (first band at the 11:10 close)

On a 5-minute close, orders fill at the next bar's open.
Entries and adds must fill before 15:30 (12:30 on a 13:00 close).

  flat,  close < M − 2S                         buy 1 unit
  flat,  close > M + 2S                         short 1 unit
  long,  close >= M                             sell every unit
  long,  units < 3, close < M − 2S,
         and close <= lowest fill − 1·S         buy 1 more unit
  short, the mirror

The last bar flattens anything still open, at its close. No stop.
Each unit is 1/3 of the previous session's equity. 1 bp per side per unit.
```

- **Why Bollinger's defaults.** Length 20 and width 2 population SD are Bollinger's published defaults and the terminal's `CBollinger` defaults. The middle-band exit is the terminal's `bollinger_revert` rule.
- **Why session-local bands.** The book is flat overnight. A window that reached into yesterday's afternoon would fire at 09:35 on any overnight gap, which is a gap fade, a different trade. The cost of this choice is that no trade happens before 11:10. The continuous band the terminal chart draws was run as diagnostic D2 (§6).
- **Why this add rule.** An add needs the close still outside the band and at least one band SD beyond the worst fill, so each add is a clearly further move. The units are equal and capped at three, the same as `qqq-atr-scale-in`, so the two studies can be compared.

### How it trades

| | |
|---|---|
| Sessions with a campaign | 1,219 of 1,254 tradable sessions |
| Campaigns | 2,217 (1,247 in sample, 970 out of sample) |
| Time in market | 32.7% of 5-minute bars |
| Holding time | median 12 bars, mean 14.4 bars (first fill through exit bar) |
| Units | 1,413 one-unit, 552 two-unit, 252 three-unit campaigns; mean 1.48 |
| Long / short | 1,118 long (PF 0.77), 1,099 short (PF 0.69) |
| Win rate | 67.3% of campaigns; per-unit skewness −2.57 |
| Exit reason | 1,867 middle band (PF 1.85), 350 session close (PF 0.02) |

## 3. Hypothesis and predictions

A close two SDs from the recent mean was taken to be a price concession to an impatient trader, which liquidity providers are paid to absorb (Grossman and Miller 1988; Campbell, Grossman and Wang 1993; Nagel 2012). If that were right, a unit added further from the mean would be bought at a larger concession. The counter-forces in `RULES.md` were trends that walk the band (Bollinger 2001), intraday momentum and dealer hedging (Gao, Han, Li and Zhou 2018; Baltussen, Da, Lammers and Martens 2021), and adverse selection on the largest moves (Glosten and Milgrom 1985).

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Quintiles 1–2 of \|open→close\| have a positive mean daily return; quintile 5 is negative | Q1–2 pooled +2.08 bp; Q5 −21.04 bp | **Consistent** |
| 2. Added legs earn more gross per leg than first legs | Added −2.90 bp (1,056 legs); first −1.42 bp | **Not consistent** |
| 3. Win rate above 55% and negative skew | 67.3%; skewness −2.57 | **Consistent** |

Predictions 1 and 3 describe the shape of a fade, and the shape showed up. Prediction 2 is the one that justified adding, and it failed: the further the move, the worse the leg (second legs −2.70 bp, third legs −3.55 bp). That is the adverse-selection counter-force, not the liquidity mechanism. The small gain on range days was more than cancelled by trend days (§7).

## 4. Method

- **Data.** QQQ, SPY, and IGV 1-minute bars, 2021-09-27 → 2026-09-25, resampled to 5 minutes by `mdq.resample`. Each ticker has 1,254 sessions with bars. The script checked that every QQQ session with bars is tradable and has 78 bars, or 43 on an early close. 2021-12-31 (the known calendar gap) is a zero day. IGV has 15 sessions with a missing 5-minute bucket; no order was scheduled across a gap. IGV's 5-for-1 split (2024-03-07) is adjusted by `mdq`. Dividends are not stored, and the book is flat overnight. Two buy-and-hold marks (QQQ 2026-09-24 and 2026-09-25) use the last 5-minute close, because the store has no daily bar for them.
- **Pre-registration.** Before any return was computed, `RULES.md` fixed the bands, the entry, the add rule, the exit, sizing, costs, the split, the grid, the diagnostics, the predictions, and the acceptance lines. The only store looks before the lock were counts of band breaks and bar coverage (`counts.py`). **Prior exposure:** the out-of-sample window was already used by six earlier studies, and I had read their results, including the rejected ATR scale-in fade on these same 5-minute bars. The store also held three terminal backtest ledgers of `bollinger_revert` on QQQ 1-minute bars (20/2, long only) over the out-of-sample window. I did not open their results. `RULES.md` was hash-locked at 19:09 UTC and was **not committed** before the run.
- **Fills and costs.** A signal at a 5-minute close fills at the next bar's open. The session flatten fills at the last bar's close. 1 bp of unit notional per side, on each unit. One cent on QQQ above $300 is under 1 bp, so this is wider than the quoted spread for a small order.
- **Returns.** Daily simple returns on the NYSE calendar. Flat days count as 0. Sharpe = mean ÷ sample SD × √252. Each session's return is the sum of its legs' net returns divided by 3, so the daily series does not depend on the equity path. Profit factor and campaign dollars use the compounded path from equity 1.
- **Verification.** `backtest.py` ran 16 hand-built self-test cases before it opened the store: long and short exits at the middle, two adds then the cap then the session flatten, an add refused by the step, the 15:30 and early-close cutoffs, a missing bucket, no decision before bar 19, a middle-band close on the last bar, exit then re-entry, delay 2, delay 0, and costs on three units. All passed. `verify.py` builds its own 5-minute bars from 1-minute bars, computes the bands in its own loops, and replayed all 1,254 QQQ sessions. It matched **all 2,217 campaigns** on side, units, leg times and prices, exit time, exit price, and reason.
- **Runs.** One store run of `backtest.py` (reason: initial), then the `verify.py` replay. No bug fix came between them, and the headline did not change. Before the run there was one single-session trace of fill times with no P&L. After the run there was one single-session check of the timing-placebo arithmetic, described in §6. No rule was ambiguous enough to need a second reading.

## 5. Results

![Growth of 1](figures/equity.svg)

The strategy falls steadily from the first months and ends at 0.65. The no-adding diagnostic falls more slowly and ends at 0.78. QQQ ends at 2.01. The dashed line is 1 July 2024. The losses come in steps on large-move days. **(post hoc)** The worst session was 2025-04-09 (−4.45%), when QQQ rose 12.14% from open to close.

![Drawdown](figures/drawdown.svg)

The strategy's drawdown reaches −36.7% and does not recover. QQQ's drawdown is just as deep (−35.6% in 2022), but QQQ recovers from it.

| Strategy (1 bp) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | −35.14% / −1.53 / −36.7% | −1.98 | −9.90% / −0.89 / −13.3% |
| D1: no adding (diagnostic) | −22.49% / −1.71 / −23.1% | −2.10 | −7.22% / −1.18 / −8.5% |
| D2: continuous bands (diagnostic) | −47.92% / −1.77 / −49.2% | −2.16 | −17.81% / −1.24 / −19.8% |
| *QQQ buy and hold* | *+101.1% / 0.71 / −35.6%* | *0.51* | *+55.4% / 0.97 / −24.2%* |
| *QQQ open to close* | *+29.8% / 0.37 / −23.7%* | *0.49* | *+4.6% / 0.20 / −20.4%* |

The daily correlation with QQQ buy and hold is −0.04.

![Calendar-year return](figures/by_year.svg)

| Year | Sessions | Strategy | Sharpe | Max DD | QQQ buy and hold |
|---|---:|---:|---:|---:|---:|
| 2021 (from 27 Sep) | 68 | −5.73% | −3.39 | −6.2% | +7.5% |
| 2022 | 251 | −10.05% | −1.41 | −11.5% | −33.1% |
| 2023 | 250 | −9.62% | −2.33 | −10.4% | +53.8% |
| 2024 | 252 | −7.01% | −1.30 | −7.6% | +24.8% |
| 2025 | 250 | −9.89% | −1.70 | −11.2% | +20.2% |
| 2026 (to 25 Sep) | 184 | +0.99% | 0.53 | −1.2% | +21.2% |

Every full year lost money, in a falling market (2022) and in rising ones. The partial year 2026 is the only positive one, at +0.99%.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

**Direction.** Each campaign keeps its timing and has its gross return multiplied by an independent ±1. Over 2,000 draws the null mean is 0.00 and the 95th percentile is 0.71. The actual gross Sharpe is −0.75, in the left tail: p = 0.954. The side the band picks is worse than a coin flip.

**Timing** (reported, not an acceptance line). Each campaign keeps its side, leg offsets, and holding offset, and starts at a random legal bar. The null mean is −4.03 and its 95th percentile is −3.41. The actual −0.75 is above all 500 draws: p = 0.002. This does **not** show a timing edge. The placebo keeps the fade's side, which usually runs against the day's move. **(post hoc)** 62.6% of campaigns were against the day's open-to-close direction. Moving such a trade to a random time gives up the band's better entry and takes more of the day's trend. The null is therefore biased strongly negative, and beating it only means the band enters at a better price than a random time *given the wrong side*. I checked this was not a bug: for single sessions, placing the placebo at the actual start reproduces the actual leg returns exactly. The rule as locked is reported. The design flaw is noted in §9.

### Bootstrap

A circular block bootstrap (20-session blocks, 2,000 draws) puts the full-sample Sharpe between **−2.12 and −0.85** at 95%. Every draw was at or below zero. The out-of-sample t-statistic of the mean daily return is −1.33. The full-sample t-statistic is −3.42.

### Parameter plateau

![Parameter grid](figures/grid.svg)

**0 of 27** in-sample cells have Sharpe > 0; the line needed 17. The primary ranks 18th of 27 in sample. The best in-sample cell (N 10, K 2.5, step 0.5) had in-sample Sharpe −0.60 and out-of-sample −0.98. In-sample and out-of-sample ranks correlate 0.73, so the ordering is stable, and every cell sits in the negative quadrant. Nothing was selected from the grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.75 | −1.14 | **−1.53** | −2.31 | −3.08 |
| OOS Sharpe | −0.08 | −0.48 | **−0.89** | −1.70 | −2.50 |
| Full-sample return | −19.32% | −27.66% | **−35.14%** | −47.86% | −58.09% |

There is no break-even cost: the result is negative at zero. With one extra bar of delay the out-of-sample Sharpe is −1.09 (full −1.47). The same-bar-close upper bound gives −0.87 out of sample (full −1.53). Latency is not the cause.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF | OOS gross Sharpe |
|---|---:|---:|---|---:|
| **QQQ** | −1.98 | −0.89 | −35.14% / 0.73 | −0.08 |
| SPY | −2.62 | −1.37 | −38.71% / 0.64 | −0.44 |
| IGV | −1.90 | −0.84 | −35.58% / 0.73 | −0.13 |

Daily strategy returns correlate 0.86 (QQQ–SPY), 0.79 (QQQ–IGV), and 0.73 (SPY–IGV). The three are one bet on intraday reversion in large US equity ETFs, and it lost in all three.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

| Quintile of \|open→close\| | Median \|move\| | Sessions | Campaigns | Mean net / session | Mean gross / session |
|---|---:|---:|---:|---:|---:|
| Q1 | 13 bp | 251 | 441 | +2.40 bp | +4.04 bp |
| Q2 | 37 bp | 251 | 463 | +1.77 bp | +3.50 bp |
| Q3 | 66 bp | 251 | 443 | +2.41 bp | +4.10 bp |
| Q4 | 109 bp | 251 | 433 | −2.56 bp | −0.86 bp |
| Q5 | 181 bp | 250 | 437 | −21.04 bp | −19.10 bp |

Summed over its sessions, Q5's daily returns total −0.526. Q1, Q2, and Q3 total +0.060, +0.044, and +0.061.

![Net P&L by units](figures/units.svg)

| Units | Campaigns | Net P&L (equity 1) | PF | Win rate | Avg net / unit |
|---|---:|---:|---:|---:|---:|
| 1 | 1,413 | +0.446 | 3.97 | 79.3% | +12.16 bp |
| 2 | 552 | −0.073 | 0.80 | 56.5% | −2.47 bp |
| 3 | 252 | −0.725 | 0.07 | 23.0% | −35.98 bp |

Out of sample the pattern is the same: one-unit +0.143, two-unit −0.021, three-unit −0.193.

- **Side.** Long: 1,118 campaigns, −0.156, PF 0.77. Short: 1,099 campaigns, −0.195, PF 0.69. Out of sample, longs were PF 0.92 and shorts 0.66. Both sides lost.
- **Exit reason.** Middle band: 1,867 campaigns, +0.425, PF 1.85, win rate 77.7%. Session close: 350 campaigns, −0.777, PF 0.02, win rate 11.4%. **(post hoc)** 368 of the 1,867 middle-band exits (19.7%) were gross losses: the middle band had moved to the price rather than the price back to the band.
- **Hour of first fill.** Every hour from 11:00 through 15:00 lost net dollars. The 13:00 hour lost most (−0.159, PF 0.57).
- None of these breakdowns was used to filter the rule.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS Sharpe and profit factor | ≥ 0.5 and ≥ 1.10 | −0.89 and 0.81 | ❌ |
| 2. Direction placebo, full-sample gross | p ≤ 0.05 | p = 0.954 | ❌ |
| 3. IS Sharpe and grid plateau | > 0, and ≥ 17 of 27 cells > 0 | −1.98, and 0 of 27 | ❌ |
| 4. Full-sample return at 2 bp | > 0 | −47.86% | ❌ |
| 5. SPY OOS Sharpe, identical rules | > 0 | −1.37 | ❌ |
| 6. OOS QQQ campaigns | ≥ 100 | 970 | ✅ |

Line 6 held, so the status is **Rejected**, not Inconclusive. The failures are not near their thresholds: every Sharpe and return in lines 1–5 is negative, and the profit factor is below 1.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

The trailing 252-session Sharpe **(post hoc)** was positive in 2.9% of windows. Its maximum was 0.28. The trailing year from 2025-09-25 returned −0.60% (Sharpe −0.20).

| Risk | Evidence | Note |
|---|---|---|
| Loss concentrated in trend days | Q5 −21.0 bp/session. **(post hoc)** Zeroing the 20 worst days moves the full-sample Sharpe from −1.53 to −0.23 and the return to −4.60%; still negative | A filter that skips trend days is not knowable in advance, and it is ruled out here |
| No gross edge | Zero-cost Sharpe −0.75 full, −0.08 OOS; first-leg mean −1.42 bp | Cheaper execution cannot fix it |
| Adding amplifies the tail | 252 three-unit campaigns lost 0.725 | Prediction 2 failed |
| Generalization | SPY −1.37, IGV −0.84 OOS; correlations 0.73–0.86 | One factor, not three tests |
| Overlap with earlier studies | **(post hoc)** Daily correlation +0.52 with `qqq-atr-scale-in` and −0.41 with `qqq-intraday-trend` P1 | This is largely the inverse of the intraday momentum that did pay on QQQ |
| Timing placebo design | Keeping the fade side gives a biased null (§6) | A fairer null would also randomize the side. That is a note for future studies, not a change to this one |
| Exposed out-of-sample window | Six earlier studies used it; terminal ledgers of a related rule exist | Pushes toward a false pass, not a false fail. The result failed anyway |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A different rule, or later data, would be a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

All from `posthoc.py` → `posthoc.json`. None of it changes §8.

- **(post hoc) The fade usually runs against the day.** 1,387 of 2,217 campaigns (62.6%) took the side opposite the session's open-to-close move. They averaged −2.11 bp gross per unit, carried 1.57 units on average, included 282 of the 350 session-close exits, and lost 0.724. The 830 campaigns on the day's side averaged +16.99 bp and made 0.373. This split uses the day's close, which is not known at entry, so it is not a rule.
- **(post hoc) Extreme days.** Zeroing the 20 worst sessions: Sharpe −0.23, return −4.60%. Zeroing the 20 best sessions: Sharpe −2.08, return −43.13%.
- **(post hoc) Rolling Sharpe.** Minimum −2.60, maximum 0.28, positive in 2.9% of 252-session windows.
- **(post hoc) Relation to earlier studies.** On shared sessions, the daily return correlates −0.41 with `qqq-intraday-trend` P1 and +0.52 with `qqq-atr-scale-in`. The band fade is a close relative of the rejected open-anchored fade, and roughly the opposite side of the intraday momentum trade that did pay on QQQ.

### Ideas for a new study

None is proposed. The obvious rescues (skip trend days, keep only one unit, go long only, stop out) either use information the rule does not have at entry or were already negative here (D1 lost 22.49%). Intraday momentum on QQQ, which this result mirrors, already has its own study.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-bollinger-adding/research/counts.py
```

```bash
python research/qqq-bollinger-adding/research/backtest.py
```

```bash
python research/qqq-bollinger-adding/research/verify.py
```

```bash
python research/qqq-bollinger-adding/research/posthoc.py
```

```bash
python research/qqq-bollinger-adding/research/charts.py
```

`counts.py` writes `counts.json` (the pre-lock counts). `backtest.py` checks the rules hash, runs the self-test, then writes `results.json`, `daily.csv`, `trades.csv`, and `placebo_draws.npy`, and appends to `RUNLOG.md`. It takes about 10 seconds. `verify.py` exits non-zero on any mismatch. `posthoc.py` writes `posthoc.json` and `rolling_sharpe.csv`. `charts.py` writes `../report/figures/*.svg`. Seeds: 20260926 (direction placebo), 20260927 (timing placebo), 20260928 (bootstrap). Reruns on the same store give identical numbers, but each rerun of `backtest.py` appends a RUNLOG entry, and a rerun needs a permitted reason.

### References

- Baltussen, G., Z. Da, S. Lammers, and M. Martens (2021). Hedging Demand and Market Intraday Momentum. *Journal of Financial Economics* 142(1).
- Bollinger, J. (2001). *Bollinger on Bollinger Bands*. McGraw-Hill.
- Campbell, J. Y., S. J. Grossman, and J. Wang (1993). Trading Volume and Serial Correlation in Stock Returns. *Quarterly Journal of Economics* 108(4).
- Gao, L., Y. Han, S. Z. Li, and G. Zhou (2018). Market Intraday Momentum. *Journal of Financial Economics* 129(2).
- Glosten, L. R., and P. R. Milgrom (1985). Bid, Ask and Transaction Prices in a Specialist Market with Heterogeneously Informed Traders. *Journal of Financial Economics* 14(1).
- Grossman, S. J., and M. H. Miller (1988). Liquidity and Market Structure. *Journal of Finance* 43(3).
- Nagel, S. (2012). Evaporating Liquidity. *Review of Financial Studies* 25(7).
