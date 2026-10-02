# IGV small-account minute fade

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** Failed 4 of 6 pre-registered tests. |
| Instruments | IGV, regular hours, 1-minute bars, flat every night. QQQ and SPY are controls. |
| Data | 2021-09-27 → 2026-09-25, evaluated from 2021-10-25, read via `agent-data/mdq.py`. 2021-12-31 has no bars and is a zero day. Missing minutes are not filled in. |
| Rules | [`research/igv-small-account-fade/research/RULES.md`](../research/RULES.md), locked 2026-09-26, sha256 `32bd1af60a0d` |
| Code | [`research/igv-small-account-fade/research/`](../research/) · 1 store run, plus a verify replay that matched it (see RUNLOG) |

## 1. Summary

**Result.** Fading unusually active one-minute moves in IGV, and only when that minute is large enough for a $5,000 order and too small for a $250,000 order, lost money after 2 bp per side. Out of sample, from 1 July 2024 through 25 September 2026, the $25,000 account returned **−2.96%** (Sharpe **−3.82**, 341 trades, profit factor 0.49). Over the full evaluation window the return was **−9.87%** (Sharpe **−3.97**). With costs set to zero, the out-of-sample Sharpe was still **−0.32**.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-25 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,235 | 673 | 562 |
| Total return | **−9.87%** | −7.12% | **−2.96%** |
| CAGR | −2.10% | −2.73% | −1.34% |
| Annual volatility | 0.53% | 0.65% | 0.35% |
| Sharpe | **−3.97** | −4.28 | **−3.82** |
| Max drawdown | −9.92% | −7.25% | −2.96% |
| Trades / profit factor | 1,480 / 0.59 | 1,139 / 0.63 | 341 / 0.49 |
| Avg net trade | −3.51 bp | −3.24 bp | −4.40 bp |
| *IGV close-to-close Sharpe (max DD)* | *0.29 (−45.9%)* | *0.15 (−45.9%)* | *0.45 (−36.6%)* |

It failed 4 of the 6 acceptance tests written before the first run (§8). The two that passed are the capacity screen (QQQ and SPY produced no signals) and the sample size (341 out-of-sample trades).

**Why.**

1. **Out of sample the gross trade is negative.** The average out-of-sample trade made −0.40 bp before costs. At zero cost the out-of-sample Sharpe is −0.32 and the return is −0.28%. A tighter spread would not have turned the out-of-sample window positive.
2. **The full-sample gross edge is smaller than the round trip, and it does not clear a placebo.** The average full-sample trade made +0.49 bp before costs. Entry plus exit at 2 bp costs 4 bp, so the average net trade is −3.51 bp. At zero cost the full-sample Sharpe is +0.49 and the return is +1.46%. The direction placebo on that gross Sharpe has p = 0.126.
3. **Stops give back more than targets make.** Targets: 884 trades, average net +8.05 bp, +$3,557. Stops: 514 trades, average net −22.83 bp, −$5,867. Time exits: 82 trades, average net −6.96 bp. No trade reached the session flatten.
4. **The loss is in every year and every trailing year.** Each calendar year from 2021 through 2026 has a negative account return. **(post hoc)** Every 252-session Sharpe is negative. The highest is −2.80. Zeroing the 20 best days makes the full-sample result worse (Sharpe −5.61, return −11.93%), so the loss is not one crash being averaged away.

**Recommendation.** Do not trade it. The thin half of the window, the rule with the dollar cap removed, either side alone, and every grid cell are diagnostics. The rules do not allow one of them to replace the primary.

## 2. The strategy

### Rules

On each regular-hours session of IGV, at the close of minute `k`:

```
r = ln(close[k] / close[k-1])          both minutes must exist
D = close[k] * volume[k]
medians = prior 20 sessions at this same clock minute

signal if 09:45 ≤ k ≤ 15:30 (12:00 on an early close)
     and $250,000 ≤ D < $1,000,000
     and D / median(D) ≥ 2
     and |r| ≥ max(10 bp, 2 × median|r|)
fade it: short an up minute, long a down minute
fill at the next existing minute's open
one position; $5,000 notional on a $25,000 account
exit, in order, on a later close:
    stop    if the move extends by another 1×
    target  if half the move has retraced
    flatten at 15:55 (12:55 on an early close), or on the last print
    time    15 minutes after the signal
a stop blocks new entries for the rest of the session
cost: 2 bp of the $5,000 on the way in and 2 bp on the way out
```

- **Why the dollar window:** a $5,000 order is at most 2% of a minute with $250,000 of dollar volume. A $250,000 order is more than 25% of a minute under $1,000,000. That is the capacity claim.
- **Why relative volume and a 10 bp floor:** aggressive flow versus that minute's own recent book, and a move larger than a one-cent bounce on an ~$83 price.
- **Why 2 bp per side:** a displayed IGV spread of about one cent is roughly 1.2 bp of the stored median price ($83.50). Half of that is about 0.6 bp. Two bp per side is wider than the 1 bp used for QQQ and SPY in the earlier studies.

### How it trades

| | |
|---|---|
| Sessions with a trade | 752 of 1,235 |
| Trades per 252 sessions | 302 **(post hoc)** |
| Time in market | 1.50% of existing 1-minute bars |
| Holding time | median 3 minutes, mean 4.71 |
| Long / short | 823 / 657 trades. Average net −3.72 bp / −3.24 bp. Profit factor 0.58 / 0.62 |
| Win rate | 53.7%. Average winner +9.58 bp, average loser −18.69 bp. Median net trade +1.46 bp |

The account is in the market a small fraction of the day, which is why a Sharpe near −4 comes with a volatility of about half a percent. The loss is steady relative to that small variation. The t-statistic of the mean daily return is −8.78 full sample and −5.70 out of sample.

## 3. Hypothesis and predictions

The claim was that a one-minute IGV move, large for that clock minute and printed on at least twice the minute's usual dollar volume, is temporary price pressure when the minute's dollar volume is still between $250,000 and $1,000,000. A fixed $5,000 fade would be paid for supplying immediacy that a $250,000 order cannot take the other side of at its own size (Kyle 1985; Campbell, Grossman, and Wang 1993; Avramov, Chordia, and Goyal 2006; Grossman and Miller 1988).

The ordering in the pre-registered predictions matches that story. The size does not. A positive prediction is a statement about a comparison that was written down in advance. It is not a passing strategy, and the gross Sharpe does not clear the placebo in §6.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Thinner signal minutes (under $500k) have a higher average gross trade than thicker ones | +1.47 bp on 236 trades versus +0.31 bp on 1,244 trades. Both average nets are negative (−2.53 bp and −3.69 bp) | Consistent |
| Fade-signed log return from the next open is positive at 5 minutes and larger than at 30 minutes, on every signal | 5 minutes: mean 3.53×10⁻⁵ (n = 3,851). 15 minutes: 3.13×10⁻⁵ (n = 3,844). 30 minutes: −4.35×10⁻⁵ (n = 3,829) | Consistent |
| Removing the $1,000,000 cap lowers the average gross trade | Uncapped average gross −0.25 bp (2,470 trades) versus primary +0.49 bp. Uncapped full-sample Sharpe −5.57, return −18.9% | Consistent |
| QQQ and SPY each have fewer than 30 raw signals | 0 and 0 | Consistent |

The five-minute mean in that row is 3.53×10⁻⁵ in log return. The round trip written into the rules is 4 bp of the ticket. The path bends in the predicted direction, and that recorded mean is much smaller than the cost.

## 4. Method

- **Data.** IGV, QQQ, and SPY 1-minute regular-hours bars, 1,254 sessions each, from 2021-09-27 through 2026-09-25, read through `agent-data/mdq.py` and split-adjusted. IGV's 5-for-1 split on 2024-03-07 is in the store; dollar volume is unchanged by that adjustment. Dividends are not adjusted. The book is flat overnight, so an ex-date open gap is not held. No bar opens outside 09:30–15:59. 2021-12-31 has no bars. Missing minutes are left missing: a signal needs the previous clock minute, and a fill uses the next existing print. ADBE and GOOGL have 19 sessions of 1-minute bars and were not used. The other stored names have daily bars only.
- **Pre-registration.** `RULES.md` fixed the account, the dollar window, the signal, the exits, the 2 bp cost, the split, and the acceptance lines before any return was computed. The only pre-lock look was `counts.py`: dollar-volume percentiles and signal counts. It found 3,942 raw IGV signals (684 out of sample) and 0 on QQQ and SPY. That count did not apply the early-close entry cutoff. The backtest, which does, records 3,941 raw IGV signals. `RULES.md` was not committed before the run. The out-of-sample window was already used by `qqq-intraday-trend` (QQQ noise-boundary momentum made money; the same rules lost on IGV), `qqq-intraday-reversion` (fading 4σ shocks lost on QQQ, SPY, and IGV), and `intraday-channel-trend` (the equal-weight book lost, including IGV).
- **Fills and costs.** A decision at a minute's close fills at the next existing minute's open. Exits are close-confirmed. High and low are not used, so this is not an intrabar stop order. One extra bar of delay, chosen before the run where the delay paragraph in the rules is ambiguous, fills the exit at the only remaining open when exactly one later minute exists, and cancels an entry that does not have the second later minute. The same-bar close fill was run only as the labelled upper bound.
- **Returns.** Daily simple return is the session's dollar P&L divided by $25,000. A session with no exiting trade is 0. Sharpe is the mean daily return divided by its sample standard deviation, times √252, with a zero rate. CAGR uses a 252-session year. The trade notional stays $5,000. It does not grow with equity.
- **Verification.** The self-test passed before the store was opened: target, stop with no re-entry, time exit, flatten, early close, missing minute, both dollar-volume bounds, relative volume, magnitude, gap fill, last-bar close, a signal ignored while in a position, re-entry after a target, and a one-bar delay. `verify.py` is a separate loop and matched all 1,480 trades on side, times, prices, and exit reason.
- **Runs.** One store run, reason `initial`. The verify replay matched and did not change the backtest. No rule was rewritten after the result.

## 5. Results

![Growth of $1](figures/equity.svg)

Growth of $1 for the fade, for IGV close to close, and for IGV open to close. The out-of-sample boundary is marked. The fade declines from the first evaluation session and ends at the full-sample loss of 9.87%. IGV close to close finishes higher and spends a long stretch well below its peak.

![Drawdown](figures/drawdown.svg)

The fade's drawdown ends at −9.92%, which is the full-sample loss. Out of sample the drawdown equals the −2.96% loss: that window finished at its low. IGV close to close drew down −45.9%.

| Strategy (2 bp per side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | −9.87% / −3.97 / −9.92% | −4.28 | −2.96% / −3.82 / −2.96% |
| *IGV close to close* | *+22.4% / 0.29 / −45.9%* | *0.15* | *+22.1% / 0.45 / −36.6%* |
| *IGV open to close* | *+35.2% / 0.37 / −35.2%* | *0.44* | *+9.71% / 0.29 / −35.2%* |

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | IGV close to close |
|---|---:|---:|---:|---:|
| 2021 | −0.48% | −5.28 | −0.54% | −7.35% |
| 2022 | −3.92% | −4.95 | −3.94% | −36.3% |
| 2023 | −2.07% | −3.71 | −2.40% | +58.5% |
| 2024 | −2.28% | −5.50 | −2.33% | +23.4% |
| 2025 | −1.43% | −3.43 | −1.47% | +5.56% |
| 2026 | −0.07% | −1.73 | −0.07% | +0.30% |

2026 has 7 trades. 2021 is a partial year (evaluation starts 25 October). 2024 mixes the two samples; the split date is 1 July, not 1 January.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trade's entry and exit and multiplies the gross dollars by a coin flip. 2,000 draws, seed 20260926. Costs are not in this comparison. The actual full-sample gross Sharpe is 0.49. The null mean is −0.02 and the null 95th percentile is 0.74. p = 0.126. The timing placebo draws the same number of entries per session from minutes that pass only the clock and the dollar window, with a random side and the same exits. 500 draws, seed 20260927. Its mean gross Sharpe is 0.016 and p = 0.162. Neither test puts the gross result outside the null. The acceptance line uses the direction placebo only.

### Bootstrap

Circular 20-session blocks, 2,000 draws, seed 20260928. The 95% interval of the full-sample net Sharpe is **−4.99 to −3.06**.

### Parameter plateau

![Parameter grid](figures/grid.svg)

The grid is relative volume 1.5, 2, 3; magnitude multiple 1.5, 2, 3; dollar cap $0.5M, $1M, $2M; hold 5, 15, 30 minutes. 81 cells. **0 cells** have an in-sample Sharpe above 0. The primary's in-sample Sharpe ranks 36th of 81. The rank correlation of in-sample and out-of-sample Sharpe is 0.56. Nothing was selected from the grid. The out-of-sample panel is the same cloud: every plotted cell is below zero on both axes.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 1 bp | **2 bp** | 4 bp | 6 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.49 | −1.61 | **−3.97** | −8.78 | −12.38 |
| Out-of-sample Sharpe | −0.32 | −2.03 | **−3.82** | −6.90 | −8.66 |
| Full-sample return | +1.46% | −4.37% | **−9.87%** | −19.9% | −28.9% |

One extra bar of delay: full-sample Sharpe −4.21, out-of-sample −3.77, 1,449 trades. The same-bar close fill, labelled in the rules as an upper bound: full-sample Sharpe −4.15, out-of-sample −3.98. It is not above the primary.

### Other markets (identical rules)

| | Raw signals | Trades |
|---|---:|---:|
| QQQ | 0 | 0 |
| SPY | 0 | 0 |

There is no Sharpe to report. The dollar cap excludes both tapes, which is what the screen was written to do.

## 7. Where the result comes from

![By the day's move](figures/move_quintiles.svg)

Average net trade by quintile of IGV's open-to-close return. Quintile 1 is the down days.

| Quintile | Trades | Avg net | Open-to-close range |
|---|---:|---:|---|
| 1 | 380 | −4.27 bp | −5.29% to −1.12% |
| 2 | 305 | −3.47 bp | −1.12% to −0.22% |
| 3 | 259 | −3.31 bp | −0.22% to +0.41% |
| 4 | 235 | −1.98 bp | +0.41% to +1.12% |
| 5 | 301 | −3.94 bp | +1.12% to +12.3% |

Every quintile is negative. Quintiles of the open-to-signal move are negative as well, between −3.08 bp and −4.02 bp. None of these splits was used to filter the rule.

![By entry time](figures/by_entry.svg)

| Entry | Trades | Avg net | Profit factor |
|---|---:|---:|---:|
| 09:45–11:00 | 653 | −2.77 bp | 0.69 |
| 11:00–13:00 | 545 | −4.09 bp | 0.51 |
| 13:00–15:00 | 256 | −4.20 bp | 0.53 |
| 15:00 and later | 26 | −3.06 bp | 0.54 |

Long and short both lose (§2). The morning is the least negative bucket and is still negative.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| Out-of-sample Sharpe and profit factor | ≥ 0.5 and ≥ 1.10 | −3.82 and 0.49 | ❌ |
| Direction placebo p, full-sample gross Sharpe | ≤ 0.05 | 0.126 | ❌ |
| In-sample Sharpe, and share of in-sample grid > 0 | > 0 and ≥ 60% | −4.28 and 0% | ❌ |
| Full-sample return at 4 bp per side | > 0 | −19.9% | ❌ |
| QQQ and SPY raw signals | each < 30 | 0 and 0 | ✅ |
| Out-of-sample trades | ≥ 100 | 341 | ✅ |

Line 5 passing means the dollar window excluded QQQ and SPY. It does not mean the IGV trades were a small-account edge. A costed placebo is not the acceptance test; the gross placebo above is, and it is not cleared either.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | Out-of-sample gross average −0.40 bp; zero-cost out-of-sample Sharpe −0.32 | No separate live book. The test already failed |
| Concentration in a few days | **(post hoc)** Worst 20 sessions sum to −2.22 percentage points of daily return; all sessions sum to −10.38 percentage points. Dropping the 20 best days deepens the loss | The loss is spread across years |
| Generalization | Same rule on QQQ and SPY does not trade. Uncapped IGV is worse. 0 of 81 grid cells are positive in sample | The result is specific to this rule and this tape, and it is a loss on both |
| Execution | Round trip is 4 bp against a full-sample gross average of +0.49 bp. One extra minute of delay lowers the full-sample Sharpe from −3.97 to −4.21. Exits are judged on closes, not on intrabar stops | A live stop would not be this backtest |
| Short sample / regime coverage | 1,235 sessions and six calendar years, all negative. **(post hoc)** Trailing 252-session Sharpe −2.90. Rolling 252-session Sharpe is positive in 0% of windows | Later years are already inside the failed out-of-sample window |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A different dollar cap, a thinner subset, one side, a 5-minute bar, or a lower cost is a new study with its own `RULES.md`. The long side alone is not a substitute for an account that cannot short.

## 11. Post hoc (not part of the verdict)

`posthoc.py` reads `daily.csv` and `trades.csv` only.

- **Rolling Sharpe.** 252-session Sharpe ranges from −6.16 to −2.80. The share above zero is 0. The window ending 2026-09-25 is −2.90.
- **Best days.** Setting the 20 highest daily returns to zero changes the full-sample result from Sharpe −3.97 and −9.87% to Sharpe −5.61 and −11.93%.
- **Trade rate.** 302 trades per 252 sessions.

None of this changes §8.

### Ideas for a new study

A test of names whose whole session is too small for an institutional order needs 1-minute bars this store does not have. The intraday history here is QQQ, SPY, and IGV, plus 19 sessions of ADBE and GOOGL. That study would need its own rules and that other tape. It is not a retune of this one.

## 12. Reproduce

From the repo root:

```bash
python research/igv-small-account-fade/research/backtest.py
```

```bash
python research/igv-small-account-fade/research/verify.py
```

```bash
python research/igv-small-account-fade/research/posthoc.py
```

```bash
python research/igv-small-account-fade/research/charts.py
```

`backtest.py` refuses to run if `RULES.md` does not match `RULES.lock`, runs the synthetic self-test, then writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`. The store run takes about one minute. `verify.py` replays IGV and checks every saved trade. `posthoc.py` does not open the store. `charts.py` writes the SVG files in `report/figures/`. Seeds are 20260926, 20260927, and 20260928. A rerun on the same store, with the same rules, produces the same numbers. `backtest.py` appends another run-log line each time it opens the store.

### References

- Avramov, D., Chordia, T., and Goyal, A. (2006). Liquidity and Autocorrelations in Individual Stock Returns. *Journal of Finance*.
- Campbell, J., Grossman, S., and Wang, J. (1993). Trading Volume and Serial Correlation in Stock Returns. *Quarterly Journal of Economics*.
- Grossman, S., and Miller, M. (1988). Liquidity and Market Structure. *Journal of Finance*.
- Kyle, A. (1985). Continuous Auctions and Insider Trading. *Econometrica*.
- Nagel, S. (2012). Evaporating Liquidity. *Review of Financial Studies*.
