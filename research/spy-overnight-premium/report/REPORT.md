# SPY overnight premium: SPY

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Rejected.** Failed 2 of 6 pre-registered tests. |
| Instruments | SPY daily bars, long every night, flat from the open to the close. QQQ is the cross-market line. IWM is reported on the same rule. |
| Data | Exits 2011-01-05 through 2026-10-01 (first close 2011-01-04), read via `agent-data/mdq.py`. Sessions with no daily bar are skipped: 2012-10-29, 2012-10-30, 2018-12-05. |
| Rules | [`research/spy-overnight-premium/research/RULES.md`](../research/RULES.md), locked 2026-10-02T19:47:53+00:00, sha256 `14f65d00f766` |
| Code | [`research/spy-overnight-premium/research/`](../research/) · 3 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The rule buys SPY at every daily close and sells at the next daily open. Out of sample, 2024-07-01 through 2026-10-01, it returned **+0.27%** after 1 bp per side (Sharpe **0.053**, profit factor **1.003**, 566 trades, max drawdown **−11.4%**). Over the same sessions, uncosted SPY close-to-close returned **+40.5%** (Sharpe 0.985, max drawdown −19.9%) and the uncosted cash session returned **+25.1%** (Sharpe 0.709, max drawdown −18.2%). The full sample, exits 2011-01-05 through 2026-10-01, returned +14.0% (Sharpe 0.133, max drawdown −29.1%, ending equity 1.140) against close-to-close ending equity 6.022.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2011-01-05 → 2026-10-01 | → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,958 | 3,392 | 566 |
| Total return | +14.03% | +13.72% | **+0.27%** |
| CAGR | 0.84% | 0.96% | 0.12% |
| Annual volatility | 10.48% | 10.94% | 7.12% |
| Sharpe | 0.133 | 0.142 | **0.053** |
| Max drawdown | −29.07% | −29.07% | −11.38% |
| Trades / profit factor | 3,958 / 1.017 | 3,392 / 1.018 | 566 / **1.003** |
| Avg net trade | 0.55 bp | 0.62 bp | 0.15 bp |
| *Close-to-close Sharpe (max DD)* | *0.754 (−34.10%)* | *0.716 (−34.10%)* | *0.985 (−19.88%)* |
| *Open-to-close Sharpe (max DD)* | *0.489 (−24.59%)* | *0.445 (−24.59%)* | *0.709 (−18.22%)* |

It failed 2 of the 6 acceptance tests written before the first run (§8). Line 6 holds (566 out-of-sample trades), so the status is Rejected.

**Why.**

1. **The gross gap is about the size of the locked cost.** The full-sample mean close-to-open simple return is 2.551 bp **(post hoc)** conversion of the stored mean). The average net trade is 0.55 bp. Out of sample the average net trade is 0.15 bp. At zero cost the same book returned +151.6% (Sharpe 0.613) and, out of sample, +12.29% (Sharpe 0.760). At 2 bp per side the full-sample return is −48.3%.
2. **Out of sample the cash session earned the price path.** Open-to-close returned +25.14% (Sharpe 0.709). Zero-cost overnight returned +12.29% (Sharpe 0.760). After the locked 2 bp per night, the strategy's compound return is +0.27% and its Sharpe is 0.053. Close-to-close returned +40.52%. The out-of-sample market rose.
3. **The full-sample net sum sits in a few 2020 nights (post hoc).** The best 10 sessions sum to more than the whole net sum. Setting them to zero leaves a compound return of −21.9% (Sharpe −0.11). The full-sample max drawdown, −29.1%, is the 2020 drawdown.
4. **The trailing year is negative while both benchmarks are positive (post hoc).** The 252-session strategy Sharpe ending 2026-10-01 is −0.54. Close-to-close is +1.12. Open-to-close is +1.04.

**Recommendation.** Do not trade it. The notional grid, QQQ, IWM, and the open-to-open delayed path stay diagnostics. The locked list of rescues stays closed: a trend filter, dropping ex-dividend nights, the turn of the month, a lower cost, and the 15:59 minute print.

## 2. The strategy

### Rules

```
On every SPY daily bar that has a following SPY daily bar:
    buy 100% of equity at that session's daily close
    sell 100% at the next session's daily open
    flat from that open until the next close

strategy return on the exit date =
    open[t] / close[t-1] - 1 - 0.0002

The open-to-close move is 0.
Exit reason: next_open.
No signal, no filter, no short.
```

- **Why every night.** The claim is that the overnight hold itself is the premium. A filter would test a different claim.
- **Why the daily open and close.** The 1-minute tape starts in 2021-09 and is a different series. The rule uses the stored daily open and the stored daily close.
- **Why 1 bp per side.** Protocol default for SPY, fixed before any overnight return in this study. The round trip is 2 bp of notional. It was not changed after the P&L.

### How it trades

| | |
|---|---|
| Sessions with a trade | 3,958 of 3,958 evaluation sessions |
| Trades per year | 252 |
| Time in market | 1.0 of evaluation sessions overnight; 0 during the cash session |
| Holding time | median 17.5 hours, mean 28.37 hours, entry stamp to exit stamp |
| Long / short | 3,958 long, 0 short. Exit reason `next_open` on all 3,958 |
| Win rate | 53.0% full sample. Average winner 38.8 bp, average loser −42.5 bp |

A Friday close to a Monday open is one trade and one 2 bp cost. The mean hold is longer than the median because a weekend is one observation.

## 3. Hypothesis and predictions

The long side holds inventory from the cash close to the next cash open. The other side will not hold inventory across the close and buys the open back. The cash-open flow is the same gap (Lou, Polk, and Skouras 2019; Cooper, Cliff, and Gulen 2008). If that hold is where this test's equity premium sits, the average night should be positive after cost, and the close-to-open mean should exceed the open-to-close mean.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Full-sample mean close-to-open simple return exceeds the full-sample mean open-to-close simple return. | Means 0.00025515 and 0.00025465. **(post hoc)** 2.551 bp and 2.546 bp. Difference 0.005 bp. | Consistent |
| 2. Average net strategy return is positive in at least 8 calendar years with at least 200 sessions. | 8 of 15 eligible years: 2013, 2014, 2017, 2018, 2019, 2020, 2021, 2024. | Consistent |

Prediction 1 holds by the locked strict inequality. The two session means are the same at a twentieth of a basis point. That score is a tie on the economic claim, recorded as consistent because the rule asked only which mean is larger. Both sessions earned about 2.55 bp per session before cost. The tradable rule still fails: 2 bp per night takes the gap.

Prediction 2 meets the bar at 8 and no more. The other eligible years have a negative mean: 2011, 2012, 2015, 2016, 2022, 2023, 2025. 2026 has 188 sessions, so it is outside the prediction, and its mean is negative. The 2024 row is the calendar year and spans the in-sample / out-of-sample cut.

The acceptance table failed. The predictions do not repair it.

## 4. Method

- **Data.** `bars(symbol, "1d", start="2011-01-04", end="2026-10-01")` for SPY, QQQ, and IWM, split adjustment left on. Each name has 3,959 daily bars, first 2011-01-04, last 2026-10-01, identical dates, no duplicates. `corporate_action` is empty, so the adjustment changes nothing and no dividend cash is added. An ex-dividend price drop lands in the overnight return. That pulls the strategy down on those nights and leaves the open-to-close benchmark untouched. The dates cannot be listed from this store. No outside calendar was added. The three calendar dates with no bar are 2012-10-29, 2012-10-30, and 2018-12-05. The trade is from a real close to the next real open. 2025-01-09 is a holiday in `mdq` and has no bar. 2021-12-31 has a bar and is included. Early closes stay in the sample. `mdq.EARLY_CLOSES` starts in 2019, so a pre-2019 early close is stamped 16:00 in `trades.csv`. The fill is still that session's daily close. The stamp does not change a return.
- **Pre-registration.** `RULES.md` fixed the every-night rule, notional 1.0, 1 bp per side, the 2024-07-01 split, the five notionals, the seeds, and the six lines before any return. The parent look was coverage and `corporate_action` only (`counts.py`, `counts.json`). `bars()` was not called before the lock. The out-of-sample window through 2026-09-25 was already used by earlier studies and is not unseen. From those reports, over 2024-07-01 through 2026-09-25, SPY buy and hold was about +41.7% (Sharpe about 1.03, max drawdown about −19%) and QQQ buy and hold was about +55.4% (Sharpe about 0.97 to 1.01). April 2025 was a crash and a rebound. 2022 sits in this study's in-sample and was a down year. This study adds four sessions, through 2026-10-01. The close-to-open versus open-to-close split was not known before this run. The 2011–2021 daily path was not a sample in the earlier studies. The post-2021 path was, mostly as a cash-session book or a multi-day hold. `qqq-intraday-trend`, `intraday-channel-trend`, `qqq-atr-scale-in`, `qqq-bollinger-adding`, `qqq-atr-band-dip-eod`, and `igv-small-account-fade` are flat overnight. `spy-rsi2-dip-buy` holds for days, so its P&L mixes gaps and cash sessions. `qqq-15m-turtle-overnight` holds some gaps chosen on 15-minute bars, not every night and not this daily series. This rule is only the gap. `RULES.md` was not committed before the run. The request forbade a commit. `git_head` at the lock and at the run was `8068750d3850669df5441a076ec108499b4acd44`, dirty.
- **Fills and costs.** Buy the daily close (market on close). Sell the next daily open (market on open). Entry stamp 16:00 America/New_York, or 13:00 when the entry date is in `mdq.EARLY_CLOSES`. Exit stamp 09:30. There is no earlier print than the close in this rule, so there is no same-bar upper bound. Cost is 1 bp of notional per side, both sides, including a zero gap. The protocol default is the source. This report does not remeasure the spread.
- **Returns.** Daily simple returns on the evaluation sessions, which are every session after the first bar. The book is in the market every one of those nights, so a flat row does not occur. Sharpe is the mean divided by the sample standard deviation (`ddof = 1`) times √252, risk-free rate 0. CAGR uses a 252-session year. Profit factor is dollar P&L compounded from equity 1 inside the slice. Slice metrics restart at equity 1. The trade that leaves at the 2024-07-01 open is out of sample.
- **Look-ahead.** The decision does not use a price. The close is the fill, known at the closing auction, not at the daily bar's 09:30 timestamp. The next open is known at the next opening auction. Quintile bins are a table after the run.
- **Verification.** The self-test ran before the store opened and passed: a normal night, Sandy and the 2018 mourning session each as one trade, the last close not entered, cost on both sides, notional 1.5 on a flat gap, the delayed path kept off the primary, the 2024-11-29 early-close stamp, the sample cut, and a bad high. `verify.py` does not import `backtest.py`. It matched all 3,958 SPY trades on side, entry time, entry price, exit time, exit price, gross, net, and reason, and it matched `strategy_net` on all 3,958 sessions. The explicit sample of 40 (seed 20261043) matched.
- **Runs.** One pre-registered store run, 2026-10-02T19:55:51+00:00, reason `initial pre-registered run`. One verification replay, 2026-10-02T19:55:57+00:00. One post-hoc store read, 2026-10-02T19:59:40+00:00, which counted daily-versus-minute price gaps and computed no strategy return. No bug fix changed a headline. `charts.py` reads the output files and does not open the store.

## 5. Results

![Growth of $1](figures/equity.svg)

The strategy ends at 1.14, close-to-close at 6.02, and open-to-close at 2.39. The dashed line is 2024-07-01. The full sample is on the figure.

![Drawdown](figures/drawdown.svg)

The strategy's deepest hole is −29.1%, in 2020. Out of sample, restarted, the hole is −11.4%. Close-to-close's full-sample hole is −34.1%.

| Strategy (1 bp/side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **SPY primary, notional 1** | +14.03% / 0.133 / −29.07% | 0.142 | +0.27% / 0.053 / −11.38% |
| SPY, zero cost | +151.6% / 0.613 / — | 0.603 | +12.29% / 0.760 / — |
| SPY delayed, open to next open | +173.8% / 0.473 / −32.59% | 0.440 | +24.78% / 0.659 / −21.26% |
| QQQ, same rule | +124.0% / 0.486 / −31.44% | 0.425 | +22.29% / 0.934 / −7.73% |
| IWM, same rule | +117.1% / 0.448 / −29.61% | 0.435 | +13.06% / 0.551 / −10.98% |
| *SPY close-to-close* | *+502.2% / 0.754 / −34.10%* | *0.716* | *+40.52% / 0.985 / −19.88%* |
| *SPY open-to-close* | *+139.3% / 0.489 / −24.59%* | *0.445* | *+25.14% / 0.709 / −18.22%* |

Zero-cost max drawdown was not stored. The delayed path is the pre-registered check in §6. It is not the primary. QQQ and IWM are the same rule on their own daily bars. QQQ's out-of-sample Sharpe is acceptance line 5. IWM is not.

![Calendar-year return](figures/by_year.svg)

Eight calendar years have a negative compound strategy return, including 2022, 2025, and the 188-session 2026. 2024 is the largest positive year, Sharpe 1.96, and that year contains both sides of the split. In 2025 the strategy returned −2.66% while close-to-close returned +16.4%. In 2022 both lost about 19%.

| Year | Sessions | Strategy | Sharpe | Max DD | Close-to-close | Open-to-close |
|---|---:|---:|---:|---:|---:|---:|
| 2011 | 250 | −3.17% | −0.16 | −15.0% | −1.17% | −2.91% |
| 2012 | 250 | −1.04% | −0.09 | −7.7% | +13.5% | +9.07% |
| 2013 | 252 | +6.43% | 0.99 | −3.3% | +29.7% | +15.9% |
| 2014 | 252 | +3.18% | 0.53 | −4.9% | +11.3% | +2.56% |
| 2015 | 252 | −4.23% | −0.36 | −9.8% | −0.81% | −1.52% |
| 2016 | 252 | −8.97% | −1.01 | −13.1% | +9.64% | +14.5% |
| 2017 | 251 | +5.61% | 1.35 | −2.5% | +19.4% | +7.51% |
| 2018 | 251 | +7.02% | 0.85 | −4.0% | −6.35% | −16.8% |
| 2019 | 252 | +7.72% | 0.98 | −8.4% | +28.8% | +13.7% |
| 2020 | 253 | +7.65% | 0.42 | −29.1% | +16.2% | +2.59% |
| 2021 | 252 | +10.3% | 1.34 | −3.0% | +27.0% | +9.50% |
| 2022 | 251 | −18.9% | −1.46 | −23.3% | −19.5% | −5.58% |
| 2023 | 250 | −0.81% | −0.07 | −9.8% | +24.3% | +19.2% |
| 2024 | 252 | +16.9% | 1.96 | −5.3% | +23.4% | +0.36% |
| 2025 | 250 | −2.66% | −0.38 | −8.2% | +16.4% | +13.7% |
| 2026 | 188 | −6.09% | −1.36 | −8.5% | +12.0% | +14.9% |

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo flips the sign of each full-sample gross return, 2,000 draws, seed 20261041. The null mean Sharpe is 0.001. The null 95th percentile is 0.437. The actual gross Sharpe is 0.613. Eighteen draws were at least that large. p = 0.0095. The figure's line is that gross Sharpe. The costed full-sample Sharpe is 0.133, and the placebo does not test it. A pass on the gross Sharpe with a fail on line 1 means the uncapped gap is positive and the locked cost removes it from the decision. The timing placebo was not run. Every evaluation session is already a trade with the same exit, so a random entry with the same count is the same strategy. That reason is in `RULES.md`, written before the lock.

### Bootstrap

Circular blocks of 20 sessions, 2,000 draws, seed 20261042, on the full-sample net returns. The 2.5 / 50 / 97.5 percentiles of Sharpe are −0.313 / 0.146 / 0.647. The interval contains 0 and contains 0.5. The full-sample t-stat of the mean net return is 0.53. The out-of-sample t-stat is 0.079.

### Parameter plateau

![Parameter grid](figures/grid.svg)

The grid is notional in {0.5, 0.75, 1.0, 1.25, 1.5} times equity. Cost stays 1 bp per side and scales with notional. All five in-sample Sharpes are 0.14247, differing only in the last stored digits. All five out-of-sample Sharpes are 0.05272 in the same way. The compound returns are not the same. In-sample rank by the stored Sharpe, with the notional tie-break the script applied, puts 0.5 first and the primary (1.0) second. The correlation of the five in-sample Sharpes with the five out-of-sample Sharpes is 0.999. That correlation is five copies of one pair plus float noise. It is not a rank that could have moved. Line 3 passes because this one series has an in-sample Sharpe above 0, five times. Nothing was selected. The primary stays 1.0. No cell has an out-of-sample Sharpe of 0.5 or higher. At notional 1.5 the out-of-sample return is −0.028%.

| Notional | IS Sharpe | IS return | OOS Sharpe | OOS return | IS rank |
|---|---:|---:|---:|---:|---:|
| 0.5 | 0.14247 | +8.84% | 0.05272 | +0.28% | 1 |
| 0.75 | 0.14247 | +11.82% | 0.05272 | +0.31% | 4 |
| **1.0** | **0.14247** | **+13.72%** | **0.05272** | **+0.27%** | **2** |
| 1.25 | 0.14247 | +14.45% | 0.05272 | +0.16% | 3 |
| 1.5 | 0.14247 | +13.99% | 0.05272 | −0.028% | 5 |

### Costs and latency

![Cost sensitivity](figures/costs.svg)

Sharpe falls through zero between 1 bp and 2 bp per side. At 2 bp per side the full-sample return is −48.3% and the out-of-sample Sharpe is −0.655. The break-even cost, where the compound return crosses to zero or below, is 1.166 bp per side on the full sample and 1.024 bp per side out of sample. The search cap was 50 bp. Both crossings are inside the cap.

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.613 | 0.373 | **0.133** | −0.348 | −0.829 |
| OOS Sharpe | 0.760 | 0.406 | **0.053** | −0.655 | −1.362 |
| Full-sample return | +151.6% | +69.4% | **+14.03%** | −48.3% | −76.6% |

The delayed path buys the next open and sells the open after that, so the held interval includes a cash session. It is pre-registered and it has no acceptance line. On SPY it returned +173.8% full sample (Sharpe 0.473, max drawdown −32.6%, profit factor 1.083, 3,957 trades) and +24.8% out of sample (Sharpe 0.659, max drawdown −21.3%, profit factor 1.126). Out-of-sample open-to-close Sharpe on the primary book is 0.709. The delayed result is the cash session plus a gap, in a window already known to be a rising market. It is not a candidate. There is no fill earlier than the close to call an upper bound.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / Sharpe / PF / max DD |
|---|---:|---:|---|
| SPY (primary) | 0.142 | 0.053 | +14.03% / 0.133 / 1.017 / −29.07% |
| QQQ (line 5) | 0.425 | 0.934 | +124.0% / 0.486 / 1.089 / −31.44% |
| IWM (reported) | 0.435 | 0.551 | +117.1% / 0.448 / 1.085 / −29.61% |

QQQ out of sample: return +22.3%, profit factor 1.189, average net trade 3.76 bp, max drawdown −7.7%, 566 trades. QQQ close-to-close out of sample returned +55.2% (Sharpe 0.966, max drawdown −24.2%). Line 5 passes on QQQ's stored daily bars. QQQ does not replace SPY. From 2024-05-31 those QQQ daily closes disagree with the minute print on the count in §11, and 2024-05-31 is inside the in-sample window. The line uses the stored daily series.

IWM out of sample: return +13.1%, profit factor 1.103, Sharpe 0.551, max drawdown −11.0%. IWM open-to-close over the full sample returned −25.6% (Sharpe −0.017, max drawdown −62.5%). Out of sample that cash session returned +8.8% (Sharpe 0.284). IWM does not take QQQ's place on line 5, and it does not take SPY's place as the primary.

Pearson correlation of `strategy_net`: SPY–QQQ 0.932, SPY–IWM 0.919, QQQ–IWM 0.826. The three books are one overnight bet.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Quintile 1 is the worst close-to-close sessions. The strategy's mean return is negative in quintiles 1 and 2 and positive in 3, 4, and 5. The gap is part of the close-to-close move, so the alignment is partly mechanical. Correlation of `strategy_net` with close-to-close is 0.642. Correlation with open-to-close is 0.038. None of this was a filter.

| Quintile | Sessions | Mean strategy | Mean close-to-close | Mean open-to-close |
|---|---:|---:|---:|---:|
| 1 | 792 | −0.484% | −1.368% | −0.905% |
| 2 | 792 | −0.112% | −0.280% | −0.187% |
| 3 | 791 | +0.011% | +0.071% | +0.041% |
| 4 | 792 | +0.109% | +0.456% | +0.328% |
| 5 | 791 | +0.506% | +1.379% | +0.851% |

By side: 3,958 long, 0 short. By exit reason: 3,958 `next_open`.

Weekday of the exit date. Monday is the weekend gap when the previous bar was a Friday. The file does not store a count of how many Mondays that is. Compound strategy return and Sharpe: Monday −24.5% / −0.66 (740 sessions); Tuesday +43.3% / 1.16 (814); Wednesday +25.3% / 0.79 (812); Thursday −7.1% / −0.19 (798); Friday −9.4% / −0.27 (794). Skipping Monday is not a rule of this study.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS Sharpe and profit factor | Sharpe ≥ 0.5 and PF ≥ 1.10 | Sharpe 0.053, PF 1.003 | ❌ |
| 2. Direction placebo | p ≤ 0.05 on full-sample gross Sharpe | p = 0.0095, gross Sharpe 0.613 | ✅ |
| 3. IS Sharpe and grid | IS Sharpe > 0 and at least 60% of cells > 0 | IS Sharpe 0.142, 5 of 5 cells | ✅ |
| 4. Full-sample return at 2× cost | Return > 0 at 2 bp per side | −48.3% | ❌ |
| 5. QQQ, identical rules | OOS Sharpe > 0 | 0.934 | ✅ |
| 6. OOS sample | ≥ 100 trades | 566 | ✅ |

Line 6 holds, and lines 1 and 4 fail, so the status is Rejected. A pass on line 2 does not say the costed strategy cleared a hurdle. The placebo is on gross returns. A pass on line 3 is five scales of one series, each with the same positive in-sample Sharpe. A pass on line 5 is QQQ's stored daily bars. IWM's out-of-sample Sharpe is 0.551 and was never this line.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

The rolling figure is **(post hoc)**. Of 3,707 windows of 252 sessions, the strategy Sharpe is positive in 60.1%. The minimum is −1.81 and the maximum is 3.31. The window ending 2026-10-01 is −0.54. Close-to-close is positive in 88.9% of windows and ends at +1.12. Open-to-close ends at +1.04.

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | OOS Sharpe 0.053 and t-stat 0.079. Trailing 252-session Sharpe −0.54 **(post hoc)**. 2025 and 2026 compounds are negative while close-to-close is positive. | None in the rule. The sample split stays. |
| Concentration in a few days | Best 10 sessions, all before the late-2024 daily-bar defect, have share of the net sum 1.77. Zeroing them: return −21.9%, Sharpe −0.11 **(post hoc)**. 2020 max drawdown equals the full-sample max drawdown. | None. The rule holds every night, including March 2020. |
| Generalization | SPY failed lines 1 and 4. QQQ's line-5 pass is a different symbol and is not the primary. The three `strategy_net` series correlate above 0.82. | QQQ and IWM stay reported. |
| Execution | Break-even is 1.17 bp per side full sample and 1.02 bp per side out of sample, against a locked 1 bp. At 2 bp per side the full-sample return is −48.3%. Dividends are omitted and hit this side of the book. Part of the out-of-sample daily close is not the cash auction (§11). | Cost stays 1 bp. Fills stay the stored daily open and close. |
| Short sample / regime coverage | 566 out-of-sample trades clear line 6. The window was already seen through 2026-09-25, and it was a rising market. 2022 is inside the in-sample window: strategy −18.9%, close-to-close −19.5%. The bootstrap interval on full-sample net Sharpe contains 0. | The floor was not lowered. |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A dividend-adjusted series, a later window, or a cash-auction daily history is a new study with its own `RULES.md`. This study does not switch the primary to QQQ or IWM, does not adopt the delayed path, and does not take a notional off the grid.

## 11. Post hoc (not part of the verdict)

**(post hoc)** The two prediction-1 means convert to 2.551 bp and 2.546 bp. The difference is 0.00496 bp. The score in §3 stays consistent.

**(post hoc)** Concentration. The sum of `strategy_net` is 0.2183. The best five exit dates are 2020-03-13 (+6.02%), 2020-03-24 (+5.12%), 2020-11-09 (+3.92%), 2020-04-06 (+3.87%), and 2020-03-10 (+3.78%). The worst five are 2020-03-16 (−10.47%), 2020-03-09 (−7.47%), 2020-03-12 (−6.71%), 2020-03-18 (−6.57%), and 2015-08-24 (−5.15%). The best 10 sum to 0.386, share of the net sum 1.77; zeroing them leaves compound return −21.9% and Sharpe −0.107. The best 20 have share 2.89; zeroing them leaves −38.6% and Sharpe −0.268. Those dates are before the daily-versus-minute disagreement below. This does not change a fill.

**(post hoc)** Negative compound years, from the year table: 2011, 2012, 2015, 2016, 2022, 2023, 2025, 2026. The positive compound years are the eight years named in prediction 2.

**(post hoc)** Rolling 252-session Sharpe is in §9 and in `rolling_sharpe.csv`. It does not enter §8.

**(post hoc)** Daily bar versus the 1-minute print, price only, relative gap above 20 bp, from 2021-09-27 through 2026-10-01. No strategy return was computed from the minute tape.

| | Compared | No minute bar | Open > 20 bp | Close > 20 bp | First close date | Max abs close |
|---|---:|---:|---:|---:|---|---|
| SPY | 1,254 | 5 | 224 (first 2024-11-18) | 111 (first 2024-11-20) | 2024-11-20 | 349 bp on 2025-04-02 |
| QQQ | 1,254 | 5 | 281 (first 2024-11-18) | 169 (first 2024-05-31) | 2024-05-31 | 440 bp on 2025-04-02 |

SPY mean absolute gaps are 10.2 bp on the open and 7.1 bp on the close. The max absolute open gap is 230 bp on 2026-03-23. QQQ mean absolute gaps are 13.7 bp on the open and 9.0 bp on the close. The max absolute open gap is 271 bp on 2025-04-04. The five sessions with no minute bar are not listed in `posthoc.json`. Out of sample starts 2024-07-01. SPY's daily close leaves the minute print on 2024-11-20, so part of the out-of-sample window is not a cash-auction gap. The days that carry the full-sample net sum are in 2020, before that break. The fills stay the stored daily open and close. No second Sharpe was computed on a 15:59 print, and no clean-versus-dirty split of the out-of-sample window was computed.

### Ideas for a new study

- The same every-night rule on a total-return series, with dividend cash on the ex-date. This store has no dividend rows for SPY, QQQ, or IWM. The sample cannot be this price sample.
- A regular-hours daily open and close for 2011 through 2021 from a source other than these stored daily bars. The minute tape does not cover that decade, and substituting the 15:59 print inside this sample was ruled out before the lock.
- QQQ as a primary, same rule, on sessions after 2026-10-01. The path through 2026-10-01 is already measured here, including the line-5 pass.

Lowering the cost, skipping Mondays, dropping ex-dividend nights, and trading only the turn of the month are not new studies suggested by unused data. They were named in `RULES.md` as things this study would not promote.

## 12. Reproduce

From the repo root:

```bash
python research/spy-overnight-premium/research/backtest.py
```

```bash
python research/spy-overnight-premium/research/verify.py
```

```bash
python research/spy-overnight-premium/research/posthoc.py
```

```bash
python research/spy-overnight-premium/research/charts.py
```

`backtest.py` checks the `RULES.md` hash, runs the self-test, then writes `results.json`, `daily.csv`, `trades.csv`, and `placebo_direction.npy`, and appends `RUNLOG.md`. `verify.py` rewrites nothing except a `RUNLOG.md` line. `posthoc.py` writes `posthoc.json` and `rolling_sharpe.csv` and appends `RUNLOG.md`. `charts.py` writes the SVG files in `report/figures/`. Seeds are 20261041, 20261042, and 20261043. A rerun on the same store and the same code reproduces the same numbers. The logged backtest and the logged verification are six seconds apart (19:55:51Z and 19:55:57Z). The post-hoc read is logged at 19:59:40Z.

`rules_sha256` in `results.json` is `14f65d00f766db9b441c279b85830b9c8e3dd7ec95594904aa0220cbc7afc796`, the same value as `RULES.lock`.

Checklist miss: `research/README.md` does not list this study. The request forbade any edit outside `research/spy-overnight-premium/`. The study README is in place. `RULES.md` was not committed. Nothing was written to `data/`. `ingest` was not run. The strategy was not ported into `apps/terminal`.

### References

- Lou, Dong, Christopher Polk, and Spyros Skouras (2019). A tug of war: Overnight versus intraday expected returns. *Journal of Financial Economics*.
- Cooper, Michael J., Michael T. Cliff, and Huseyin Gulen (2008). Return differences between trading and non-trading hours: Like night and day. Working paper.
