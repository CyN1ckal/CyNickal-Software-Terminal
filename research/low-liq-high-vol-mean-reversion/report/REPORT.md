# Weekly quintile reversal: low-liquidity small caps

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** Failed 5 of 6 pre-registered tests. The minimum sample was met. |
| Instruments | 27 primary names from the Koyfin screen, dollar-neutral, held about a week. 28 other names from the same screen are the cross-market book |
| Data | Daily bars 2016-01-04 → 2026-09-25, evaluated 2016-01-19 → 2026-09-25, read via `agent-data/mdq.py`. HLS, TCS, and CVO had no US listing. PARK had 204 bars and was dropped |
| Rules | [`research/low-liq-high-vol-mean-reversion/research/RULES.md`](../research/RULES.md), locked 2026-09-26 18:04 UTC, sha256 `59b886851c1c` |
| Code | [`research/low-liq-high-vol-mean-reversion/research/`](../research/) · 2 logged store runs, after 1 crash that wrote nothing (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** A one-week reversal book on the low-liquidity small-cap screen lost money out of sample. From 2 January 2024 through 25 September 2026 it returned **−56.6%** after 20 bp per side and a 5% borrow on shorts (Sharpe **−1.13**, profit factor 0.78, 521 trades). Over the full evaluation window, 19 January 2016 through 25 September 2026, it returned **−58.5%** (Sharpe **−0.44**). In sample the return was −4.4% and the Sharpe was 0.01.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2016-01-19 → 2026-09-25 | → 2023-12-29 | 2024-01-02 → |
| Sessions | 2,689 | 2,003 | 686 |
| Total return | **−58.5%** | −4.4% | **−56.6%** |
| CAGR | −7.9% | −0.6% | −26.4% |
| Annual volatility | 15.9% | 11.6% | 24.5% |
| Sharpe | **−0.44** | 0.01 | **−1.13** |
| Max drawdown | −61.6% | −25.5% | −59.6% |
| Trades / profit factor | 936 / 0.87 | 415 / 1.03 | 521 / 0.78 |
| Avg net trade | −31.1 bp | +6.3 bp | −60.9 bp |
| *Equal-weight screen Sharpe (max DD)* | *0.74 (−43.0%)* | *0.91 (−41.9%)* | *0.16 (−23.8%)* |
| *SPY Sharpe (max DD)* | *0.49 (−25.4%)* | *0.13 (−25.4%)* | *1.20 (−19.9%)* |

It failed 5 of the 6 acceptance tests written before the first run (§8). The one that passed is the sample size, 521 out-of-sample trades.

**Why.**

1. **Out of sample the gross trade is negative.** The average out-of-sample trade made −15.2 bp before costs. At a zero spread and zero borrow the out-of-sample Sharpe is still −0.33 and the return is −25.8% (§6). A tighter spread does not turn that window positive.
2. **The full-sample gross edge is smaller than the round trip.** The average full-sample trade made +14.6 bp before costs. Entry plus exit at 20 bp is 40 bp, before borrow. The average net trade is −31.1 bp. At zero cost the full-sample return is +12.3% and the Sharpe is 0.15. That gross result does not clear the direction placebo (p = 0.289).
3. **The same rule loses on the other half of the screen.** The cross-market book returned −31.9% out of sample (Sharpe −0.56). Its in-sample Sharpe was 0.84. The in-sample gain did not carry over.
4. **The book was flat when the screen was rising, and invested when the reversal stopped paying.** Exposure is 28.5% of in-sample sessions and 97.1% of out-of-sample sessions. The names were chosen because they sit in the band in September 2026. In the earlier years the point-in-time volatility and dollar-volume filters often left them out, so the book earned nothing in 2016, 2017, and 2020 while the equal-weight screen rose. Out of sample they are inside the band almost every week, and the average gross trade is negative.

**Recommendation.** Do not trade it. The rules forbid promoting the long-only book, the cross-market book, either side alone, a grid cell, or a lower cost. The long-only book also failed its own out-of-sample line (Sharpe −0.56, profit factor 0.84, 258 trades).

## 2. The strategy

### Rules

```
Each ISO week, at that week's last close:
  keep names whose trailing 63-session volatility is 20–60%
    and whose median dollar volume is $1–10 million
  rank them by the one-week close-to-close return
  if at least 10 names qualify:
    long the worst quintile, short the best quintile, equal weight, dollar neutral
    buy and sell at the next session's open
    cover at the open of the first session two weeks on
    pay 20 bp on shares traded, and 5% Actual/365 on short notional overnight
```

- **Why one week:** Lehmann (1990) and Jegadeesh (1990) document a one-week reversal. The next open, rather than the signal close, is the fill this protocol requires.
- **Why a quintile:** the published effect is in the extremes. A 27-name book cannot fill a decile with two names on each side. `q = n // 5`, and a week with `q < 2` is flat.
- **Why these names:** they are the even tickers, A to Z, from the user's Koyfin screen after three names with no US listing and one name with fewer than 252 daily bars were dropped. The odd tickers are the cross-market book. The price-change columns of the screen were not used.

### How it trades

| | |
|---|---|
| Trades | 936 full sample, 521 out of sample |
| Time in market | 46.0% of sessions full sample, 97.1% out of sample |
| Holding time | median 7 calendar days, mean 8.4 |
| Long / short | 466 longs, average net −24.5 bp; 470 shorts, average net −37.7 bp |
| Win rate | 48.8% full sample, 48.0% out of sample |

## 3. Hypothesis and predictions

The claim was that a week of one-sided flow in a name that trades $1–10 million a day pushes the price past the level a patient holder would set, and that the liquidity demander keeps paying for immediacy. The screen's flat revenue growth was meant to keep fundamental momentum from dominating that week. The sources are Jegadeesh (1990), Lehmann (1990), Avramov, Chordia, and Goyal (2006), and Nagel (2012). Novy-Marx and Velikov (2016) is the reason the test charges a real spread: they find short-term reversal often fails once costs are included.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Both legs have positive gross price P&L | Long +0.081 and short +0.125 per unit of starting equity | Consistent |
| Larger formation moves revert more | Mean gross position return +57 bp above the median absolute formation move, −28 bp below | Consistent |
| The concession is earned in the first two sessions of the hold | Mean signed return −2.5 bp on the first two sessions, +2.3 bp on the last two, 1,118 holds | Not consistent |
| Gross week return is higher when formation returns are more dispersed | +26 bp in the high-dispersion half of weeks, −9 bp in the low half, 258 weeks | Consistent |

The two legs and the dispersion split are full-sample facts. They sit next to an out-of-sample average gross trade of −15.2 bp. The prediction that identifies the liquidity concession, that the gain shows up immediately, does not hold. The mechanism is not confirmed.

## 4. Method

- **Data.** Daily bars, split-adjusted, not dividend-adjusted, 2016-01-04 through 2026-09-25. The user asked for an MBoum download, and that is the ingest that was run: daily bars and the vendor's splits for the screen, and nothing else. Dividend rows came back as zero for every name, so an ex-dividend drop stays in the close. Nonpositive prices: 0. The screen had 59 names. HLS, TCS, and CVO have no US OpenFIGI listing. PARK has 204 daily bars from 2025-12-03 and fell under the 252-bar floor. Those four were not replaced. The candidate list is a September 2026 snapshot. Market cap and revenue growth are not applied on each historical date. Volatility and dollar volume are.
- **Pre-registration.** `RULES.md` was locked at 18:04 UTC on 2026-09-26, sha256 `59b886851c1c`. It was not committed. The lock records git HEAD `73b746db`, and the tree was dirty. No earlier study used these names. The QQQ, SPY, and IGV studies had already reported the 2024-07-01 through 2026-09-25 equity path, including the April 2025 tariff crash and rebound, so that part of this out-of-sample window was not unseen. `counts.py`, run before the lock, counted signal slots and did not record a forward return. It counted 630 out-of-sample name-slots on the primary book, which is why the 100-trade minimum was left where the protocol puts it.
- **Fills and costs.** The signal is the week's last close. The fill is the next session's open. The base cost is 20 bp per side of the shares actually traded, plus 5% Actual/365 on short notional held overnight. Twenty basis points a side is a 40 bp effective spread, chosen as a round figure for this dollar-volume band and not estimated from these prices.
- **Returns.** Daily simple returns on the NYSE calendar. A session with no position is 0. Sharpe is the mean divided by the sample standard deviation, times √252, with a zero rate. A trade is counted in the sample that contains its entry date.
- **Verification.** The self-test passed, including a hand-computed round trip, a tie broken by ticker, a missing exit bar, a flip, the long-only book, and the same-bar close fill. `verify.py` is a separate implementation and matched all 936 primary trades on side, entry session, entry price, exit session, and exit price.
- **Runs.** One execution computed the book and then crashed on an attribute name before it wrote `results.json` or the log. The first logged run is the rerun after that fix. A second logged run stopped rewriting the entry price when a position was added to; the rules call the entry the first fill, and dollar P&L does not use that field. The headlines did not move: full Sharpe −0.4382 and −58.52%, out of sample −1.1290 and −56.58%, on both logged runs.

## 5. Results

![Growth of $1](figures/equity.svg)

The reversal book finishes the sample down 58.5%, and most of that loss is after the out-of-sample line. The equal-weight screen of the same names is up 363%. SPY's line is flat until September 2021 because the store's SPY daily bars start on 2021-09-16 and the rules record a missing print as zero.

![Drawdown](figures/drawdown.svg)

| Book (20 bp and 5% borrow) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | −58.5% / −0.44 / −61.6% | 0.01 | −56.6% / −1.13 / −59.6% |
| Long-only losers | −63.9% / −0.24 / −67.0% | −0.05 | −53.8% / −0.56 / −57.3% |
| Cross-market, same rule | +48.4% / 0.32 / −44.5% | 0.84 | −31.9% / −0.56 / −40.1% |
| *Equal-weight screen* | *+363% / 0.74 / −43.0%* | *0.91* | *+3.4% / 0.16 / −23.8%* |
| *SPY, zeros before the stored history* | *+72.7% / 0.49 / −25.4%* | *0.13* | *+62.5% / 1.20 / −19.9%* |

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Equal-weight screen | SPY as stored |
|---|---:|---:|---:|
| 2016 | 0 | +78.9% | 0 |
| 2017 | 0 | +26.4% | 0 |
| 2018 | −6.8% | −12.4% | 0 |
| 2019 | +2.6% | +31.5% | 0 |
| 2020 | 0 | +56.3% | 0 |
| 2021 | +9.5% | +42.9% | +6.2% |
| 2022 | −16.5% | −27.7% | −19.5% |
| 2023 | +9.3% | +6.4% | +24.3% |
| 2024 | −25.7% | −1.8% | +23.4% |
| 2025 | −13.5% | +4.8% | +16.4% |
| 2026 | −32.5% | +0.5% | +13.1% |

2026 is 184 sessions. SPY is zero in the table through 2020 because those bars are not in the store. The strategy is zero in 2016, 2017, and 2020 because it did not trade.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trade's dates and size and flips the sign of its price P&L. The actual gross Sharpe is 0.22. The mean of 2,000 draws is 0.09. p = 0.289. The ranking is not separated from a coin flip at the pre-registered 0.05 line.

The timing placebo keeps the eligible names and the costs and assigns the quintiles at random. The actual net Sharpe is −0.44. The mean of 500 random books is −0.66. p = 0.20. Random names lose more after costs. That is not the acceptance test, and it is not 0.05.

### Bootstrap

The 95% block-bootstrap interval of the full-sample Sharpe is −0.98 to +0.11, from 2,000 draws of 20-session blocks. The out-of-sample t-statistic of the mean daily return is −1.86.

### Parameter plateau

![Parameter grid](figures/grid.svg)

Three of the twelve in-sample cells have Sharpe above zero: 25%, against a required 60%. The primary, one week and K = 5, is one of those three, at 0.01. The other two are 0.07 and 0.07. Cells with K = 8 or K = 10 are flat in sample: a 27-name book does not produce two names per side at those cuts, so there is no variance and the Sharpe is undefined. Nothing was selected from the grid. The out-of-sample column is in `results.json` and is not a menu.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost multiplier | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.15 | −0.15 | **−0.44** | −1.01 | −1.55 |
| Out-of-sample Sharpe | −0.33 | −0.73 | **−1.13** | −1.90 | −2.63 |
| Full-sample return | +12.3% | −31.7% | **−58.5%** | −84.8% | −94.5% |

There is no break-even cost in the out-of-sample window. The zero-cost out-of-sample return is −25.8%. Waiting one extra session to fill changes the out-of-sample Sharpe from −1.13 to −0.95 and the return from −56.6% to −49.2%. Filling at the signal close, the labelled upper bound, makes the out-of-sample Sharpe −1.28.

### Other half of the screen (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|
| Cross-market, 28 names | 0.84 | −0.56 | +48.4% / 1.14 |

## 7. Where the result comes from

![By SPY-move quintile](figures/move_quintiles.svg)

The quintile edges use the full sample, including the zero SPY days before 2021-09-16, so the middle of the figure is not a clean split of market days. Bin 3 has 0 days. Bin 4 has 1,566 days and a mean SPY return of about zero. The two tails are readable and were not used as a filter. On the 538 days in the worst SPY quintile the book averaged −10.3 bp. On the 538 days in the best quintile it averaged +4.2 bp.

| Exit | Trades | Avg net | Net P&L per unit of starting equity |
|---|---:|---:|---:|
| Scheduled week-end | 737 | −133 bp | −1.84 |
| Flip to the other side | 199 | +346 bp | +1.26 |

No trade was closed by the end-of-sample rule. The flip exits are the stints whose rank reversed. They are not a separate strategy, and the rules do not allow keeping them and dropping the scheduled exits. Long gross price P&L was +0.081 and short gross price P&L was +0.125 per unit of starting equity. After costs the long net was −0.267 and the short net was −0.319.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| Out-of-sample Sharpe and profit factor | ≥ 0.5 and ≥ 1.10 | −1.129 and 0.777 | ❌ |
| Direction placebo | p ≤ 0.05 | 0.289 | ❌ |
| In-sample Sharpe, and share of grid cells above zero | > 0, and ≥ 60% | 0.009, and 25% | ❌ |
| Full-sample return at 2× cost | > 0 | −84.8% | ❌ |
| Cross-market out-of-sample Sharpe | > 0 | −0.559 | ❌ |
| Out-of-sample trades | ≥ 100 | 521 | ✅ |

The in-sample Sharpe is above zero. The line still fails because 3 of 12 grid cells are above zero. A costed placebo is not the test that was failed. The direction placebo is on gross price P&L, and that p-value is 0.29.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| The gross reversal is not there out of sample | Average out-of-sample gross trade −15.2 bp. Zero-cost out-of-sample Sharpe −0.33 | None in these rules. A lower cost does not repair a negative gross trade |
| The screen is a 2026 snapshot | Exposure 28.5% in sample, 97.1% out of sample. The equal-weight screen rose in the years the book was flat | The rolling volatility and dollar-volume filters are point-in-time. Market cap and revenue growth are not. The list was not replaced when names failed to download |
| Costs | Full-sample average gross trade +14.6 bp against a 40 bp round trip plus borrow. Out-of-sample average net trade −60.9 bp | The cost sweep is the record. It was not refit |
| The other half of the screen | Cross-market out-of-sample Sharpe −0.56 after an in-sample Sharpe of 0.84 | Reported. Not promoted |
| Short sample of the invested regime | 686 out-of-sample sessions, but the book is invested on 97% of them and the loss is in 2024, 2025, and 2026 | The bootstrap interval of the full-sample Sharpe is −0.98 to +0.11 |
| SPY benchmark before September 2021 | Stored SPY daily bars start 2021-09-16. Earlier days are zeros, which does not change the compounded SPY return and does lower the full-sample SPY Sharpe | The equal-weight screen is the benchmark that uses these names. The overlap figure is in §11 |
| Dividends | No dividend rows were stored. An ex-dividend drop looks like a loss and can put a payer into the long quintile | Disclosed before the run. Not adjusted after it |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. The long-only book, the cross-market book, either side, any grid cell, and any other cost are not substitutes. A different universe or a later window is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

These use `posthoc.json`. None of them changes §8.

- **(post hoc)** Correlation of the daily book with SPY is 0.05, and with the equal-weight screen is 0.02. The loss is not a hidden short of the screen or of the market.
- **(post hoc)** On the 1,262 sessions from 2021-09-16, when SPY daily bars exist, SPY returned +72.7% with a Sharpe of 0.71. The book returned −53.6% with a Sharpe of −0.59. The pre-registered full-sample SPY Sharpe of 0.49 is the same compounded path with zeros stuffed in front of it.
- **(post hoc)** Setting the 20 best days to zero takes the full-sample result from −58.5% to −83.6% (Sharpe −1.10). Setting the 20 worst days to zero takes it to +15.5% (Sharpe 0.17). The loss is not one day, and it is also not spread so evenly that the worst twenty days are irrelevant.
- **(post hoc)** The trailing 252 sessions returned −32.3% with a Sharpe of −1.76.
- **(post hoc)** The worst name by net P&L is JILL at −0.248 per unit of starting equity. The best is BGS at +0.085. No name was removed.

### Ideas for a new study

The exposure gap, 28.5% in sample against 97.1% out of sample, is what a screen dated September 2026 does to a backtest. A new study would need a membership list that was knowable on each date, or a paper-trading window that starts after this sample. It would not reuse this out-of-sample window, and it would not start from the price-change columns of this file. Lowering the 20 bp cost is not that study: the zero-cost out-of-sample Sharpe is −0.33.

## 12. Reproduce

From the repo root:

```bash
python research/low-liq-high-vol-mean-reversion/research/backtest.py
```

```bash
python research/low-liq-high-vol-mean-reversion/research/verify.py
```

```bash
python research/low-liq-high-vol-mean-reversion/research/posthoc.py
```

```bash
python research/low-liq-high-vol-mean-reversion/research/charts.py
```

`backtest.py` checks the rules hash, runs the synthetic self-test, and refuses to open the store if either fails. A store run appends to `RUNLOG.md`. Seeds are 20260926 for the direction placebo, 20260927 for the bootstrap, and 20260928 for the timing placebo. A rerun on the same store and the same code writes the same headlines. The store run takes about 20 seconds.

`counts.py` and `select_universe.py` are the pre-lock looks. They do not compute a forward return.

### Checklist

- `RULES.md` matches `RULES.lock`, and `results.json` carries sha256 `59b886851c1c86945b986e3dfea4f45148ebecef57eb4b7306e6d9bd5313d0a1`.
- Both logged reruns have a permitted reason. The crash that preceded them wrote no log line and no results file.
- The self-test passes. `verify.py` matched all 936 primary trades.
- The status is Rejected, which is what 5 failed lines and a met sample minimum produce.
- Post-hoc numbers are labelled and are not in §8.
- Prior exposure and the names that were dropped are in §4.
- `RULES.md` was not committed before the run.
- The MBoum download was run because the request asked for it. It wrote daily bars and splits for the screen. The study scripts do not write to `data/`.

### References

- Jegadeesh, Narasimhan (1990). Evidence of Predictable Behavior of Security Returns. *Journal of Finance* 45(3).
- Lehmann, Bruce N. (1990). Fads, Martingales, and Market Efficiency. *Journal of Finance* 45(1).
- Avramov, Doron, Tarun Chordia, and Amit Goyal (2006). Liquidity and Autocorrelations in Individual Stock Returns. *Journal of Finance* 61(5).
- Nagel, Stefan (2012). Evaporating Liquidity. *Review of Financial Studies* 25(7).
- Novy-Marx, Robert, and Mihail Velikov (2016). A Taxonomy of Anomalies and Their Trading Costs. *Review of Financial Studies* 29(1).
