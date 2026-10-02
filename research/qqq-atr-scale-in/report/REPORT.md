# QQQ intraday ATR scale-in

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** Failed 5 of 6 pre-registered tests. |
| Instruments | QQQ, regular hours, 5-minute bars, flat every night. SPY is the cross-market test. IGV is reported under the same rules. |
| Data | 2021-10-07 → 2026-09-25, read via `agent-data/mdq.py`. 2021-12-31 has no 1-minute bars and is a zero day. 2025-01-09 is a closure and is not in the calendar. |
| Rules | [`research/qqq-atr-scale-in/research/RULES.md`](../research/RULES.md), locked 2026-09-26 17:12 UTC, sha256 `c645bad14262` |
| Code | [`research/qqq-atr-scale-in/research/`](../research/) · 2 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Scaling into a QQQ move away from the session open, in units of the prior day's average true range, lost money. Out of sample, from 1 July 2024 through 25 September 2026, the account returned **−8.81%** after 1 bp per side (Sharpe **−0.57**, profit factor 0.78, 343 campaigns). Over the full window, 7 October 2021 through 25 September 2026, it returned **−25.51%** (Sharpe **−1.00**). With costs set to zero, the out-of-sample Sharpe was still **−0.38** and the return was **−6.19%**.

The payoff had the shape the study was built to look for. The full-sample win rate was **55.2%**, and the skewness of per-unit campaign returns was **−2.23**. Out of sample the win rate was **58.6%**. The average campaign was positive per unit of notional (**+5.63 bp** out of sample, **+2.44 bp** full sample) and the account lost money, because the campaigns that added a second or third unit lost more dollars than the one-unit campaigns made.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-07 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,247 | 685 | 562 |
| Total return | **−25.51%** | −18.31% | **−8.81%** |
| CAGR | −5.78% | −7.17% | −4.05% |
| Annual volatility | 5.80% | 4.79% | 6.83% |
| Sharpe | **−1.00** | −1.53 | **−0.57** |
| Max drawdown | −27.54% | −20.78% | −10.62% |
| Trades / profit factor | 849 / 0.72 | 506 / 0.68 | 343 / 0.78 |
| Avg net trade (per unit) | +2.44 bp | +0.27 bp | +5.63 bp |
| *QQQ buy and hold Sharpe (max DD)* | *0.74 (−35.6%)* | *0.56 (−35.6%)* | *0.97 (−24.2%)* |

QQQ buy-and-hold returned +107.0% over the full window and +55.4% out of sample. The scale-in failed 5 of the 6 acceptance tests written before the first run (§8). The test it passed is the sample size.

**Why.**

1. **There is no gross edge.** At zero cost the out-of-sample return is −6.19% and the Sharpe is −0.38. The full-sample zero-cost return is −20.18% (Sharpe −0.76). A tighter spread would have left both windows negative.
2. **The side of the trade is worse than a coin flip.** The full-sample gross Sharpe is −0.76. Two thousand books that kept the same entries and exits and flipped each campaign's side averaged +0.03, and their 95th percentile was +0.73. The actual result sits in the left tail: p = 0.95.
3. **The capped winners do not pay for the positions that are still open at the close.** On an account that starts at 1, target exits contributed +0.508 and session flattens contributed −0.763. One-unit campaigns contributed +0.253. Two-unit campaigns contributed −0.221. Three-unit campaigns, 32 of them, contributed −0.287.
4. **The same rule loses on SPY and on IGV.** Out-of-sample Sharpe is −0.53 on SPY and −1.09 on IGV. Daily strategy returns on QQQ and SPY correlate 0.91.
5. **The sign is the same across the grid and across the bootstrap.** None of the 15 in-sample grid cells has a positive Sharpe. A block bootstrap puts the full-sample Sharpe between −1.66 and −0.44 at 95%.

**Recommendation.** Do not trade it. The long side, the short side, any grid cell, a stop, and a book that takes only the first unit are diagnostics. The rules do not allow one of them to replace the primary.

## 2. The strategy

### Rules

```
A = Wilder ATR(14), fixed at the prior daily close
S = today's 09:30 open

On a 5-minute close, if the next bar's open is before 15:30
(before 12:30 on a 13:00 close):

    flat, close <= S − 0.5 A     buy 1 unit at the next open
    flat, close >= S + 0.5 A     short 1 unit at the next open

    long,  close >= average + 0.25 A
                                  sell every unit at the next open
    long,  units < 3, close at or below the next half-ATR rung,
           and the close is under every fill already on
                                  buy 1 more unit at the next open

    short, the mirror

The last bar flattens whatever is still open, at that bar's close.
There is no stop. Each unit is one third of equity at the prior close.
Cost is 1 bp of unit notional per side. A campaign that adds pays
the cost on every unit.
```

- **Why three equal units, not a doubled stake.** The question was whether adding to a loser, and so pulling the average toward the market, is paid. A doubled stake is a different bet on the same signal. It was left out on purpose.
- **Why the gain is capped and the loss is not.** That is the negative-skew shape. The target is a quarter of an ATR past the average entry, which is half the distance between rungs. The position that never gets there is closed at the cash session's last print.
- **Why both sides.** A long-only book on this window would have been long a market that rose 55% out of sample. That rise was already known from earlier studies. The rule fades moves in either direction.

### How it trades

| | |
|---|---|
| Sessions with a campaign | 702 of 1,246 tradable sessions |
| Campaigns | 849 (172 per year) |
| Time in market | 29.4% of 5-minute bars |
| Holding time | median 29 bars, mean 33.5 bars |
| Long / short | 453 / 396 campaigns; profit factor 0.73 / 0.70; net dollars −0.135 / −0.120 |
| Win rate | 55.2% full sample; average winner +44.9 bp, average loser −50.0 bp, both per unit |

A campaign is one round trip from flat to flat, however many units it added. The average entry in the trade file is the arithmetic mean of the fills. Net dollars are on an account that starts at 1 and compounds once a day. They sum to the full-sample total return.

## 3. Hypothesis and predictions

The scale-in is an inventory. A market maker who buys as price falls is paid when the dislocation was someone else's need to transact (Hendershott and Seasholes 2007). On an index ETF that someone is often a hedger or a leveraged fund that has to finish the order (Cheng and Madhavan 2009; Baltussen, Da, Lammers and Martens 2021). The unit is Wilder's ATR because it is the published scale of a normal day. The cap on the gain, and the refusal to stop, are what make the distribution short-option shaped (Ilmanen 2011). The prior on this particular tape was low: single-shot intraday fades in `qqq-intraday-reversion` and `igv-small-account-fade` had already lost here, and the shocks in the first of those kept going.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Win rate above 55% and negative skew of per-unit net returns | Win rate 55.2%. Skewness −2.23 | **Consistent** |
| 2. Mean daily return positive in excursion quintiles 2 and 3 pooled, and negative in quintile 5 | Quintiles 2–3: +0.073% per session. Quintile 5: −0.324% per session | **Consistent** |
| 3. One-unit campaigns have a higher mean per-unit net return than three-unit campaigns | +13.4 bp (694 campaigns) against −107.1 bp (32 campaigns) | **Consistent** |

All three predictions are consistent, and the account lost money. The predictions describe the shape and where the loss sits. They do not say the ordinary-day gains are large enough to pay for the tail. That was the P&L claim, and it failed. The mechanism of a paid inventory is not confirmed.

## 4. Method

- **Data.** QQQ, SPY, and IGV 1-minute bars, 27 September 2021 through 25 September 2026, resampled to 5 minutes from 09:30. Daily bars supply the ATR. QQQ and SPY daily history starts 16 September 2021; IGV starts 17 September. The ATR seed is the 6 October 2021 close for QQQ and SPY, and the evaluation starts the next session. IGV's first session is 8 October 2021. QQQ and IGV have no daily bar on 24 and 25 September 2026; the buy-and-hold mark on those two sessions is the last 5-minute close (`marks_from_5m_close` = 2). No dividends are stored. IGV's 5-for-1 split on 7 March 2024 is adjusted by `mdq`. QQQ and SPY have no missing 5-minute bucket. IGV has 15 decision bars whose next bucket is missing; those signals are void. 2021-12-31 is on the calendar with a strategy return of 0.
- **Pre-registration.** `RULES.md` fixed the hypothesis, the rungs, the target, the three-unit cap, the 1 bp cost, the split, and the acceptance lines before any return was computed. The only pre-lock look was `counts.py`: coverage, corporate actions, and counts of closes beyond each rung. It found 702 QQQ sessions with a legal level-1 close (284 of them out of sample) and 31 sessions that reached the third rung. The backtest later records 702 sessions with a campaign, the same count. Out of sample, level-1 sessions were somewhat more often down than up (160 against 129, and a session can be both). The rule stayed symmetric. `RULES.md` was locked with a hash at 17:12 UTC. It was **not committed to git** before the first run. The first run-log entry is 17:22 UTC.
- **Fills and costs.** A signal at a bar's close fills at the next bar's open. The session flatten fills at the last bar's close. One basis point per side is the protocol default and is wider than a one-cent QQQ spread.
- **Returns.** Daily simple returns on the NYSE calendar. Flat sessions and the missing tape on 2021-12-31 are 0. Sharpe is the mean divided by the sample standard deviation, times √252, with a zero rate. Profit factor uses net dollars. The average trade in basis points is the mean per-unit net return, so a three-unit loss is not three rows.
- **Verification.** The self-test covers a one-unit target, a three-unit flatten, the short side, a gap that does not add, a first close beyond the third rung, the 15:30 and 12:30 cutoffs, a missing bucket, a re-entry, a last-bar signal, a wick through the target, and same-bar and two-bar fills. `verify.py` replayed every QQQ session in a second implementation and matched all 849 campaigns.
- **Runs.** Two store runs. The first was the initial run. The second was the verify replay, which matched and did not change a headline. No bug fix moved the numbers.

## 5. Results

![Growth of $1](figures/equity.svg)

The scale-in ends the sample at a loss in both halves. QQQ buy-and-hold rises. The dashed line is 1 July 2024.

![Drawdown](figures/drawdown.svg)

The strategy's deepest drawdown is −27.5%, and the line does not get back to the old peak. QQQ's drawdown is deeper (−35.6%) on a position that is always on. The strategy is in the market on 29% of its bars.

| Strategy (1 bp/side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **QQQ primary** | −25.51% / −1.00 / −27.54% | −1.53 | −8.81% / −0.57 / −10.62% |
| SPY, same rules | −21.12% / −0.98 / −22.38% | −1.72 | −7.18% / −0.53 / −9.46% |
| IGV, same rules | −29.92% / −1.15 / −29.95% | −1.20 | −14.03% / −1.09 / −16.68% |
| *QQQ buy and hold* | *+107.0% / 0.74 / −35.6%* | *0.56* | *+55.4% / 0.97 / −24.2%* |
| *QQQ open to close* | *+31.8% / 0.39 / −23.7%* | *0.52* | *+4.6% / 0.20 / −20.4%* |

The open-to-close row is the cash session itself, uncosted. The scale-in lost money in a window where the intraday drift was positive.

![Calendar-year return](figures/by_year.svg)

| Year | Sessions | Strategy | Sharpe | Max DD | QQQ buy and hold |
|---|---:|---:|---:|---:|---:|
| 2021 | 60 | −1.35% | −1.62 | −2.70% | +10.6% |
| 2022 | 251 | −12.76% | −2.05 | −13.75% | −33.1% |
| 2023 | 250 | −5.02% | −1.61 | −5.85% | +53.8% |
| 2024 | 252 | −3.67% | −0.88 | −4.58% | +24.8% |
| 2025 | 250 | −6.80% | −0.71 | −8.59% | +20.2% |
| 2026 | 184 | +1.49% | +0.67 | −2.36% | +21.2% |

2021 starts on 7 October and 2026 ends on 25 September. The strategy's yearly return is negative in every complete year in the sample. The partial year 2026 is the exception, at +1.49%.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction test keeps every campaign's entry and exit and multiplies the whole campaign's gross result by a coin flip. Costs are left out, so a coin flip is not dragged down by the spread. The 2,000 draws (seed 20260926) averaged a gross Sharpe of +0.03. The 95th percentile was +0.73. The actual gross Sharpe, −0.76, gives **p = 0.95**. Random sides beat this book. The test the rules set was whether the chosen side beats the coin flip. It does not.

A timing placebo, not an acceptance line, keeps the side and the unit cap and moves the entry to a random legal bar, then applies the same add and the same target. Across 500 draws (seed 20260927) the mean gross Sharpe was **−3.11**. The actual −0.76 was above all 500 draws (p = 0.002). The rung picks a less bad minute than a random minute with the same side. The book at that minute is still negative. The mean draw contained 749 campaigns, against 849 in the primary; that count is stored under the key `mean_sessions_with_a_contribution`.

### Bootstrap

A circular block bootstrap of the full-sample daily returns (20-session blocks, 2,000 draws, seed 20260928) gives a 95% interval of **−1.66 to −0.44** for the Sharpe. The share of draws at or below zero is 0.999. The full-sample t-statistic of the mean daily return is −2.22. Out of sample it is −0.85.

### Parameter plateau

![Parameter grid](figures/grid.svg)

Spacing runs from 0.25 to 0.75 ATR and the target from 0.125 to 0.375. Fifteen cells, judged in sample. **None of the 15 has an in-sample Sharpe above zero** (the line required 9). The primary cell, 0.50 by 0.25, ranks 2nd of 15, with in-sample Sharpe −1.53. The in-sample best cell is spacing 0.625 and target 0.375, in-sample Sharpe −1.53, out-of-sample Sharpe −0.52. Every cell's out-of-sample Sharpe is negative. The least negative is −0.33, at spacing 0.75 and target 0.25. Nothing was selected from the grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.76 | −0.88 | **−1.00** | −1.24 | −1.47 |
| OOS Sharpe | −0.38 | −0.48 | **−0.57** | −0.76 | −0.94 |
| Full-sample return | −20.18% | −22.89% | **−25.51%** | −30.49% | −35.13% |

There is no cost in the sweep at which the full-sample return is positive. The out-of-sample Sharpe is negative at zero.

Filling one bar later (10 minutes after the signal close, instead of 5) gives Sharpe −1.24 full and **−0.79** out of sample. The labelled upper bound, filling at the signal close, gives −1.13 full and −0.56 out of sample. In this sample the same-bar fill is not a better book than the primary.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|
| QQQ | −1.53 | −0.57 | −25.51% / 0.72 |
| **SPY** | −1.72 | **−0.53** | −21.12% / 0.70 |
| IGV | −1.20 | −1.09 | −29.92% / 0.74 |

SPY is the name the acceptance line requires. It lost. IGV lost by more, at a 1 bp cost that is light for IGV's spread. Daily strategy returns correlate 0.91 (QQQ–SPY), 0.74 (QQQ–IGV), and 0.69 (SPY–IGV).

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

| Quintile of \|open → last\| / ATR | Sessions | Mean excursion | Mean daily strategy return |
|---|---:|---:|---:|
| 1 (quiet) | 250 | 0.07 | +0.050% |
| 2 | 249 | 0.21 | +0.066% |
| 3 | 249 | 0.38 | +0.079% |
| 4 | 249 | 0.58 | +0.014% |
| 5 | 249 | 1.05 | −0.324% |

The positive cells are small. Quintile 5 is where the day's range exceeds a prior ATR, which is where the second and third units can be filled, and the mean day there is negative.

| Exit | Campaigns | Win rate | Avg per unit | Net dollars |
|---|---:|---:|---:|---:|
| Target | 310 | 100% | +54.6 bp | +0.508 |
| Session flatten | 539 | 29.5% | −27.5 bp | −0.763 |

Every target exit was a winner. That is the cap doing what it was built to do. The flattens, which include the days that never came back, more than offset them. Their profit factor is 0.15.

| Units | Campaigns | Win rate | Avg per unit | Net dollars |
|---|---:|---:|---:|---:|
| 1 | 694 | 62.4% | +13.4 bp | +0.253 |
| 2 | 123 | 25.2% | −30.9 bp | −0.221 |
| 3 | 32 | 15.6% | −107.1 bp | −0.287 |

The first unit, taken alone in the dollar column, is a gain. Adding the second and the third is where the account gives it back. That split was pre-registered as a prediction about the tail. It was not a license to drop the adds after seeing it.

Long campaigns won 61.8% of the time and lost 0.135 of starting equity. Short campaigns won 47.7% and lost 0.120. All seven entry hours from 9 through 15 have negative net dollars. None of these splits was used to change the rule.

![Campaign return](figures/trade_distribution.svg)

Most campaigns are small. The left tail is long. Skewness of the per-unit net return is −2.23, which is prediction 1, and the sum of the dollars under that histogram is the −25.51% full-sample return.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| OOS Sharpe | ≥ 0.50 | −0.57 | ❌ |
| OOS profit factor | ≥ 1.10 | 0.78 | ❌ |
| Direction placebo, full sample | p ≤ 0.05 | 0.95 | ❌ |
| In-sample Sharpe | > 0 | −1.53 | ❌ |
| In-sample grid cells with Sharpe > 0 | ≥ 9 of 15 | 0 of 15 | ❌ |
| Full-sample return at 2 bp per side | > 0 | −30.49% | ❌ |
| SPY out-of-sample Sharpe | > 0 | −0.53 | ❌ |
| Out-of-sample campaigns | ≥ 100 | 343 | ✅ |

The status is **Rejected** because the sample is large enough and five of the other lines fail. A gross placebo whose null is centered at zero is the test that was written; this book fails it on the losing side.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | What follows |
|---|---|---|
| No edge before costs | Zero-cost out-of-sample Sharpe −0.38, return −6.19% | A cheaper broker does not flip the sign |
| The adds are the loss | Three-unit net dollars −0.29; session flattens −0.76 | The cap harvested the wins and the tail was larger |
| One factor | QQQ–SPY correlation 0.91; SPY and IGV lose | The loss is the same trade three times |
| Recent window | **(post hoc)** Trailing 252 sessions: −1.72%, Sharpe −0.39. From 1 May 2025: −0.77%, Sharpe −0.13 | The partial 2026 gain does not carry the trailing year |
| Concentration of losses | **(post hoc)** The worst 5 campaigns are 57% of net dollars. Zeroing the worst 5 days leaves −11.17% (Sharpe −0.54). Zeroing the worst 20 days leaves +8.53% (Sharpe +0.49) | Deleting the tail after the fact is not a strategy |
| The wins are small | **(post hoc)** Zeroing the best 5, 10, and 20 days makes the full-sample result worse (−31.3%, −33.9%, −37.7%) | The result is not a few large wins covering a flat book |

**(post hoc)** The trailing 252-session Sharpe is positive in 0.10% of windows. The highest is +0.16 and the lowest is −2.40. The longest run of negative sessions is 5, so the path is a grind: annual volatility is 5.8%, and five of the six calendar years lose. April 2025, 21 sessions, returned −3.20% (Sharpe −1.18). That month is inside the loss and it is not the whole of it.

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A stop, a one-unit book, or the other side of these entries would be a different rule, with its own `RULES.md`, on data this study has not used.

## 11. Post hoc (not part of the verdict)

The checks in §9 that are marked **(post hoc)** were computed after the run, from `daily.csv` and `trades.csv`, by `posthoc.py`. Zeroing the worst days and the best days, the trailing year, the window from May 2025, and April 2025 are in that group. None of them changes §8.

### Ideas for a new study

The one-unit column is a gain on this sample, and the adds are a loss. A rule that does not add was visible only after these results. So was the fact that a random side beat the side this rule took. Either change needs its own pre-registration and data this study did not use. This sample cannot be the test of the rescue.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-atr-scale-in/research/backtest.py
```

```bash
python research/qqq-atr-scale-in/research/verify.py
```

```bash
python research/qqq-atr-scale-in/research/posthoc.py
```

```bash
python research/qqq-atr-scale-in/research/charts.py
```

`backtest.py` runs the self-test, refuses to open the store if `RULES.md` has changed, and writes `results.json`, `daily.csv`, `trades.csv`, and a `RUNLOG.md` entry. The full run takes under a minute. `verify.py` rewrites nothing except a match entry in the log. Seeds are 20260926 (direction placebo), 20260927 (timing placebo), and 20260928 (bootstrap). A rerun on the same store produces the same headlines.

### References

- Baltussen, Guido, Zhi Da, Sten Lammers, and Martin Martens (2021). "Hedging Demand and Market Intraday Momentum." *Journal of Financial Economics*.
- Cheng, Minder, and Ananth Madhavan (2009). "The Dynamics of Leveraged and Inverse Exchange-Traded Funds." *Journal of Investment Management*.
- Gao, Lei, Yufeng Han, Sophia Zhengzi Li, and Guofu Zhou (2018). "Market Intraday Momentum." *Journal of Financial Economics*.
- Glosten, Lawrence R., and Paul R. Milgrom (1985). "Bid, Ask and Transaction Prices in a Specialist Market with Heterogeneously Informed Traders." *Journal of Financial Economics*.
- Hendershott, Terrence, and Mark S. Seasholes (2007). "Market Maker Inventories and Stock Prices." *American Economic Review, Papers and Proceedings*.
- Ilmanen, Antti (2011). *Expected Returns*. Wiley.
- Lehmann, Bruce N. (1990). "Fads, Martingales, and Market Efficiency." *Quarterly Journal of Economics*.
- Wilder, J. Welles Jr. (1978). *New Concepts in Technical Trading Systems*. Trend Research.
