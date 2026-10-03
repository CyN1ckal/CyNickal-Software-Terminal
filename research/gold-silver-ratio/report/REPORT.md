# Gold/silver ratio: GLD and SLV

| | |
|---|---|
| Date | 2026-10-02 |
| Status | **Inconclusive.** Line 6 is 8 OOS round trips; the floor is 15. Line 1 also fails. |
| Instruments | GLD and SLV, dollar-neutral, next-open fill. Cross-market: GLD and PPLT, same rule |
| Data | Daily bars 2011-01-04 → 2026-10-01, evaluated 2011-04-25 → 2026-10-01, read via `agent-data/mdq.py`. Book sessions omit 2012-10-29, 2012-10-30, and 2018-12-05 |
| Rules | [`research/gold-silver-ratio/research/RULES.md`](../research/RULES.md), locked 2026-10-02 21:15:11 UTC, sha256 `abedf66dd1b3` |
| Code | [`research/gold-silver-ratio/research/`](../research/) · 2 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Out of sample, 2024-07-01 through 2026-10-01, the GLD/SLV pair returned **−9.5%** after 1 bp per side (Sharpe **0.220**, profit factor **0.615**, max drawdown **−70.3%**, t-stat **0.330**, **8** round trips, 566 sessions, annualized volatility **81.9%**). In sample, the first fill on 2011-04-25 through 2024-06-28, it returned +12.5% (Sharpe 0.146, max drawdown −31.0%, 47 trades). Over the full evaluation window it returned +1.7% (Sharpe 0.115, max drawdown −72.1%, 55 trades). Uncosted GLD buy-and-hold returned +78.3% out of sample (Sharpe 1.209). Uncosted SLV buy-and-hold returned +107.8% (Sharpe 0.933).

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2011-04-25 → 2026-10-01 | → 2024-06-28 | 2024-07-01 → 2026-10-01 |
| Sessions | 3,883 | 3,317 | 566 |
| Total return | +1.7% | +12.5% | **−9.5%** |
| CAGR | 0.1% | 0.9% | −4.4% |
| Annual volatility | 32.3% | 8.7% | 81.9% |
| Sharpe | 0.115 | 0.146 | **0.220** |
| Max drawdown | −72.1% | −31.0% | −70.3% |
| Trades / profit factor | 55 / 1.019 | 47 / 1.215 | **8 / 0.615** |
| Avg net trade | 14.0 bp | 31.9 bp | −91.2 bp |
| *GLD buy-and-hold, Sharpe (max DD)* | *0.451 (−45.6%)* | *0.262 (−45.6%)* | *1.209 (−27.9%)* |
| *SLV buy-and-hold, Sharpe (max DD)* | *0.197 (−76.3%)* | *−0.006 (−76.3%)* | *0.933 (−53.0%)* |

Returns, CAGR, volatility, and drawdowns above are the stored fractions printed to one decimal. Sharpe is stored to three decimals. Profit factor and average trade are stored to three decimals and one decimal. The acceptance table in §8 quotes the stored values.

Lines 2, 3, 4, and 5 pass. Lines 1 and 6 fail. The map locked in `RULES.md` sets the status to **Inconclusive** when line 6 is missed, and still reports the other lines. With the sample floor met, line 1 would have set the status to **Rejected**. These numbers have no path on that map to Paper-trading candidate.

**Why.**

1. **The OOS round-trip count is 8.** The floor is 15. All 55 trades, and all 8 OOS-entry trades, exit with reason `flat`. There is no `flip` and no `end` on the primary book. The count was not lowered after the run.
2. **OOS Sharpe is 0.220 and OOS profit factor is 0.615.** The bars are 0.5 and 1.10. OOS compounded return is −9.5%. OOS Sharpe is positive because the mean daily return is positive; annualized volatility on those 566 sessions is 81.9%. The `long_ratio` opened 2025-07-14 and closed 2026-02-06 has stored net P&L −0.263. Its gold-leg gross is +0.258 and its silver-leg gross is −0.521. Entry prices are GLD 310.52 and SLV 35.43. Exit prices are GLD 447.03 and SLV 66.82.
3. **`short_ratio` gross P&L is −0.0497.** `long_ratio` gross P&L is +0.0795. Prediction 1 required both to be positive.
4. **Gold-leg gross P&L is −0.278. Silver-leg gross P&L is +0.308.** Prediction 2 required each leg's gross, with the other removed from the sum, to stay positive. The positive dollar result is the silver leg.
5. **Both buy-and-hold books have higher OOS return and higher OOS Sharpe than the pair.** GLD OOS return +78.3%, Sharpe 1.209, max drawdown −27.9%. SLV OOS return +107.8%, Sharpe 0.933, max drawdown −53.0%. At zero cost the strategy's OOS Sharpe is 0.221. At 1 bp it is 0.220. The OOS loss is in the price path.

**Recommendation.** Do not trade it. There is no paper-trading proposal. The locked list stays off the verdict: USO, the six-ETF momentum book, a return rank, a trend overlay, a different exit or window, a sample divisor of 59, dropping a metal, keeping one side, PPLT as the primary, a moved sample, any grid cell other than E = 2, a different cost or fill, the same-bar close path, daily rebalance back to the 0.5 weights, a stop, and volatility scaling.

## 2. The strategy

### Rules

```
R = adjusted GLD close / adjusted SLV close, on sessions where both have a close
z = 0 if the population standard deviation of the last 60 ratio points is 0
    else (R - mean of those 60) / that standard deviation, divisor 60
From flat, z > 2 wants short GLD and long SLV, weights -0.5 and +0.5
From flat, z < -2 wants long GLD and short SLV, weights +0.5 and -0.5
From short, z < -2 flips; else z <= 0 exits; else hold
From long, z > 2 flips; else z >= 0 exits; else hold
Fill at the next pair session's open. One position. No add. No daily resize.
Old shares earn the gap. Size off mark-to-open equity before cost.
Cost is 1 bp per side on the absolute notional traded.
New shares earn open-to-close.
A session missing either leg earns 0 and is skipped in the ratio.
On 2026-10-01, flatten any shares still held at the close and discard that close's signal.
```

- **Why two standard deviations and an exit at the mean:** the pairs entry and the revert-to-the-mean exit in Gatev, Goetzmann, and Rouwenhorst (2006), applied to the raw close ratio. Their normalized distance, 12-month formation, and 6-month hold are a different rule.
- **Why 60 sessions and divisor 60:** fixed in the study request before any ratio was computed. The commodity-momentum report was read first. The window and the band were left as specified.
- **Why ±0.5:** gross long 0.5 and gross short 0.5 at the sizing instant, so the pair is dollar-neutral at the fill. Shares then stay constant until the next fill.
- **Why 1 bp:** the skill default for SPY-class liquidity. GLD and SLV are in that class. PPLT uses the same prior so the cross-market is the same rule. The store has no quotes. The rate was left at 1 bp after P&L.
- **Why the split:** 2024-07-01 is the repo's existing OOS start. `commodity-etf-momentum` already reported that window. It is not unseen (§4).

### How it trades

| | |
|---|---|
| Sessions with a fill | 110 **(post hoc)** |
| Trades per year | 3.57 **(post hoc)**. 55 trades over 3,883 / 252 evaluation years |
| Time in market | 0.591 of evaluation sessions after the open's fills. IS 0.584. OOS 0.634 |
| Holding time | median 32 sessions, mean 41.7. Every trade exits `flat`, so the count is exit index minus entry index |
| Long ratio / short ratio | 26 `long_ratio`, net +0.074, profit factor 1.159, win rate 0.769, average 42.0 bp. 29 `short_ratio`, net −0.056, profit factor 0.873, win rate 0.552, average −11.2 bp |
| Win rate | 0.655 of 55 trades. **(post hoc)** 36 winners, average net P&L +0.0257. 19 losers, average net P&L −0.0478. Zero trades with net P&L of 0 |

`bad_equity_flattens` is 0. No fill was refused because mark-to-open equity was not positive.

## 3. Hypothesis and predictions

The locked claim is that the split-adjusted GLD/SLV close ratio mean-reverts, so a dollar-neutral pair entered beyond two standard deviations of its trailing 60-session mean, and exited at that mean, has a positive return after costs. Gold and silver share a monetary-metal factor. Escribano and Granger (1998) found a long-run relationship that is not stable across a whole sample, and a strong simultaneous link between the two returns. Lucey and Tully (2006) found periods where the link is weak and a relationship that still holds over a long sample. The residual is temporary hedging and relative-value flow. The other side extrapolates a widening gold/silver gap. Gatev, Goetzmann, and Rouwenhorst (2006) describe pairs profits as temporary mispricing of close substitutes that share a common factor.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Full-sample gross P&L is positive on `short_ratio` and positive on `long_ratio` | `short_ratio` gross −0.0497. `long_ratio` gross +0.0795 | Not consistent |
| Full-sample gross P&L stays positive with the gold leg removed, and stays positive with the silver leg removed | Gold gross −0.278. Silver gross +0.308. Sums of the legs the primary book actually held | Not consistent |

The full-sample net return is +1.7%. The mechanism predictions both fail. The dollar result is the silver leg, and it is one side of the ratio. That does not confirm a two-sided mean-reverting residual.

## 4. Method

- **Data.** `agent-data/mdq.py`, split-adjusted daily bars. `counts.json`, written before the lock, records GLD and SLV at 3,959 bars, 2011-01-04 through 2026-10-01, and PPLT at 3,960 bars with the extra bar on 2026-10-02 left unused. `nyse_sessions` over that span has 3,962 dates. Dropping 2012-10-29, 2012-10-30, and 2018-12-05 leaves 3,959 book sessions, the same set as GLD, SLV, and PPLT through 2026-10-01. Coverage rows: GLD and SLV 4,108 (4,103 complete, 4 missing, 1 partial); PPLT 4,109. `needs_attention` on each name is those three closures, note `no bars: re-run ingest for this session`. The other missing row is 2025-01-09, already outside `nyse_sessions`. The partial row is 2021-12-31, a real daily bar, and it stays in the book. No dividend row is stored. GLD and SLV have no corporate actions. PPLT has one split, ratio 10.0, ex-date 2026-05-18. The continuity check in `counts.json`: raw close on 2026-05-18 over the 2026-05-15 close is 0.10050279329608938; the adjusted ratio is 1.0050279329608938; the prior day, raw and adjusted, is 0.9594768439108061. The adjusted series does not jump by about 10×. `backtest.py` re-checked raw < 0.2 and adjusted between 0.5 and 1.5 before writing results. A session missing either leg earns 0 and is not a ratio point. There was no hole between GLD and SLV on the book calendar.
- **Pre-registration.** `RULES.md` fixed the pair, the 60-session population z, E = 2, the exit at 0, the weights, the 1 bp cost, the samples, both predictions, the five-cell grid, the placebos, and the six acceptance lines. The hash was written by `research/gold-silver-ratio/research/_lock.py` at 2026-10-02T21:15:11+00:00, git HEAD `577dd38c7b98928f8fe9a696aff046425df9dafa`. The file was not committed before the run. The user forbade a commit. Prior exposure, recorded in `RULES.md` before any ratio: `commodity-etf-momentum` is Inconclusive on the same OOS dates. That book returned +41.5% after costs (Sharpe 0.574, profit factor 2.26, max drawdown −37.0%, 21 round trips, 566 sessions) and missed its own floor of 24. Its uncosted equal-weight six-ETF book returned +47.0% OOS (Sharpe 0.967, max drawdown −15.4%). Full sample, that strategy returned +95.2% (Sharpe 0.310). GLD own-trade net was +0.744 and SLV +0.629. Leave-one-out gross stayed positive without GLD (+0.290) and without SLV (+0.348). OOS-entry trades: GLD +0.472 on 1 trade, SLV +0.488 on 2. SLV was long from 2025-07-01 into that study's flatten (net +0.431). No GLD position was open at that flatten. Equal-weight years in that report: 2022 +11.8%, 2023 −11.6%, 2024 +15.3%, 2025 +23.8%, 2026 through 2026-10-01 +14.8%. Strategy years: 2022 +26.0%, 2023 −20.7%, 2024 −2.5%, 2025 +43.7%, 2026 +3.4%. That report already stated, citing `micro-futures-trend`, that gold rallied through 2024–2025 and that crude drifted lower over 2023–2025. The futures gold sleeve was negative in the full sample and positive out of sample. Both GLD and SLV were strong enough, on a 12-month rank, to be held long during parts of this OOS window. The ratio path was not known. No earlier study used PPLT or a gold/silver ratio rule. From that report's account of the equity studies, QQQ buy-and-hold was about +55% in a window that contains the April 2025 tariff crash and rebound. Those are equity results.
- **Fills and costs.** A signal at the close fills at the next pair session's open. The same-bar close path is a labelled upper bound in §6 and is not an acceptance input. Cost is 1 bp of notional per side, not a measured spread. No borrow and no cash interest. Charging zero borrow is the limitation written in the rules.
- **Returns.** Daily simple returns on evaluation sessions. A flat day inside the window is 0 and stays in the mean and the standard deviation. Sharpe is mean ÷ sample standard deviation × √252, delta degrees of freedom 1, risk-free rate 0. CAGR uses a 252-session year. Max drawdown compounds equity from 1 on that window's returns, peak seeded at 1. A trade is in-sample or out-of-sample by its entry fill. Daily Sharpe uses the session date. One primary trade enters 2024-04-04 and exits 2024-07-18 (`long_ratio`, net −0.0327). Its sessions on and after 2024-07-01 are inside the OOS daily path. It is not one of the 8 OOS round trips.
- **Verification.** The self-test runs before `MarketData()`. It covers z > 2, the exit at z ≤ 0, z < −2, a flip, a zero-sigma window, a missing bar, the last session, and the analytic window: 59 ones and one 2 has z = √59, and the following 1 in a 60-point window that still holds that single 2 has z = −1/√59. One call of that analytic check failed before any store open, because the window passed to it had 61 points. The call was sliced to 60 points. That failure wrote no `RUNLOG` line. The initial store run then passed the self-test. `verify.py` imports neither `backtest.py` nor another study module. It matched all 55 primary trades and all 61 cross-market trades on side, entry date, exit date, both entry prices, both exit prices, exit reason, gross, and net, to 1e-6 absolute. It matched 3,883 daily `strategy_net` and `strategy_gross` rows to 1e-8. Seed 20261084 drew 40 trades. The pass is the full list.
- **Runs.** Two store runs, both in `RUNLOG.md`. The first is `initial pre-registered run` at 2026-10-02T21:26:50+00:00. The second is `verification, independent rebuild of every trade (match)` at 2026-10-02T21:34:25+00:00. Both record git HEAD `738d3e7ec4340e05458317114dfe247d55a1ef16`, dirty. Rounded headlines match: Sharpe 0.115 / 0.146 / 0.220 and total return 0.017 / 0.125 / −0.095. There was no bug-fix store rerun, so no headline moved between store runs. The rules hash in `RULES.lock`, `results.json`, and both log entries is `abedf66dd1b3dae3990d0ddcaf6fcb6a38638a2cba540311ec5256b0f8b1078a`.
- **Checklist.** The hash matches. `python -m research.kit guard research/gold-silver-ratio/research/verify.py` printed `ok`. `results.json` has no `kit_schema` field. The locked rules specify this study's own document, and the run was not rewritten onto kit schema 1. That checklist item is unmet. `research/README.md` was not updated. The request forbade editing that file. That checklist item is unmet on purpose. Nothing was written to `data/`. Ingest was not run.

## 5. Results

![Growth of $1](figures/equity.svg)

Growth of $1 from the first fill, strategy and both uncosted buy-and-hold books on one linear scale. The dashed line is 2024-07-01. The strategy's full-sample return is +1.7%. GLD and SLV finish far above it, and the strategy's OOS compounded return is −9.5%.

![Drawdown](figures/drawdown.svg)

Drawdown from each series' own peak over the full evaluation window. Strategy max drawdown is −72.1% full sample and −70.3% out of sample. GLD's full-sample max drawdown is −45.6%. SLV's is −76.3%.

| Strategy (1 bp/side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary GLD/SLV** | +1.7% / 0.115 / −72.1% | 0.146 | −9.5% / 0.220 / −70.3% |
| *GLD buy and hold, uncosted* | +159.7% / 0.451 / −45.6% | 0.262 | +78.3% / 1.209 / −27.9% |
| *SLV buy and hold, uncosted* | +17.6% / 0.197 / −76.3% | −0.006 | +107.8% / 0.933 / −53.0% |
| *GLD/PPLT, same rule, 1 bp* | −11.4% / −0.033 / −37.8% | −0.072 | −0.2% / 0.075 / −22.4% |

The cross-market row is line 5's book. It is not a second primary. Its OOS Sharpe is 0.075 and its OOS compounded return is −0.16%.

![Calendar-year return](figures/by_year.svg)

| Year | Sessions | Strategy | Sharpe | Max DD | GLD | SLV |
|---|---:|---:|---:|---:|---:|---:|
| 2011 | 175 | +4.0% | 0.484 | −11.8% | +3.0% | −42.6% |
| 2012 | 250 | +2.1% | 0.390 | −7.1% | +6.6% | +9.0% |
| 2013 | 252 | +0.3% | 0.081 | −6.9% | −28.3% | −36.3% |
| 2014 | 252 | +3.1% | 0.623 | −4.6% | −2.2% | −19.5% |
| 2015 | 252 | +15.9% | 3.084 | −1.3% | −10.7% | −12.4% |
| 2016 | 252 | −3.9% | −0.519 | −9.9% | +8.0% | +14.6% |
| 2017 | 251 | −2.1% | −0.692 | −4.1% | +12.8% | +5.8% |
| 2018 | 251 | −2.1% | −0.445 | −4.2% | −1.9% | −9.2% |
| 2019 | 252 | +2.9% | 0.563 | −4.9% | +17.9% | +14.9% |
| 2020 | 253 | −13.9% | −0.632 | −27.5% | +24.8% | +47.3% |
| 2021 | 252 | −5.1% | −0.545 | −9.7% | −4.1% | −12.5% |
| 2022 | 251 | +4.2% | 0.588 | −6.4% | −0.8% | +2.4% |
| 2023 | 250 | +12.3% | 2.488 | −2.2% | +12.7% | −1.1% |
| 2024 | 252 | +3.2% | 0.390 | −9.3% | +26.8% | +20.9% |
| 2025 | 250 | −29.0% | −1.645 | −35.9% | +63.4% | +147.0% |
| 2026 | 188 | +20.3% | 0.638 | −57.4% | −3.3% | −15.1% |

2011 starts at the first fill, 2011-04-25, so it has 175 evaluation sessions. 2024 straddles 2024-07-01. 2026 runs through 2026-10-01 and has 188 sessions. 2015's Sharpe is 3.084 on a +15.9% year whose max drawdown is −1.3%. 2025's strategy return is −29.0% while SLV's year return is +147.0% and GLD's is +63.4%. 2026's strategy return is +20.3% and its max drawdown is −57.4%. The table prints stored fractions to one decimal and stored Sharpes to three decimals.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trade's timing and multiplies that trade's daily gross price P&L by an independent ±1. Two thousand draws, seed 20261081. The denominator is the actual costed book's previous net equity. Actual full-sample gross Sharpe is 0.117. The null mean is 0.0005, the null standard deviation is 0.068, and the null 95th percentile is 0.112. Seventy draws landed at or above the actual. p = 0.0355, so line 2 passes. The placebo is on the full-sample gross Sharpe. The OOS compounded return is still −9.5%.

The timing placebo is not testable. Seed 20261083 made 200,000 attempts and accepted 0 draws. p is null. The stored reason is that fewer than 500 draws stayed inside the sample without overlapping. Every primary trade exits `flat`, including the last, which exits 2026-10-01. No `end` trade was dropped to force a sample. This check is not an acceptance line. A null p here is neither a pass nor a fail of line 2.

### Bootstrap

Circular 20-session blocks of the full-sample net daily returns, 2,000 draws, seed 20261082. The 2.5 percentile of draw Sharpes is −0.517, the median is 0.124, and the 97.5 percentile is 0.455. The interval contains 0. The OOS t-stat is 0.330.

### Parameter plateau

![Parameter grid](figures/grid.svg)

| Entry \|z\| | First fill | IS Sharpe | IS return | OOS Sharpe | OOS return |
|---|---|---:|---:|---:|---:|
| 1.0 | 2011-03-31 | 0.163 | +16.4% | 0.276 | −18.2% |
| 1.5 | 2011-03-31 | 0.138 | +12.2% | 0.259 | −20.2% |
| **2.0** | **2011-04-25** | **0.146** | **+12.5%** | **0.220** | **−9.5%** |
| 2.5 | 2011-08-10 | −0.109 | −12.0% | 0.171 | −4.1% |
| 3.0 | 2011-09-26 | 0.074 | +3.2% | 0.082 | −11.6% |

Four of five IS cells have Sharpe above 0. The cell at 2.5 is the one below 0. The E = 2 cell's IS Sharpe equals the primary IS Sharpe. The cell at 1.0 has the highest IS Sharpe in the grid, 0.163, and an OOS return of −18.2%. Nothing was selected from the grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.117 | 0.116 | **0.115** | 0.113 | 0.110 |
| OOS Sharpe | 0.221 | 0.220 | **0.220** | 0.219 | 0.218 |
| Full-sample return | +2.9% | +2.3% | **+1.7%** | +0.6% | −0.5% |

Line 4 uses the 2× full-sample return, +0.614%, which is above 0. The 3× full-sample return is −0.500%. OOS Sharpe at zero cost is 0.221. Moving the cost from 0 to 3× changes OOS Sharpe from 0.221 to 0.218.

Delay of two pair sessions, instead of one: full-sample Sharpe 0.096, OOS Sharpe 0.203, full-sample return +4.0%. The same-bar close path, labelled upper bound and not a verdict input: full-sample Sharpe 0.110, OOS Sharpe 0.248, full-sample return −2.4%.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / profit factor | OOS return | Trades |
|---|---:|---:|---|---:|---:|
| GLD/PPLT | −0.072 | 0.075 | −11.4% / 0.853 | −0.16% | 61 full, 52 IS, 9 OOS |

`cross_trades.csv` opens on 2011-05-24. The last cross-market trade enters 2026-09-09 and exits 2026-10-01 with reason `end`. Line 5 asks only whether OOS Sharpe is above 0. It is 0.075. Full-sample return is −11.4% and IS Sharpe is −0.072. The cross-market does not replace GLD/SLV.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

| GLD quintile | Sessions | Mean strategy net | Mean GLD |
|---|---:|---:|---:|
| 1, worst | 777 | +0.002869 | −0.013906 |
| 2 | 777 | +0.000220 | −0.003813 |
| 3 | 776 | −0.000300 | +0.000436 |
| 4 | 777 | −0.000377 | +0.004556 |
| 5, best | 776 | −0.001679 | +0.014266 |

The strategy's mean daily return is positive in the worst GLD quintile and negative in the best. Quintiles were not used to filter the rule.

OOS-entry trades, all reason `flat`, from `trades.csv`. Gross and net are account P&L on the unit book.

| Side | Entry | Exit | Net | Gold gross | Silver gross |
|---|---|---|---:|---:|---:|
| short_ratio | 2024-08-06 | 2024-09-18 | +0.0360 | −0.0426 | +0.0789 |
| long_ratio | 2024-10-21 | 2024-11-07 | +0.0309 | −0.0103 | +0.0414 |
| short_ratio | 2024-12-20 | 2025-03-12 | +0.0104 | −0.0708 | +0.0814 |
| short_ratio | 2025-04-02 | 2025-06-04 | −0.0334 | −0.0444 | +0.0112 |
| long_ratio | 2025-07-14 | 2026-02-06 | −0.2626 | +0.2583 | −0.5206 |
| long_ratio | 2026-05-12 | 2026-06-08 | +0.0552 | −0.0392 | +0.0946 |
| short_ratio | 2026-06-25 | 2026-08-21 | +0.0314 | −0.0702 | +0.1018 |
| long_ratio | 2026-09-23 | 2026-10-01 | +0.0183 | −0.0174 | +0.0358 |

The 2025-07-14 trade is a bet that the GLD/SLV ratio rises: long gold, short silver, shares fixed at the entry open. Gold's stored gross on that trade is positive. Silver's stored gross is −0.521. The trade exits at the open on 2026-02-06 because z has crossed back through 0, which is the locked exit, not a stop. OOS profit factor is 0.615 and OOS average net trade is −91.2 bp, with an OOS win rate of 0.75. Side, exit reason, and quintile were not used to drop a trade.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS Sharpe and OOS profit factor | Sharpe ≥ 0.5 and profit factor ≥ 1.10 | Sharpe 0.21994903937127092, profit factor 0.6153560518784879 | ❌ |
| 2. Direction placebo | p ≤ 0.05 on the full sample | p 0.03548225887056472 | ✅ |
| 3. IS plateau | IS Sharpe > 0 and at least 3 of 5 IS cells with Sharpe > 0 | IS Sharpe 0.1463104386434701, 4 cells | ✅ |
| 4. Cost | Full-sample total return > 0 at 2× | 0.0061398515899064865 | ✅ |
| 5. Cross-market | OOS Sharpe > 0 on GLD/PPLT | 0.0749282820918089 | ✅ |
| 6. Sample | At least 15 OOS round trips with exit `flat` or `flip` | 8 | ❌ |

Line 2 passing does not mean the OOS book made money. Line 5 passing does not mean the GLD/PPLT book made money: its OOS return is −0.0016073750245865215. Line 6 sets the status. The floor stays at 15.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | OOS compounded return −9.5% after an IS return of +12.5%. OOS Sharpe 0.220 against a bar of 0.5. **(post hoc)** The last 252 evaluation sessions, 2025-10-01 through 2026-10-01, returned −11.1% (Sharpe 0.313) while GLD returned +7.8% and SLV +30.3% | No deployment. A later window is a new study |
| Concentration in a few days | **(post hoc)** Setting the 10 best `strategy_net` days to 0 changes the full-sample return from +1.7% to −78.7% (Sharpe −0.726). The largest day is 2026-01-30 at +1.091, inside the open 2025-07-14 trade. Setting the 10 worst days to 0 changes the full-sample return to +208.0% (Sharpe 0.337) | Shares stay constant between fills, which is the locked rule. A daily resize is a different study |
| Generalization | GLD/PPLT full-sample return −11.4%, IS Sharpe −0.072, OOS Sharpe 0.075. Prediction 1 and prediction 2 both fail. Full-sample gold gross is −0.278 and silver gross is +0.308 | PPLT was not promoted. One metal was not dropped |
| Execution | OOS Sharpe moves from 0.221 at zero cost to 0.220 at 1 bp and 0.218 at 3×. Full-sample return is negative at 3× (−0.5%). Delay of one extra pair session leaves OOS Sharpe at 0.203. No borrow was charged. The same-bar close upper bound has full-sample return −2.4% | The 1 bp rate stays. Live spreads were not measured |
| Short sample | 8 OOS round trips against a floor of 15. Bootstrap 95% interval of full-sample Sharpe is −0.517 to 0.455. Timing placebo accepted 0 of 200,000 draws | Status stays Inconclusive. The floor was not lowered |

## 10. No deployment

There is no paper-trading proposal. The primary missed the sample floor written to decide that question, and the OOS Sharpe and profit factor missed their bars as well. A different exit, a resized book, one metal, one side, or a later sample is a new study with its own `RULES.md`.

## 11. Post hoc (not part of the verdict)

- **Rolling 252-session Sharpe.** 3,632 windows, first ending 2012-04-23 and last ending 2026-10-01. The fraction with strategy Sharpe above 0 is 0.555. The range is −2.686 to 3.386. The figure in §9 draws that series. It is not an acceptance input.
- **Concentration.** Full-sample return with the 10 best days set to 0 is −0.7868245953590648. With the 10 worst days set to 0 it is 2.079862903814498. All ten worst days in `posthoc.json` fall from 2025-12-26 through 2026-02-04, inside the 2025-07-14 to 2026-02-06 `long_ratio`. The best day, 2026-01-30, is in that same span, as are 2026-02-05, 2026-01-21, 2026-01-07, 2025-12-29, and 2025-12-31. The other four of the ten best days are 2020-08-11, 2020-09-21, 2020-09-23, and 2021-02-02. The locked book does not resize shares after the fill, so a large silver move is a large percentage of whatever equity the open trade has left. The trade's stored net P&L is −0.263. None of these days was removed from the test.
- **Correlation.** Daily strategy net against GLD buy-and-hold is −0.201. Against SLV buy-and-hold it is −0.329. GLD against SLV is 0.791. The pair's daily path does not move with either metal one for one.
- **Last 252 sessions.** 2025-10-01 through 2026-10-01. Strategy return −0.11120793614612823, Sharpe 0.31251494081889. GLD return 0.07827721045376812, Sharpe 0.4021026938255864. SLV return 0.3029420785655317, Sharpe 0.7508622192840646.
- **Trade-list description.** 3.57 trades per evaluation year, 110 sessions that contain an entry or an exit, 36 winners, 19 losers, 0 zeros. These counts do not change §8.

### Ideas for a new study

The 2026-01-30 session return of +1.091 sits inside a constant-share silver short that the exit did not close until 2026-02-06. A rule that rescaled the legs back to ±0.5 each day, or that capped one leg's share of equity, is a different strategy. It would need its own `RULES.md` and a sample this study has not used. Keeping only `long_ratio`, or only the silver leg, is the same kind of new study. This file does not promote either one.

## 12. Reproduce

From the repo root:

```bash
python research/gold-silver-ratio/research/backtest.py
```

```bash
python research/gold-silver-ratio/research/verify.py
```

```bash
python research/gold-silver-ratio/research/posthoc.py
```

```bash
python research/gold-silver-ratio/research/charts.py
```

`backtest.py` runs the self-test, refuses a hash mismatch, and on a store run writes `results.json`, `daily.csv`, `trades.csv`, `cross_trades.csv`, `placebo_direction.npy`, and one `RUNLOG.md` entry. The timing placebo accepted no draws, so it writes no timing array. `verify.py` is a second store run and appends its own log entry. `posthoc.py` reads the CSVs and writes `posthoc.json` and `rolling_sharpe.csv`. `charts.py` reads those files and writes the eight SVGs under `report/figures/`. Seeds are direction 20261081, bootstrap 20261082, timing 20261083, and verify 20261084. A rerun on the same store follows the same code and the same seeds.

### References

- Gatev, E., Goetzmann, W. N., and Rouwenhorst, K. G. (2006). Pairs trading: Performance of a relative-value arbitrage rule. *Review of Financial Studies*, 19(3), 797–827.
- Escribano, A., and Granger, C. W. J. (1998). Investigating the relationship between gold and silver prices. *Journal of Forecasting*, 17(2), 81–107.
- Lucey, B. M., and Tully, E. (2006). The evolving relationship between gold and silver 1978–2002: evidence from a dynamic cointegration analysis: a note. *Applied Financial Economics Letters*, 2(1), 47–53.
