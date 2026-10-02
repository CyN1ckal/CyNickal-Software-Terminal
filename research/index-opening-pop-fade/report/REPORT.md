# Index opening-pop fade: SPY, with QQQ and IGV

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** Failed 6 of 7 pre-registered tests. The minimum sample was met. The secondary (S1) failed 5 of 7. |
| Instruments | SPY (primary), QQQ (cross-market), IGV (reported). Regular hours, short only, entered at 10:00 and covered at the close. Nothing held overnight. |
| Data | 1-minute bars, 2021-09-27 → 2026-09-25, read via `agent-data/mdq.py`. Evaluated 2021-12-21 → 2026-09-25 (1,195 sessions). 2021-12-31 has no bars and is a zero day. 2025-01-09 is a closure and is not in the calendar. |
| Rules | [`research/index-opening-pop-fade/research/RULES.md`](../research/RULES.md), locked 2026-09-27 00:55 UTC, sha256 `a72202a2dac0` |
| Code | [`research/index-opening-pop-fade/research/`](../research/) · 1 store run plus 1 verification replay (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The request was a strategy that sells into intraday pops on stock indices. The study tested the published form of that idea: short SPY at 10:00 when its first 30 minutes rose at least one trailing RMS unit, and cover at the close. It lost money. Out of sample, from 1 July 2024 through 25 September 2026, it returned **−11.96%** after 1 bp per side (Sharpe **−0.66**, profit factor 0.65, 82 trades). Over the full window it returned **−20.17%** (Sharpe **−0.61**). At zero cost the out-of-sample Sharpe was still **−0.57**. SPY buy and hold returned +41.7% out of sample and +69.5% over the full window.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-12-21 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,195 | 633 | 562 |
| Total return | **−20.17%** | −9.33% | **−11.96%** |
| CAGR | −4.64% | −3.82% | −5.55% |
| Annual volatility | 7.31% | 6.47% | 8.16% |
| Sharpe | **−0.61** | −0.57 | **−0.66** |
| Max drawdown | −26.49% | −15.88% | −17.10% |
| Trades / profit factor | 178 / 0.72 | 96 / 0.78 | 82 / 0.65 |
| Avg net trade | −11.93 bp | −9.65 bp | −14.58 bp |
| *SPY buy and hold Sharpe (max DD)* | *0.73 (−25.4%)* | *0.49 (−25.4%)* | *1.03 (−19.0%)* |

It failed 6 of the 7 acceptance tests written before the first run (§8). The one it passed was the sample size.

**Why.**

1. **There is no gross edge.** The average trade lost **−9.92 bp** before costs. At zero cost the full-sample Sharpe is −0.51 and the out-of-sample Sharpe is −0.57. A tighter spread would not have made it positive.
2. **Pop days were worse days to short than random days.** Shorting SPY from 10:00 to the close on *every* session had a full-sample Sharpe of −0.30. The pop filter's gross Sharpe was −0.51. Shorting the same number of randomly chosen sessions beat the actual book in 83% of 2,000 draws (timing placebo p = 0.830). The direction placebo gives p = 0.877.
3. **Bigger pops lost more, the opposite of the mechanism.** Pops of 1.0–1.5 RMS averaged +4.27 bp gross over 101 trades. Pops of 1.5 RMS or more averaged **−28.54 bp** over 77 trades. The reversal to noon was negative (−1.47 bp), and the last half hour also went against the short (−0.34 bp). All five pre-registered predictions failed (§3).
4. **The losses come from rebound and squeeze days.** Pops on days that opened at or below the prior close (88 trades) averaged −25.22 bp net. Pops on days that opened above it (90 trades) averaged +1.07 bp. Pops with SPY below its 50-day average averaged −27.24 bp. **(post hoc)** The worst trade was 9 April 2025 at **−978 bp**, the day the tariff pause was announced. The 10 worst trades summed to −0.281 on a per-trade basis, more than the whole book's −0.212. None of these splits was used as a filter.
5. **The same rule loses on QQQ and IGV, everywhere on the out-of-sample grid, and in four of five full years.** QQQ's out-of-sample Sharpe is **−1.27** (daily correlation with SPY 0.83). IGV's is −0.73. All 15 out-of-sample grid cells are negative. Only 3 of the 15 in-sample cells are above zero, all at Z = 2.0 with 11 to 15 trades. The bootstrap puts the full-sample Sharpe between −1.44 and +0.36 at 95%.

**S1, the secondary.** S1 measures the pop from the prior close, so it includes the overnight gap. It made money: +4.49% out of sample (Sharpe 0.38, PF 1.22, 75 trades) and +8.38% over the full sample (Sharpe 0.33). It failed 5 of its 7 lines: OOS Sharpe 0.38 against a 0.5 bar, direction placebo p = 0.158, timing placebo p = 0.105, 8 of 15 in-sample grid cells positive against 9 required, and QQQ out-of-sample Sharpe −0.51. **(post hoc)** Zeroing its 20 best sessions turns it to −26.7%. It is rejected, and the rules never let a secondary replace a failed primary.

**Recommendation.** Do not trade it. S1, the gap-up subset, the above-50-day-average subset, the Z = 2.0 cells, the mirror long, a stop, and a midday exit are diagnostics. The rules do not allow any of them to replace the primary.

## 2. The strategy

### Rules

```
For each SPY session d (regular hours, 1-minute bars):
  O  = open of the 09:30 bar
  P  = close of the last bar before 10:00 (normally 09:59), known at 10:00:00
  pop = P / O − 1
  R  = sqrt(mean of pop² over the 60 prior sessions that have a pop)

  if pop >= 1.0 × R:
      short 100% of equity at the open of the first bar at or after 10:00
      cover at the close of the session's last bar (15:59, or 13:00 on an early close)

  No stop, no target, no re-entry, no longs. 1 bp of notional per side.
S1: identical, with pop = P / (prior session's last close) − 1 and its own R.
```

- **Why 10:00 and hold to the close:** the opening-reversal literature measures a large move over the opening period and the reversal over the rest of the day (Fung, Mok & Lam 2000; Grant, Wolf & Yu 2005). The first half hour is also the window Gao et al. (2018) use for the opposing momentum effect, so both forces are measured on the same window.
- **Why 1.0 RMS over 60 sessions:** fixed a priori as the smallest conventional "large move", and a lookback that follows volatility regimes. The grid in §6 shows its neighbours.
- **Why SPY first:** the published evidence is on S&P 500 index futures.

### How it trades

| | |
|---|---|
| Sessions with a trade | 178 of 1,195 (14.9%) |
| Time in market | 13.8% of regular-hours minutes |
| Holding time | fixed: 10:00 → close (360 minutes, or 181 on an early close) |
| Long / short | 0 / 178. Short PF 0.72 |
| Win rate | 40.4% |
| Exit reason | session close, 178 of 178 |

## 3. Hypothesis and predictions

Retail and attention-driven orders that build up overnight execute at the open (Berkman, Koch, Tuttle & Zhang 2012). If intermediaries absorb that imbalance, a large opening rise should overshoot and revert over the day. Fung, Mok & Lam (2000) and Grant, Wolf & Yu (2005) report such reversals in S&P 500 futures from 1987 to 2002, stronger after up moves and small after costs. The opposing force is intraday momentum (Gao, Han, Li & Zhou 2018; Baltussen, Da, Lammers & Martens 2021).

| Pre-registered prediction | Evidence (SPY, full sample, gross) | Scored |
|---|---|---|
| 1. Pops ≥ 1.5 RMS revert more than 1.0–1.5 RMS | −28.54 bp (77 trades) vs +4.27 bp (101 trades) | Not consistent |
| 2. Fade-signed return from entry to the 11:59 close > 0 | −1.47 bp (178 trades) | Not consistent |
| 3. Short on pops beats the mirror long on drops | −9.92 bp vs +3.82 bp | Not consistent |
| 4. Timing placebo p ≤ 0.05 | p = 0.830 | Not consistent |
| 5. Fade-signed last half hour ≥ 0 | −0.34 bp (178 trades) | Not consistent |

No part of the mechanism showed up on SPY from 2021 to 2026. The sign of prediction 1 is reversed: the larger the opening rise, the worse the short did, which fits momentum better than overreaction. The prediction-5 result is small, but its sign is the one Gao et al. predict.

For S1, predictions 1, 2, 3, and 5 were consistent (+12.98 vs +2.61 bp; +8.55 bp to noon; +7.32 vs +5.67 bp; +1.79 bp in the last half hour), and prediction 4 was not (p = 0.105). S1's P&L did not pass its acceptance table, so this does not confirm the mechanism either.

## 4. Method

- **Data.** SPY, QQQ, and IGV 1-minute regular-hours bars. `backtest.py` checks that every SPY and QQQ session starts at 09:30, ends at 15:59 (13:00 on the ten early closes), and that 2021-12-31 is the only calendar session without bars. All checks passed. No splits in SPY or QQQ. IGV's 5:1 split on 2024-03-07 is adjusted by mdq. Dividends are not adjusted. They fall at the open and do not enter a 10:00 → close return. The SPY buy-and-hold benchmark therefore excludes dividends.
- **Pre-registration.** `RULES.md` fixed the rule, parameters, samples, costs, every check, and the acceptance table before any return was computed. The only look at the store before the lock was `counts.py`: bar presence and how often the signal fired. **Prior exposure:** every earlier study used the same out-of-sample window, and I had read them. They showed QQQ intraday fades losing and QQQ 10:00 momentum working (`qqq-intraday-trend`), so the out-of-sample window was not unseen, and the prior evidence pointed against this hypothesis. `RULES.md` was **not** committed to git before the first run. The lock file records 00:55:11 UTC. The first store run was at 00:58:45 UTC.
- **Fills and costs.** Entry at the 10:00 bar's open, a signal at the 09:59 close filled one bar later. Exit at the last bar's close as a proxy for the closing auction. 1 bp of notional per side for SPY and QQQ, 2 bp for IGV. No borrow fee, because nothing is held overnight.
- **Returns.** Daily simple returns on the NYSE calendar, flat days = 0, Sharpe = mean ÷ sample SD × √252.
- **Look-ahead.** The audit table is in `RULES.md`. R uses only prior sessions. The pop uses bars before 10:00. The fill is the next bar's open. No statistic uses the full sample.
- **Verification.** The self-test in `backtest.py` builds ten synthetic sessions by hand. It covers the warm-up, a trade with hand-computed gross and net returns, a non-signal, a missing 09:30 bar, missing 09:59 and 10:00 bars, an early-close exit, a session with no bar after 10:00, a zero day, the delay and upper-bound fills, the mirror long, the gap-inclusive pop, zero cost, and a 09:45 grid cell. The first two drafts failed on errors in the tests' own expected values, which were fixed before any store run. `verify.py` is an independent implementation that uses UTC timestamps, NumPy, and a cumulative-sum RMS. It replayed every SPY and QQQ session and matched all **720 trades** (primary SPY 178, QQQ 189; S1 SPY 165, QQQ 188) on session, side, entry time and price, exit time and price, and net return.
- **Runs.** One backtest run and one verification replay. No reruns and no bug fixes after the first store run. The headline numbers never changed.
- **A literal reading to disclose.** Prediction 5 uses "the last bar before 15:30". On an early close that bar is the 13:00 closing bar, so those trades contribute a zero last-half-hour segment. It does not affect the P&L or the verdict.

## 5. Results

![Growth of $1](figures/equity.svg)

The primary (blue) drifts down across both windows and loses its largest step in April 2025. It ends at 0.80, almost exactly where shorting SPY from 10:00 to the close every day ends (0.79). S1 (orange) ends at 1.08. SPY buy and hold ends at 1.70.

![Drawdown](figures/drawdown.svg)

The primary's drawdown reaches −26.5%, deeper than SPY's −25.4% over the same window, while it is in the market 14.9% of sessions.

| Strategy (1 bp per side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | −20.17% / −0.61 / −26.49% | −0.57 | −11.96% / −0.66 / −17.10% |
| S1 (secondary) | +8.38% / 0.33 / −10.99% | 0.29 | +4.49% / 0.38 / −3.94% |
| *Short 10:00 → close every day (gross)* | *−20.54% / −0.30 / −32.19%* | *−0.33* | *−8.65% / −0.26 / −21.22%* |
| *Mirror long on drops (1 bp, diagnostic)* | *+2.60% / 0.13* | *0.34* | *−2.10% / −0.19* |
| *SPY buy and hold* | *+69.53% / 0.73 / −25.35%* | *0.49* | *+41.72% / 1.03 / −18.98%* |

![Calendar-year return](figures/by_year.svg)

| Year | Trades | Primary | Sharpe | Max DD | S1 | SPY buy and hold |
|---|---:|---:|---:|---:|---:|---:|
| 2021 (8 sessions) | 0 | 0.00% | — | 0.00% | 0.00% | +4.67% |
| 2022 | 46 | −5.06% | −0.52 | −11.00% | +0.56% | −19.71% |
| 2023 | 37 | −4.09% | −1.04 | −5.80% | +0.87% | +24.33% |
| 2024 | 31 | +2.60% | 0.62 | −2.04% | +2.07% | +23.28% |
| 2025 | 36 | −12.50% | −1.16 | −16.89% | +6.89% | +16.34% |
| 2026 (to 25 Sep) | 28 | −2.34% | −0.73 | −3.94% | −2.07% | +13.12% |

The primary lost in 2022, when SPY fell 19.7%. A short-only rule that cannot make money in a bear market is not being held back by the index's drift.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

Direction placebo: each trade's gross return times a random ±1, 2,000 draws, seed 20260926. Null mean 0.01, 95th percentile 0.79, actual gross Sharpe −0.51, **p = 0.877**. Timing placebo: shorting 178 randomly chosen evaluable sessions from 10:00 to the close, 2,000 draws, seed 20260927. Null mean −0.11, 95th percentile 0.57, actual −0.51, **p = 0.830**. The random-session null is below zero because SPY rose from 10:00 to the close on average. The pop sessions did worse than that.

### Bootstrap

Circular block bootstrap, 20-session blocks, 2,000 draws, seed 20260928: the 95% interval for the full-sample Sharpe is **−1.44 to +0.36**. The t-statistic of the mean daily return is −1.33 over the full sample and −0.98 out of sample. The loss is not statistically distinguishable from zero. That supports "no edge", not "a reliable negative edge".

### Parameter plateau

![Parameter grid](figures/grid.svg)

Three of 15 in-sample cells are above zero (0.05, 0.10, and 0.21), all at Z = 2.0, each with 11 to 15 in-sample trades. The locked cell (10:00, 1.0) is −0.57. All 15 out-of-sample cells are negative, from −0.52 to −0.96. Picking the best in-sample cell (10:30, Z = 2.0, in-sample 0.21) would have given −0.79 out of sample. For S1, 8 of 15 in-sample cells are positive, and the out-of-sample panel is positive in 12 of 15. Nothing was selected from either grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.51 | −0.56 | **−0.61** | −0.71 | −0.81 |
| OOS Sharpe | −0.57 | −0.61 | **−0.66** | −0.75 | −0.83 |
| Full-sample return | −17.27% | −18.74% | **−20.17%** | −22.97% | −25.67% |

There is no break-even cost: the rule loses at zero cost. Filling one minute later (10:01 open) gives an out-of-sample Sharpe of −0.61. Filling at the 09:59 close, the labelled upper bound, gives −0.65. Latency is not the problem.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF | Daily corr. with SPY |
|---|---:|---:|---|---:|
| QQQ (1 bp) | −0.83 | **−1.27** | −36.19% / 0.59 | 0.83 |
| IGV (2 bp, reported) | −0.78 | −0.73 | −29.97% / 0.70 | 0.67 |
| S1 on QQQ | −0.41 | −0.51 | −18.55% / 0.79 | 0.67 |

QQQ is the worst of the three. That matches `qqq-intraday-trend`, where QQQ momentum after 10:00 was profitable.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

| Quintile of SPY open → close | Range | Sessions | Trades | Mean strategy day | Sum of net |
|---|---|---:|---:|---:|---:|
| Q1 | −486 to −54 bp | 238 | 20 | +14.05 bp | +0.334 |
| Q2 | −53 to −9 bp | 239 | 20 | +5.79 bp | +0.138 |
| Q3 | −9 to +22 bp | 239 | 21 | +2.78 bp | +0.067 |
| Q4 | +22 to +61 bp | 239 | 36 | −1.30 bp | −0.031 |
| Q5 | +61 to +1,118 bp | 239 | 81 | −30.14 bp | −0.720 |

A short makes money on down days, so the sign across quintiles is mechanical. What the table shows is where the trades land: **81 of 178 trades fell on SPY's strongest-quintile days.** An opening pop tended to be the first half hour of a strong up day, not an overshoot.

| Split (not used as a filter) | Trades | Avg net | Profit factor |
|---|---:|---:|---:|
| Pop 1.0–1.5 RMS | 101 | +2.27 bp | 1.07 |
| Pop 1.5–2.0 RMS | 48 | −28.30 bp | 0.35 |
| Pop ≥ 2.0 RMS | 29 | −34.25 bp | 0.57 |
| Opened above the prior close | 90 | +1.07 bp | 1.04 |
| Opened at or below the prior close | 88 | −25.22 bp | 0.55 |
| Prior close above its 50-day average | 87 | +4.09 bp | 1.19 |
| Prior close below its 50-day average | 91 | −27.24 bp | 0.57 |

Side and exit reason each have one value (short, session close). The splits describe the loss. Intraday rebounds after a gap down, and rallies in a falling market, kept going. None of these splits was used to filter the rule, and none may be promoted from this data.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS Sharpe and profit factor | ≥ 0.5 and ≥ 1.10 | −0.66 and 0.65 | ❌ |
| 2. Direction placebo, full sample | p ≤ 0.05 | p = 0.877 | ❌ |
| 3. IS Sharpe; IS grid cells > 0 | > 0; ≥ 9 of 15 | −0.57; 3 of 15 | ❌ |
| 4. Full-sample return at 2 bp per side | > 0 | −22.97% | ❌ |
| 5. QQQ OOS Sharpe, identical rules | > 0 | −1.27 | ❌ |
| 6. Timing placebo, full sample | p ≤ 0.05 | p = 0.830 | ❌ |
| 7. Minimum OOS trades | ≥ 60 | 82 | ✅ |

**S1 (secondary, never promotable over a failed primary):** 1 ❌ (0.38, PF 1.22) · 2 ❌ (p = 0.158) · 3 ❌ (IS 0.29; 8 of 15) · 4 ✅ (+4.86%) · 5 ❌ (QQQ −0.51) · 6 ❌ (p = 0.105) · 7 ✅ (75).

Line 7 passing means the verdict is Rejected, not Inconclusive.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

**(post hoc)** The primary's trailing 252-session Sharpe was positive in 26.2% of windows, with a low of −2.32 and a high of 1.04. The trailing year to 25 September 2026 was +2.01% (Sharpe 0.41). QQQ's trailing year was −8.49% (Sharpe −1.54).

| Risk | Evidence | Note |
|---|---|---|
| Squeeze days | **(post hoc)** Worst trade −978 bp on 9 April 2025. The 10 worst trades sum to −0.281 against a book total of −0.212. Zeroing the 20 worst days gives +19.1% (Sharpe 0.82). Zeroing the 20 best gives −43.2% (Sharpe −1.89) | An uncapped short on an index can lose a year's expected edge in one session. The study had no stop, as pre-registered |
| No gross edge | Zero-cost Sharpe −0.51 full, −0.57 OOS | Costs are not the cause |
| Generalization | QQQ −1.27 and IGV −0.73 out of sample | Fails on every instrument tested |
| Exposed test window | Every earlier study used the same OOS window, and I had read them | The negative result is still informative, because the prior evidence pointed the same way |
| Short sample | 178 trades. The bootstrap interval of −1.44 to +0.36 includes zero | The data cannot rule out a small positive edge. It rules out the edge the acceptance table asked for |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A different rule or later data is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

All from `posthoc.json`.

- **(post hoc)** Rolling and trailing Sharpe: §9.
- **(post hoc)** Concentration: the primary's worst sessions are 2025-04-09 (−978 bp), 2022-10-13 (−398 bp, the CPI-day reversal), 2022-02-24 (−297 bp, the day Russia invaded Ukraine), 2023-03-16 (−193 bp), and 2025-04-07 (−178 bp). Every one of them opened at or below the prior close. S1's best session was 2025-04-08 (+498 bp), and its 20 best sessions carry more than all of its profit (zeroed: −26.7%).
- **(post hoc)** Overlap: the primary and S1 traded the same session 65 times (sum of net +0.015). The primary's 113 sessions that S1 did not trade summed to −0.227. S1's 100 sessions that the primary did not trade summed to +0.073. The primary's loss sits in pops that started from a gap down, which S1 by construction almost never trades (164 of 165 S1 trades opened above the prior close).

### Ideas for a new study

- **Shorting gap-up opening extensions** (S1's form, or the primary restricted to gap-up opens) was suggested by this data. S1 was pre-registered and failed. The gap-up split of the primary was found by looking here. Either would need its own `RULES.md` and data after 25 September 2026 (paper trading), and would still face the squeeze-day tail in §9.

## 12. Reproduce

From the repo root:

```bash
python research/index-opening-pop-fade/research/counts.py
```

```bash
python research/index-opening-pop-fade/research/backtest.py --reason "reproduce"
```

```bash
python research/index-opening-pop-fade/research/verify.py
```

```bash
python research/index-opening-pop-fade/research/posthoc.py
```

```bash
python research/index-opening-pop-fade/research/charts.py
```

`counts.py` writes `counts.json` (pre-lock counts only). `backtest.py` runs the self-test, checks the rules hash against `RULES.lock`, and writes `results.json`, `daily.csv`, `trades.csv`, and a `RUNLOG.md` entry, in about 10 seconds. `verify.py` replays every SPY and QQQ session and exits non-zero on any mismatch. `posthoc.py` writes `posthoc.json`. `charts.py` writes `report/figures/*.svg` from those files. Seeds: direction 20260926, timing 20260927, bootstrap 20260928. Reruns on the same store give identical numbers.

### References

- Baltussen, G., Da, Z., Lammers, S., & Martens, M. (2021). Hedging demand and market intraday momentum. *Journal of Financial Economics*, 142(1), 377–403.
- Berkman, H., Koch, P. D., Tuttle, L., & Zhang, Y. J. (2012). Paying attention: Overnight returns and the hidden cost of buying at the open. *Journal of Financial and Quantitative Analysis*, 47(4), 715–741.
- Cliff, M., Cooper, M. J., & Gulen, H. (2008). Return differences between trading and non-trading hours: Like night and day. SSRN 1004081.
- Fung, A. K.-W., Mok, D. M. Y., & Lam, K. (2000). Intraday price reversals for index futures in the US and Hong Kong. *Journal of Banking & Finance*, 24(7), 1179–1201.
- Gao, L., Han, Y., Li, S. Z., & Zhou, G. (2018). Market intraday momentum. *Journal of Financial Economics*, 129(2), 394–414.
- Grant, J. L., Wolf, A., & Yu, S. (2005). Intraday price reversals in the US stock index futures market: A 15-year study. *Journal of Banking & Finance*, 29(5), 1311–1327.
