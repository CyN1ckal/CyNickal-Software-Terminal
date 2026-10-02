# Commodity ETF momentum: GLD, SLV, USO, UNG, DBA, DBB

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Inconclusive.** Line 6 is 21 OOS round trips; the floor is 24. Line 3 is 2 of 5 IS grid cells with Sharpe above 0; the floor is 3. |
| Instruments | GLD, SLV, USO, UNG, DBA, DBB. Monthly rank, next-open fill, top 2 long and bottom 2 short, held to the next fill |
| Data | Daily bars 2011-01-04 → 2026-10-01, evaluated 2012-02-01 → 2026-10-01, read via `agent-data/mdq.py`. Book sessions omit 2012-10-29, 2012-10-30, and 2018-12-05 |
| Rules | [`research/commodity-etf-momentum/research/RULES.md`](../research/RULES.md), locked 2026-10-02 19:50:06 UTC, sha256 `0a269fcb4d44` |
| Code | [`research/commodity-etf-momentum/research/`](../research/) · 2 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Out of sample, 2024-07-01 through 2026-10-01, the book returned **+41.5%** after costs (Sharpe **0.574**, profit factor **2.26**, max drawdown **−37.0%**, t-stat **0.860**, **21** round trips, 566 sessions). The uncosted equal-weight long book of the same six names returned +47.0% in that window (Sharpe 0.967, max drawdown −15.4%). Over the full evaluation window, 2012-02-01 through 2026-10-01, the strategy returned +95.2% (Sharpe 0.310, max drawdown −65.7%, 135 trades). In sample, through 2024-06-28, it returned +38.0% (Sharpe 0.247, max drawdown −65.7%, 114 trades).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2012-02-01 → 2026-10-01 | → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,688 | 3,122 | 566 |
| Total return | +95.2% | +38.0% | **+41.5%** |
| CAGR | 4.7% | 2.6% | 16.7% |
| Annual volatility | 36.4% | 34.1% | 46.8% |
| Sharpe | 0.310 | 0.247 | **0.574** |
| Max drawdown | −65.7% | −65.7% | −37.0% |
| Trades / profit factor | 135 / 1.27 | 114 / 1.06 | **21 / 2.26** |
| Avg net trade | 140 bp | 74 bp | 499 bp |
| *Equal-weight long, Sharpe (max DD)* | *0.231 (−62.5%)* | *0.109 (−62.5%)* | *0.967 (−15.4%)* |

Returns, CAGR, volatility, and drawdowns above are the stored fractions printed to one decimal. Sharpes and the t-stat are the stored values to three decimals. The acceptance table in §8 quotes the stored values.

Lines 1, 2, and 4 pass. Lines 3 and 6 fail. Line 5 is not in the verdict. The map locked in `RULES.md` sets the status to **Inconclusive** when line 6 is missed, and still reports the other lines. With the sample floor met, line 3 would have set the status to **Rejected**. These numbers have no path on that map to Paper-trading candidate.

**Why.**

1. **The OOS trade count is 21.** The floor is 24. Twenty-eight OOS sessions had a fill. A same-sign resize stays inside the open round trip, so fill sessions and round trips are different counts. The four `end` trades are included in the 21.
2. **Two of five IS formation windows have Sharpe above 0.** The floor is three. K=63, 126, and 189 have IS Sharpe −0.242, −0.123, and −0.005, and IS total returns −126.9%, −163.9%, and −271.1%. K=252 and K=315 are the two positive IS cells. OOS Sharpe is positive in all five cells. Nothing was selected from the grid.
3. **Short-side gross P&L is −0.239.** Long-side gross P&L is +1.265. Prediction 1 required both to be positive. At zero cost the full-sample Sharpe is 0.321 and the full-sample return is +107.3%. At 3× cost the full-sample return is still +73.1%. The short book is negative before costs.
4. **Removing DBA, or removing DBB, makes full-sample gross P&L negative.** Dropping DBA leaves −2.192. Dropping DBB leaves −1.791. Prediction 2 required every leave-one-out sum to stay positive. Those two names' own net P&L is +0.031 and −0.019. Removing a name re-ranks the other five.
5. **The OOS equal-weight book has the higher stored return, Sharpe, and a smaller stored drawdown.** Strategy OOS return +41.5%, Sharpe 0.574, max drawdown −37.0%. Benchmark OOS return +47.0%, Sharpe 0.967, max drawdown −15.4%. **(post hoc)** Daily correlation of strategy net with that book is −0.016.

**Recommendation.** Do not trade it. There is no paper-trading proposal. The locked list stays off the verdict: dropping a name, keeping one side, a top-1 book, a one-month skip, volatility scaling, any grid cell other than K=252, the same-bar close fill, a different cost, the futures sleeve, and a moved sample split.

## 2. The strategy

### Rules

```
On the last NYSE session of each month that has a SPY daily bar:
    formation = close[t] / close[t-252] - 1 on each name's own bars
    if any of the six is not rankable, skip the rebalance
    sort by formation descending; exact ties break by symbol ascending
    +1/2 on each of the top 2, -1/2 on each of the bottom 2, middle 2 flat
Fill at the next open that has a bar.
Old shares earn the gap into that open.
Size off mark-to-open equity before cost.
Charge 1 bp per side on GLD and SLV, 5 bp per side on USO, UNG, DBA, and DBB.
New shares earn open-to-close.
A missing bar earns 0 that session and is not a forward-filled signal.
On 2026-10-01, discard that day's signal and flatten at the close.
```

- **Why 252 sessions and no skip:** the 12-month commodity ranking window in Erb and Harvey (2006) and Miffre and Rallis (2007), including the most recent month. The equity one-month skip is a different rule.
- **Why book size 2:** with six names a published quintile is 1.2 names. Two is the smallest book that is not a single-name bet.
- **Why these costs:** 1 bp is the SPY-class default for GLD and SLV. 5 bp on the other four is a round prior. The store has no quotes. The rate was not revised after P&L.
- **Why the split:** 2024-07-01 is the repo's existing OOS start. It was fixed before any return on these series. The window overlaps `micro-futures-trend` and is not unseen (§4).

### How it trades

| | |
|---|---|
| Sessions with a fill | 177 full, 149 in sample, 28 out of sample |
| Trades per year | 9.22 |
| Time in market | 1.0 of evaluation sessions after the open's fills |
| Holding time | median 43 sessions, mean 109.3. For `flip` and `flat` the count is exit index minus entry index. For `end` it includes the exit session. `RULES.md` did not define the session count; this is the count the script wrote |
| Long / short | 68 long, net P&L +1.234, profit factor 1.64, win rate 0.382, average 277 bp. 67 short, net P&L −0.282, profit factor 0.83, win rate 0.463, average +0.25 bp |
| Win rate | 0.422 of 135 trades. Average winner net P&L +0.0792. Average loser net P&L −0.0457. A zero net is neither |

Trade bp uses opening-fill notional as the denominator. A later resize stays in the same trade, so dollar P&L and average bp can disagree in sign. The short side does that: dollar net is −0.282 and average bp is +0.25. The OOS average of 499 bp uses the same denominator.

## 3. Hypothesis and predictions

The claim is that, among these six ETFs, the two with the highest prior 12-month price return outperform the two with the lowest over the next month. Miffre and Rallis (2007) attribute commodity-futures momentum to slow hedgers and to underreaction. The other side is a hedger, or a rebalancer, leaning against the move. That paper also finds that the profitable books buy backwardation and sell contango. This study does not observe the curve and does not add that filter. USO and UNG embed a futures roll. Their long public downtrends were known before the lock. They stayed in the universe, and the rule is allowed to be short them.

The P&L lines that pass do not confirm that mechanism. Both predictions fail.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Full-sample gross P&L positive on the longs and positive on the shorts | Long gross +1.265. Short gross −0.239 | Not consistent |
| Leave-one-out full-sample gross P&L stays positive as each name is removed | GLD +0.290, SLV +0.348, USO +1.164, UNG +0.077, DBA −2.192, DBB −1.791 | Not consistent |

## 4. Method

- **Data.** Daily bars through `agent-data/mdq.py`, split-adjusted. Before the lock, `counts.py` recorded coverage only. Each of the six, and SPY, has 3,959 daily bars from 2011-01-04 through 2026-10-01, no duplicate sessions, no non-positive OHLC, and no zero-volume bars. In that window each name's sessions match SPY. Coverage has 4,108 rows: 4,103 complete, 4 missing, 1 partial. The partial row is 2021-12-31, a real bar. The missing row 2025-01-09 is already outside `nyse_sessions`. The missing rows 2012-10-29, 2012-10-30, and 2018-12-05 sit inside `nyse_sessions` and have no bars. The book skips those three dates. Stored actions are USO 0.125 on 2020-04-29 and UNG 0.25 on 2018-01-05 and 2024-01-24. No dividends are stored. The pre-lock jump check, ex-date close over the previous close: USO 2020-04-29 raw 8.451 versus adjusted 1.056; UNG 2018-01-05 raw 3.894 versus adjusted 0.974; UNG 2024-01-24 raw 4.187 versus adjusted 1.047. `backtest.py` re-checks raw ratio above 3 and adjusted ratio between 0.5 and 1.5, and that check passed. The engine does not invent a dividend or a roll adjustment the store does not store.
- **Pre-registration.** `RULES.md` fixed the universe, K=252, book size 2, weights, costs, the month-end signal, the next-open fill, the accounting, the samples, the benchmark, the grid, the placebos, the bootstrap, and the verdict map. Prior exposure, written before any return on these series: no earlier study used these six symbols. `micro-futures-trend` (Rejected) was time-series momentum on micro futures, open-to-close, with equities and rates in the same book. Its commodity sleeve was gold, copper, and crude, evaluated 2022-10-03 through 2026-09-25, OOS from 2024-10-01. From that study's rules and report: gold rallied through 2024–2025; crude drifted lower over 2023–2025; the commodity sleeve was IS Sharpe −0.64 and OOS Sharpe +0.88. This study is cross-sectional, ETF, 12-month only, dollar-neutral, and includes silver, gas, agriculture, and base metals. Equity studies in the repo used an OOS start of 2024-07-01; from those reports, QQQ buy-and-hold was about +55% in a window that contains the April 2025 tariff crash and rebound. Those are not these ETFs. `RULES.md` was locked at 2026-10-02T19:50:06Z at git `8068750d3850669df5441a076ec108499b4acd44` and was not committed. The working tree was dirty. The first store run was 2026-10-02T20:03:09Z.
- **Fills and costs.** The primary fill is the next open. One extra session of delay, and a same-bar close fill labelled as an upper bound, are reported in §6 and are not the verdict. GLD and SLV pay 1 bp per side. USO, UNG, DBA, and DBB pay 5 bp per side. No borrow and no cash interest. The store has no borrow quotes. Charging zero borrow is the locked limitation.
- **Returns.** Daily simple returns on evaluation sessions. A flat day inside the window counts as 0. Sharpe is mean divided by sample standard deviation (ddof=1), times √252, with a zero risk-free rate. CAGR uses a 252-session year. Max drawdown compounds that window's own daily returns from 1. A trade is in a sample by its entry date. Daily Sharpe uses the session date. An IS entry that spans 2024-07-01 stays one IS trade, and its later sessions sit in the OOS daily Sharpe.
- **Verification.** The self-test runs before the store opens. It covers a clean rank, an exact tie, a missing bar, a smooth series across a labelled reverse-split date, a flip, a discarded last signal, and a superseded pending target. `verify.py` shares no signal code with `backtest.py`. It matched 135 of 135 trades on symbol, side, entry and exit date and price (1e-6), and exit reason, and matched gross and net within 1e-6. Seed 20261024 drew 40 entry dates covering 53 of those trades. The pass condition is the full list.
- **Runs.** One launch imported `backtest.py` with `ROOT` set one directory above the repo, raised `ModuleNotFoundError: No module named 'mdq'`, and exited before the store opened. It computed no return and wrote no `RUNLOG` line. `ROOT` is `Path(__file__).resolve().parents[3]` in the file that ran. The 20:03:09 UTC run is the first store run, reason `initial pre-registered run`. The 20:05:31 UTC run is `verify.py`, reason `verification, independent rebuild of every trade`. No headline was replaced after results were seen. The short-K grid cells with IS total return below −1 are the locked sizing rule, equity with no floor, and the script stored those returns. The primary path's status is Inconclusive, not Void.

`research/README.md` was not updated. The request forbade editing that file. That is the checklist item this study leaves undone. The rules hash in `RULES.lock`, `results.json`, and both `RUNLOG` entries is `0a269fcb4d443675fdf2f5d6b96c75bfdccc93742152739ee67d5c0c9535e7e2`. Nothing was written to `data/`. Ingest was not run.

## 5. Results

![Growth of $1](figures/equity.svg)

The full path compounds to a stored total return of +95.2%, and the vertical line is 2024-07-01. The full-sample max drawdown, −65.7%, is the same number as the in-sample max drawdown. From that date the uncosted equal-weight book has the higher stored Sharpe.

![Drawdown](figures/drawdown.svg)

| Strategy (base cost) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary, K=252, top 2 / bottom 2** | +95.2% / 0.310 / −65.7% | 0.247 | +41.5% / 0.574 / −37.0% |
| *Equal-weight long, uncosted* | +52.8% / 0.231 / −62.5% | 0.109 | +47.0% / 0.967 / −15.4% |

There is no secondary.

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | Benchmark |
|---|---:|---:|---:|---:|
| 2012 (from Feb, 230 sessions) | −15.5% | −0.618 | −28.8% | +38.7% |
| 2013 | +9.5% | 0.467 | −23.0% | −12.5% |
| 2014 | −7.5% | −0.144 | −23.0% | −16.0% |
| 2015 | +40.6% | 1.284 | −15.1% | −25.6% |
| 2016 | −11.2% | −0.166 | −39.3% | +12.4% |
| 2017 | +18.4% | 0.952 | −16.5% | +0.2% |
| 2018 | −13.3% | −0.335 | −22.3% | −7.3% |
| 2019 | −15.9% | −0.397 | −37.0% | +4.3% |
| 2020 | +100.4% | 1.622 | −17.2% | −7.2% |
| 2021 | −27.1% | −0.857 | −30.6% | +23.0% |
| 2022 | +26.0% | 0.733 | −43.6% | +11.8% |
| 2023 | −20.7% | −0.439 | −35.8% | −11.6% |
| 2024 (straddles the split) | −2.5% | 0.150 | −37.2% | +15.3% |
| 2025 | +43.7% | 1.232 | −20.5% | +23.8% |
| 2026 (through Oct 1, 188 sessions) | +3.4% | 0.405 | −37.0% | +14.8% |

Stored yearly strategy returns are negative in 2012, 2014, 2016, 2018, 2019, 2021, 2023, and 2024. The 2024 row has a negative compounded return and a positive daily-mean Sharpe. 2020 is the largest stored yearly return, +100.4%, against an equal-weight year of −7.2%.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trade's timing and multiplies that trade's daily gross price P&L by an independent ±1. Two thousand draws, seed 20261021. The denominator is the actual book's previous net equity. Actual full-sample gross Sharpe is 0.321. The null mean is 0.007, the null standard deviation is 0.185, and the null 95th percentile is 0.304. p = (1 + number of draws at or above the actual) / 2001 = **0.040**. That passes line 2. It says the signed paths, taken together, sit in the right tail of sign flips. It does not say the short side made money, and it does not say the rank assigned names better than a shuffle.

The timing placebo is a reported check, not an acceptance line. Five hundred draws, seed 20261023, permute the six formation returns across the six names at each rebalance and resimulate at base cost. Score is full-sample gross Sharpe. Null mean −0.006, null standard deviation 0.258, p = **0.122**. Random assignment of the same cross-section is not rejected at the 0.05 bar the direction test uses.

### Bootstrap

Circular blocks of 20 evaluation sessions, 2,000 draws, seed 20261022, of full-sample net daily returns. The 2.5 percentile of draw Sharpes is **−0.187**. The median is 0.312. The 97.5 percentile is **0.798**. The interval contains zero. The OOS t-stat of the mean daily return is 0.860.

### Parameter plateau

![Parameter grid](figures/grid.svg)

| K | First fill | IS Sharpe | IS return | OOS Sharpe | OOS return |
|---|---|---:|---:|---:|---:|
| 63 | 2011-05-02 | −0.242 | −126.9% | 0.278 | +4.4% |
| 126 | 2011-08-01 | −0.123 | −163.9% | 0.542 | +36.8% |
| 189 | 2011-11-01 | −0.005 | −271.1% | 0.809 | +79.7% |
| **252** | 2012-02-01 | **0.247** | **+38.0%** | **0.574** | **+41.5%** |
| 315 | 2012-05-01 | 0.256 | +44.3% | 0.505 | +31.5% |

Two of five IS Sharpes are above 0. The K=252 IS Sharpe equals the primary IS Sharpe, 0.2469832357368081. K=315 has the highest IS Sharpe. Its OOS Sharpe is 0.505. The highest OOS Sharpe is K=189, the cell whose IS total return is −271.1%. Nothing was selected from the grid. The script did not store an IS/OOS rank correlation.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.321 | 0.315 | **0.310** | 0.298 | 0.287 |
| OOS Sharpe | 0.585 | 0.580 | **0.574** | 0.563 | 0.552 |
| Full-sample return | +107.3% | +101.1% | **+95.2%** | +83.8% | +73.1% |

Each multiple is a full resimulation. The sweep stays positive through 3×. Line 4 uses the 2× full-sample return, +83.8%, which is above 0. One extra session of delay: full Sharpe 0.254, OOS Sharpe 0.515, full return +44.5%. The same-bar close fill, labelled upper bound: full Sharpe 0.282, OOS Sharpe 0.427, full return +68.8%. On this sample the close fill's OOS Sharpe is below the next-open OOS Sharpe.

### Other markets (identical rules)

Not run. Line 5 was marked not applicable before the lock: the futures series were already used by `micro-futures-trend` and are not these ETFs, and the store has no second commodity-ETF set. The line is neither a pass nor a fail.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Months are evaluation months, sorted by the equal-weight book's monthly return. Quintile 1 is the worst benchmark months.

| Quintile | Months | Mean strategy | Mean benchmark |
|---|---:|---:|---:|
| 1 | 36 | −0.0032 | −0.0614 |
| 2 | 35 | +0.0323 | −0.0192 |
| 3 | 36 | −0.0049 | +0.0028 |
| 4 | 35 | +0.0291 | +0.0233 |
| 5 | 35 | −0.0097 | +0.0761 |

The strategy's mean month is negative in quintiles 1, 3, and 5, including the best benchmark months.

| Slice | Trades | Gross P&L | Net P&L |
|---|---:|---:|---:|
| Long | 68 | +1.265 | +1.234 |
| Short | 67 | −0.239 | −0.282 |
| Exit `flat` | 114 | +1.152 | +1.093 |
| Exit `flip` | 17 | −0.738 | −0.749 |
| Exit `end` | 4 | +0.612 | +0.608 |
| GLD | 19 (13 long) | +0.747 | +0.744 |
| SLV | 26 (15 long) | +0.633 | +0.629 |
| USO | 26 (13 long) | −0.094 | −0.114 |
| UNG | 14 (7 long) | −0.304 | −0.319 |
| DBA | 26 (13 long) | +0.047 | +0.031 |
| DBB | 24 (17 short) | −0.003 | −0.019 |

GLD and SLV are the two names with large positive own-trade net P&L. That split was not used to filter the rule. Own-trade P&L is not the leave-one-out test: removing a name re-ranks the other five, and that re-rank is what makes the DBA and DBB leave-one-out sums negative.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS Sharpe and OOS profit factor | Sharpe ≥ 0.5 and PF ≥ 1.10 | Sharpe 0.574078, PF 2.256257 | ✅ |
| 2. Direction placebo | p ≤ 0.05 | p 0.039980 | ✅ |
| 3. IS Sharpe and IS grid | IS Sharpe > 0, and at least 3 of 5 cells with IS Sharpe > 0 | IS Sharpe 0.246983. Positive cells: 2 | ❌ |
| 4. Full-sample return at 2× cost | > 0 | +0.838206 | ✅ |
| 5. Cross-market | Not applicable. Not a pass and not a fail | not applicable | — |
| 6. OOS round trips | ≥ 24, entry on or after 2024-07-01 | 21 | ❌ |

A passing direction placebo is a sign-flip of the gross paths. The timing placebo, which is the check on whether the rank matters, has p 0.122 and is not an acceptance line. Line 1's profit factor is 21 trades. The OOS t-stat is 0.860.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | **(post hoc)** Trailing 252-session Sharpe ranges from −1.92 to +2.35. The fraction of those windows with Sharpe above 0 is 0.605. The last 252 sessions, 2025-10-01 through 2026-10-01, returned +27.3% (Sharpe 0.725) against the equal-weight book's +25.5% (Sharpe 1.097) | The verdict does not use the trailing window. A later window is a new study |
| Concentration in a few days | **(post hoc)** Full return +95.2%. Zeroing the 10 best days leaves −27.5% (Sharpe 0.117). Zeroing the 10 worst days leaves +637.5% (Sharpe 0.567). Best day 2026-02-02, +0.200. Worst day 2026-01-30, −0.299. Both dates are inside the OOS window | None in the locked rule. There is no stop |
| Generalization | IS grid 2 of 5. Leave-one-out fails for DBA and for DBB. No second ETF set was tested. The futures study was a different rule and was already read | Line 5 stays not applicable. The futures sleeve is not a substitute pass |
| Execution | 5 bp is a round prior, not a measured spread. Borrow is charged at zero. The close fill's OOS Sharpe, 0.427, is below the next-open OOS Sharpe, 0.574. One session of delay leaves OOS Sharpe 0.515 | The cost sweep is the pre-registered check. It does not measure the spread |
| Short sample | 21 OOS round trips over 566 sessions. Bootstrap 95% interval of full-sample net Sharpe is −0.187 to +0.798. OOS t-stat 0.860. The OOS window overlaps a futures study that already described 2024–2025 gold and crude | Line 6 fails. The status is Inconclusive |

**(post hoc)** Of the 21 OOS-entry trades, net P&L by name is GLD +0.472 on 1 trade, SLV +0.488 on 2, USO +0.108 on 6, UNG −0.063 on 3, DBA −0.090 on 5, and DBB −0.148 on 4. Gold's 2024–2025 rally was prior exposure. This name split is not the verdict.

## 10. No deployment

There is no paper-trading proposal. The primary missed the sample test and the grid test written to decide that question. A different rule, a later window, or a wider set of names is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

The figures and tables in §6 and §7 that come from `results.json` were specified in `RULES.md` before the run. The items below were computed by `posthoc.py` after the store run. None of them changes §8.

- **Rolling Sharpe.** 252-session windows from 2013-02-01. Fraction positive 0.605. Minimum −1.916. Maximum +2.351.
- **Concentration.** Full return +0.952. Without the 10 best days, −0.275. Without the 10 worst days, +6.375. The best and worst days are both in early 2026.
- **Correlation.** Strategy net versus the equal-weight book, −0.016. Gross versus that book, −0.016.
- **Last 252 sessions.** 2025-10-01 through 2026-10-01. Strategy return +0.273, Sharpe 0.725. Benchmark return +0.255, Sharpe 1.097.
- **OOS-entry trades by name.** Listed in §9. This is trade P&L by entry sample. It is not the OOS window's compounded return. IS entries that span the boundary are excluded from this split and included in the OOS daily returns.
- **Terminal trades.** All four have OOS entries and exit only because the sample ends: SLV long from 2025-07-01, net +0.431; UNG short from 2026-01-02, net +0.077; DBA short from 2026-02-02, net −0.103; USO long from 2026-04-01, net +0.203. No GLD position is open at the flatten. These four are inside the 21 round trips.

### Ideas for a new study

The OOS equal-weight book has a higher stored Sharpe than the dollar-neutral rank. That comparison is already in this sample, on these six names. A long-only test of a commodity-ETF set would need names this study did not use. The store has no second commodity-ETF set. The backwardation filter in Miffre and Rallis needs a futures curve, which this study does not have. The micro gold, copper, and crude series were already used by `micro-futures-trend`.

## 12. Reproduce

From the repo root:

```bash
python research/commodity-etf-momentum/research/backtest.py
```

```bash
python research/commodity-etf-momentum/research/verify.py
```

```bash
python research/commodity-etf-momentum/research/posthoc.py
```

```bash
python research/commodity-etf-momentum/research/charts.py
```

`backtest.py` checks the rules hash, runs the synthetic self-test, then reads the store and writes `results.json`, `daily.csv`, and `trades.csv`. `daily.csv` has `date` and `strategy_net` on every evaluation session from 2012-02-01 through 2026-10-01. A store run appends to `RUNLOG.md`. `verify.py` rewrites nothing in `results.json` and appends its own log line. `posthoc.py` writes `posthoc.json`. `charts.py` writes the SVG files in `report/figures/` from those outputs. Seeds: direction 20261021, bootstrap 20261022, timing 20261023, verify 20261024. The same store and the same locked rules reproduce the same numbers. `counts.py` is the pre-lock coverage look and writes `counts.json`. It does not compute a return.

### References

- Erb, Claude B., and Campbell R. Harvey. 2006. "The Strategic and Tactical Value of Commodity Futures." *Financial Analysts Journal* 62 (2): 69–97.
- Miffre, Joëlle, and Georgios Rallis. 2007. "Momentum Strategies in Commodity Futures Markets." *Journal of Banking & Finance* 31 (6): 1863–1886.
