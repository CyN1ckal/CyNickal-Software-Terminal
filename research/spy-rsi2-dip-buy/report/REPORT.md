# SPY RSI(2) dip-buy: a negatively skewed, high-win-rate index strategy

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Paper-trading candidate.** Passed all 7 pre-registered tests. One of them, the timing placebo, passed at p = 0.046 against a 0.05 line and fails under two of five other seeds. Not for live capital. |
| Instruments | SPY, long only, bought and sold at the regular-hours close, held for days. QQQ is the cross-market test. IGV is reported under the same rules |
| Data | Regular-hours 1-minute bars, 2021-09-27 → 2026-09-25, read via `agent-data/mdq.py`. Evaluated 2021-10-25 → 2026-09-25 (1,235 sessions). 2021-12-31 has no bars and is a zero day. **The stored daily bars were not used:** from late 2024 they carry extended-hours prices (§4) |
| Rules | [`research/spy-rsi2-dip-buy/research/RULES.md`](../research/RULES.md), locked 2026-09-27 00:33 UTC, sha256 `e343018e290a` |
| Code | [`research/spy-rsi2-dip-buy/research/`](../research/) · 1 backtest run, 1 verification replay, 3 post hoc runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The request was a stock-index strategy with a negatively skewed payoff and a high win rate. The study tested Connors' 2-period RSI dip-buy on SPY. The rule buys at the close when RSI(2) is below 10 and sells at the first close above the 5-day average. It has no stop. Out of sample, from 1 July 2024 through 25 September 2026, it returned **+33.8%** after 1 bp per side (Sharpe **1.29**, profit factor 5.09, 32 trades, 93.8% winners, max drawdown **−11.9%**). SPY buy-and-hold returned +41.7% over the same window (Sharpe 1.03, drawdown −19.0%). Over the full window the rule returned +53.1% (Sharpe 0.97) against SPY's +70.2% (Sharpe 0.72). It was in the market on 18.0% of sessions.

The payoff has the requested shape. Over the full sample 81.8% of trades won. The average winner made +126 bp and the average loser lost −205 bp. The skewness of net trade returns was **−2.04**.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-25 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,235 | 673 | 562 |
| Total return | +53.1% | +14.5% | **+33.8%** |
| CAGR | 9.1% | 5.2% | 13.9% |
| Annual volatility | 9.5% | 8.5% | 10.5% |
| Sharpe | 0.97 | 0.64 | **1.29** |
| Max drawdown | −11.9% | −6.8% | **−11.9%** |
| Trades / profit factor | 66 / 2.78 | 34 / 1.81 | 32 / 5.09 |
| Win rate / trade skew | 81.8% / −2.04 | 70.6% / −1.18 | 93.8% / −3.37 |
| Avg net trade | +66.0 bp | +41.2 bp | +92.4 bp |
| *SPY buy and hold Sharpe (max DD)* | *0.72 (−25.4%)* | *0.47 (−25.4%)* | *1.03 (−19.0%)* |

It passed all 7 acceptance tests written before the first run (§8).

**What has to sit next to that pass.**

1. **The test that separates dip timing from being long a rising market barely passed, and it is seed-dependent.** Random long holds with the same durations beat the actual gross Sharpe in 4.6% of 2,000 draws (p = 0.046, line 0.05). **(post hoc)** Under seeds 1 to 5 the same test gives p = 0.051, 0.052, 0.046, 0.045, and 0.046. A second placement method in `verify.py` gives 0.0495. The direction placebo (p = 0.001) is not evidence on this question, because it rewards any long book in an up market. Buying dips was better timed than random long exposure by an amount at the edge of detectability.
2. **One session carries much of the out-of-sample result.** The rule bought SPY at the close on 3 April 2025 and held through −5.8% on 4 April and −1.6% on 8 April. It exited with a gain after SPY rose **+10.5% on 9 April**, the day the tariff pause was announced. **(post hoc)** Without that session the out-of-sample return is +21.1% and the Sharpe 1.14. Zeroing the 20 best sessions of the full sample leaves −8.8% (Sharpe −0.24).
3. **The sample is small.** There are 66 trades, 32 of them out of sample. The out-of-sample t-statistic is 1.93. The full-sample bootstrap interval for the Sharpe is 0.28 to 1.73. With 32 trades, a 93.8% win rate is consistent with a much lower long-run rate.
4. **The mechanism is not confirmed.** Two of five pre-registered predictions held (§3). Deeper dips (RSI < 5) paid less than shallow ones, not more. The first day after entry averaged +5.7 bp and the later days +27.1 bp. The reversal was not fast.
5. **It trails buy-and-hold in total return, and the fill matters.** It earned 53.1% against SPY's 70.2% while holding SPY 18% of the time. Deciding on the full close and filling at the next open cut the full-sample Sharpe from 0.97 to 0.63 (§6). The rule has been public since 2008, and this out-of-sample window had already been read in earlier studies.

**Recommendation.** Paper-trade the locked SPY rule forward of 25 September 2026, using market-on-close orders and the halt rules in §10. Do not add a stop, change the threshold, add the 200-day filter, or switch to QQQ on the strength of this report. Those are different rules, and the grid and S1 results in §6 were not used to choose among them.

## 2. The strategy

### Rules

```
Closes C are regular-hours closes built from 1-minute bars.
P = the close of the 15:49 minute (12:49 on a 13:00 close), known at 15:50.

At 15:50 each session:
  RSI2  = Wilder RSI(2) on prior closes, updated with P as today's close
  SMA5  = (last 4 closes + P) / 5

  in a position and P > SMA5          -> sell at today's close (market on close)
  flat, not sold today, and RSI2 < 10 -> buy at today's close (market on close)

Long only, 100% of equity, one position, no stop, no time stop, no target.
1 bp per side. Flat days earn 0.
```

- **Why these parameters.** RSI length 2, the 0–10 buying zone, and the exit above the 5-day average are Connors & Alvarez's (2008) published rule. Threshold 10 was chosen before any run, for sample size. Connors reports that below 5 did better, and 5 is in the grid.
- **Why there is no 200-day filter.** The filter would use 200 of about 1,250 sessions as warm-up and remove most of the 2022 bear market. That is where a dip-buyer's loss tail lives. The published filtered rule is secondary S1 (§6), and it cannot replace the primary.
- **Why the decision is taken at 15:50 and filled at the close.** Connors' convention decides on the close and fills at the close, which is look-ahead. A market-on-close order must be entered by 15:50, so the decision uses the 15:49 minute. That published convention is reported as an upper bound (§6).

### How it trades

| | |
|---|---|
| Trades | 66 (34 in sample, 32 out of sample), 13.4 a year |
| Time in market | 18.0% of sessions |
| Holding time | median 3 sessions, mean 3.4 |
| Side | long only |
| Win rate | 81.8%; average winner +126 bp, average loser −205 bp |
| Best / worst trade | +2.78% / −6.21% (21 Feb → 14 Mar 2025, 15 sessions) |
| Exits | all 66 by the SMA rule; none left open at the sample end |

![Net return per trade, and by holding time](figures/trade_distribution.svg)

Most trades are wins of +50 to +200 bp, and there is a long left tail down to −621 bp. This is the requested shape. **(post hoc)** All 45 trades that exited within three sessions won. All 8 trades held six sessions or longer lost.

## 3. Hypothesis and predictions

Short-term index declines are partly driven by liquidity demand. Examples are margin calls, de-leveraging by volatility-targeting funds, and leveraged-ETF rebalancing at the close. Whoever buys from those sellers earns a reversal premium for holding inventory risk (Campbell, Grossman & Wang 1993; Nagel 2012; Cheng & Madhavan 2009). The position behaves like a short put. It wins small and often when liquidity returns. It loses large when the decline is news and keeps going.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Average loser ≥ 2× average winner | −205 bp vs +126 bp: ratio 1.62 | Not consistent |
| 2. Entries with RSI < 5 earn more gross than RSI 5–10 | +41 bp (14 trades) vs +75 bp (52 trades) | Not consistent |
| 3. Entries in higher trailing volatility earn more gross | +85 bp above the median vol vs +51 bp below (33 trades each) | Consistent |
| 4. Worst 10% of trades carry ≥ 50% of the losses | The worst 7 trades carry 90.7% of summed losing returns | Consistent |
| 5. The first held day is positive and beats later days | +5.7 bp first day (66) vs +27.1 bp later days (156) | Not consistent |

The acceptance table passed, but the mechanism is only partly confirmed. The tail is concentrated, and the premium was larger when volatility was high, which fits Nagel's liquidity-provision account. The liquidity story also predicts that deeper dips pay more and that the rebound comes fast. Neither held. The losers are not twice the winners, because the exit takes the first close above the 5-day average, and that can come while the trade is still under water. Positive P&L alone is not evidence that the mechanism is the one above.

## 4. Method

- **Data.** SPY, QQQ, and IGV regular-hours 1-minute bars, 1,254 sessions each, 2021-09-27 → 2026-09-25. The 15:49 decision minute exists on every session. 2021-12-31 has no bars. It is a zero-return day, and a position held through it books the move on 2022-01-03. IGV's 5-for-1 split (2024-03-07) is adjusted by mdq. Dividends are not stored, so both the strategy and buy-and-hold omit them. The strategy holds SPY on 18.0% of sessions, so it misses the dividends only on those sessions.
- **A defect in the store's daily bars.** The pre-lock check (`counts.py`) found that from late 2024 the stored 1d bars are not regular-hours bars. Their closes differ from the last regular-hours minute by more than 20 bp on 111 SPY sessions (first on 2024-11-20) and 168 QQQ sessions. On 2025-04-02 SPY's stored close is 544.83. The regular session ended at 564.52, and the stored figure is the after-hours price following that evening's tariff announcement. Opens show the same pattern. This study builds every open and close from minutes and never reads the 1d bars. Earlier studies that read 1d opens or closes after 2024 are exposed to this defect (see the note to the user).
- **Pre-registration.** `RULES.md` fixed the rule, both placebos, the grid, the 25-trade minimum, and the payoff-shape line before any return was computed. Prior exposure: the out-of-sample window had already been used by nine earlier studies in this store. I knew SPY and QQQ rose strongly in it, and that it contains the April 2025 crash and rebound. `RULES.md` was hash-locked but **not committed** before the run.
- **Fills and costs.** Fills are at the regular-hours close, meaning the last minute's close. The closing-auction print is not in the store. The cost is 1 bp per side, well above SPY's one-cent spread (about 0.2 bp).
- **Returns.** Daily simple returns on the NYSE calendar. Flat days are 0. Sharpe is the mean over the sample SD × √252.
- **Verification.** The self-test covers every rule path on synthetic sessions and passed before the store was opened: the 15:49, 12:49, and fallback minutes, RSI with no losses, entry, hold, SMA exit, no re-entry on an exit day, a no-bar day while holding, the end-of-sample close, the S1 filter, the D1 open fill, and the cost arithmetic. `verify.py` is a naive re-implementation with RSI recomputed from scratch every day and no shared code. It matched **all 66 SPY and 61 QQQ trades** on entry and exit date, price, and reason. It matched the SPY daily returns to 5e−11.
- **Runs.** One backtest run, then one verification replay. No bug fix, and no rerun of the backtest. Three post hoc runs re-simulated the locked primary for the seed check and added descriptive figures. The first did not log itself and was logged retroactively. None changed a file the verdict uses.

## 5. Results

![Growth of $1](figures/equity.svg)

The strategy rises in steps and ends at 1.53. Buy-and-hold ends at 1.70. The strategy was nearly flat through 2022, when SPY fell, and lagged SPY through most of 2023–2025. The dashed line marks 1 July 2024.

![Drawdown](figures/drawdown.svg)

The strategy's worst drawdown, −11.9%, came in March–April 2025. It stacks the −6.2% trade from February–March on the early-April hold. SPY's worst was −25.4%, in 2022.

| Strategy (1 bp) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary, SPY** | +53.1% / 0.97 / −11.9% | 0.64 | +33.8% / 1.29 / −11.9% |
| S1, 200-day filter (secondary) | +26.7% / 0.99 / −8.1% | 0.47 | +21.0% / 1.44 / −8.1% |
| *SPY buy and hold* | *+70.2% / 0.72 / −25.4%* | *0.47* | *+41.7% / 1.03 / −19.0%* |

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | SPY | Trades | Win rate |
|---|---:|---:|---:|---:|---:|---:|
| 2021 (from 25 Oct) | +2.2% | 1.46 | −3.0% | +5.1% | 2 | 50% |
| 2022 | +4.0% | 0.40 | −6.8% | −19.7% | 16 | 75% |
| 2023 | +5.2% | 0.81 | −5.7% | +24.3% | 11 | 64% |
| 2024 | +13.3% | 2.77 | −1.9% | +23.3% | 14 | 93% |
| 2025 | +5.6% | 0.45 | −11.9% | +16.3% | 12 | 83% |
| 2026 (to 25 Sep) | +14.4% | 3.02 | −1.9% | +13.1% | 11 | 100% |

Every calendar year made money. It beat SPY in 2022 and 2026 and trailed it in the other four.

## 6. Is it real?

### Placebo

![Placebos](figures/placebo.svg)

**Direction:** flipping each trade's sign at random gave a null mean gross Sharpe of 0.01 and a 95th percentile of 0.51. The actual 0.99 gives p = 0.001. This test only shows that a long book beat a coin-flip book in a rising market.

**Timing:** each draw put long holds of the same lengths at random non-overlapping dates. The null mean was 0.32 and the 95th percentile 0.97. The actual 0.99 gives **p = 0.046**. That is the test of whether the dip timing adds anything to plain long exposure, and it clears 0.05 by a margin smaller than the seed-to-seed spread **(post hoc)**: seeds 1–5 give 0.045 to 0.052.

### Bootstrap

A circular 20-session block bootstrap of daily returns (2,000 draws, seed 20260928) gives a 95% interval of **0.28 to 1.73** for the full-sample Sharpe. 0.25% of draws were ≤ 0. The t-statistic is 2.14 for the full sample, 1.04 in sample, and 1.93 out of sample.

### Parameter plateau

![Parameter grid](figures/grid.svg)

All 15 in-sample cells (entry RSI 5 to 25, exit SMA 3/5/10) have a positive Sharpe, from 0.39 to 0.87. The primary's is 0.64. The in-sample best cell, RSI < 20 with SMA 3 (0.87), had an out-of-sample Sharpe of 1.11, below the primary's 1.29. Every out-of-sample cell is between 0.81 and 1.42. Nothing was selected from the grid. The grid shows the result does not depend on the exact threshold. It says nothing about drift, which every cell shares.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.99 | 0.98 | **0.97** | 0.94 | 0.91 |
| OOS Sharpe | 1.32 | 1.31 | **1.29** | 1.27 | 1.24 |
| Full-sample return | +55.1% | +54.1% | **+53.1%** | +51.1% | +49.1% |

At 13.4 round trips a year, the rule is insensitive to cost. It is sensitive to the fill:

| Fill | Full Sharpe | IS Sharpe | OOS Sharpe | Full return | Win rate |
|---|---:|---:|---:|---:|---:|
| **Primary:** decide at 15:50, fill at the close | **0.97** | 0.64 | **1.29** | +53.1% | 81.8% |
| D1: decide on the close, fill at the next open | 0.63 | 0.26 | 1.02 | +31.6% | 67.6% |
| Upper bound: decide on the close, fill at the close (look-ahead) | 1.09 | 0.56 | 1.63 | +62.3% | 80.9% |

Waiting for the next open gives up part of the rebound, which starts in the overnight gap after the dip close. The rule works only as a market-on-close trade.

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF | Win rate / trade skew |
|---|---:|---:|---|---|
| SPY (primary) | 0.64 | 1.29 | +53.1% / 2.78 | 81.8% / −2.04 |
| **QQQ** (acceptance) | 0.83 | **1.28** | +75.5% / 4.03 | 75.4% / −0.99 |
| IGV, 2 bp (reported) | 0.40 | 1.15 | +63.0% / 1.98 | 70.1% / −0.28 |

QQQ's daily returns correlate 0.80 with SPY's. It is close to the same trade, not an independent confirmation. IGV's trade skew is only −0.28, and its drawdown was −21.9%.

### Secondary S1 (Connors' published rule with the 200-day filter)

S1 passed its own lines: OOS Sharpe 1.44, OOS profit factor 3.69 on 27 trades, direction p = 0.006, timing p = 0.033, +25.6% at 2 bp, QQQ OOS Sharpe 1.22, win rate 82.9%, trade skew −2.73. Its in-sample record is 14 trades with a Sharpe of 0.47. **(post hoc)** It earned 50% of the primary's full-sample return at 53% of its volatility. S1 is reported, not promoted. It does not change the primary's verdict, and the primary is not replaced by it.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

| SPY daily move quintile | SPY avg | Strategy avg |
|---|---:|---:|
| Q1 (worst) | −141.7 bp | −28.5 bp |
| Q2 | −35.8 bp | −6.2 bp |
| Q3 | +6.9 bp | +0.7 bp |
| Q4 | +50.9 bp | +8.0 bp |
| Q5 (best) | +144.2 bp | +44.2 bp |

**(post hoc)** On the worst quintile of days the strategy took 20% of SPY's average move. On the best quintile it took 31%. That asymmetry is the whole result. **(post hoc)** On SPY's 20 worst sessions the strategy averaged −92 bp against SPY's −347 bp. It was in the market on 65% of them. Daily returns correlate 0.55 with SPY.

By holding time (figure in §2): trades that exited within 1 to 3 sessions averaged +128 to +144 bp net. Trades held 6 or more sessions all lost. **By exit reason:** all 66 exits were the SMA rule. **By entry RSI:** 14 trades below 5 averaged +41 bp gross, and 52 trades between 5 and 10 averaged +75 bp. None of these breakdowns was used to filter the rule.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1. OOS Sharpe | ≥ 0.50 | 1.29 | ✅ |
| 1. OOS profit factor | ≥ 1.10 | 5.09 | ✅ |
| 2. Direction placebo p (full, gross) | ≤ 0.05 | 0.001 | ✅ |
| 2. Timing placebo p (full, gross) | ≤ 0.05 | 0.046 | ✅ |
| 3. IS Sharpe | > 0 | 0.64 | ✅ |
| 3. IS grid cells with Sharpe > 0 | ≥ 9 of 15 | 15 of 15 | ✅ |
| 4. Full-sample return at 2 bp | > 0 | +51.1% | ✅ |
| 5. QQQ OOS Sharpe | > 0 | 1.28 | ✅ |
| 6. OOS trades (minimum sample) | ≥ 25 | 32 | ✅ |
| 7. Full-sample win rate | ≥ 60% | 81.8% | ✅ |
| 7. Full-sample net trade skewness | < 0 | −2.04 | ✅ |

The status follows mechanically from these lines. Line 2's timing half passed by less than the variation between random seeds. Line 6 was lowered from the protocol's 100 to 25 before the lock, because the rule cannot reach 100 trades in 2.2 years. At 32 trades, the out-of-sample profit factor and win rate are estimated loosely.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

**(post hoc)** The trailing 252-session Sharpe was positive in 96% of windows. It ranged from −0.28 to 3.20 and ended at 2.69. Since 1 May 2025 the strategy returned +19.9% (Sharpe 2.23) while SPY returned +39.1% (Sharpe 1.95).

| Risk | Evidence | Mitigation |
|---|---|---|
| Timing edge not distinguishable from drift | Timing placebo p = 0.046; 0.045–0.052 across seeds **(post hoc)** | Paper-trade; judge it against random long exposure, not against zero |
| Concentration in a few days | **(post hoc)** Without 9 April 2025, OOS +21.1% / Sharpe 1.14. Without the best 20 sessions, full sample −8.8% | Expect long stretches of small gains, with the result decided on a few rebound days |
| Unbounded tail | No stop. Worst trade −6.21% over 15 sessions. **(post hoc)** The deepest open-trade mark was −8.1% (21 Feb → 14 Mar 2025). The April 2025 hold reached −7.5% before the pause announcement | The halt rules in §10. A crash without a policy reversal is not in this sample |
| Execution | D1 (next open) cuts full Sharpe to 0.63. The fill is the last minute, not the closing auction | Market-on-close orders only; log the auction price against the 15:59 minute |
| Short sample, public rule | 66 trades; bootstrap 0.28–1.73; rule public since 2008 (McLean & Pontiff 2016) | A forward test is the only unseen data |
| Mechanism | 3 of 5 predictions not consistent | Do not treat the pass as proof of the liquidity-provision story |
| Data | Stored 1d bars are extended-hours from late 2024; no dividends | Build every price from regular-hours minutes |

## 10. Deployment proposal (paper trading)

- **Account and size.** One paper account, 100% of equity long SPY while in a trade, no leverage. Size nothing else from this study.
- **Signal time.** At 15:50 ET (12:50 on a 13:00 close), take the 15:49 minute's last price as P. Compute RSI(2) with Wilder smoothing on regular-hours closes, and SMA5 from the last four closes plus P.
- **Orders.** Buy or sell market-on-close by the NYSE 15:50 cutoff (12:50 on early closes). No stop orders. No intraday exits.
- **Logging.** Record every fill in a terminal ledger. Record the closing-auction price next to the 15:59 minute close that the backtest assumed.
- **Review.** After 12 months or 25 closed trades, whichever comes later.
- **Halt rules** (stop the paper test, do not retune):
  - one trade loses more than 12.4% (twice the backtest's worst, −6.21%);
  - paper equity falls 20% from its peak (the backtest's worst was −11.9%);
  - over the first 25 trades, the win rate is below 60% (the acceptance line).

A flat or negative six-month stretch is **not** by itself a halt. The backtest spent long periods trailing SPY, and 2023 made only +5.2%.

## 11. Post hoc (not part of the verdict)

The seed sensitivity of the timing placebo, the best- and worst-day removals, the April 2025 single-session effect, the rolling Sharpe, SPY's worst days, and the since-May-2025 window are above. All were computed by `posthoc.py` after the run and are labelled where used. None of them changes §8.

### Ideas for a new study

- A short-put overlay or put spread on SPY, which is the direct form of the premium this rule collects indirectly. It is not testable here, because the store holds one option snapshot, not a history.
- The same rule on forward data (after 2026-09-25), or on index history before 2021 from another source, with these rules unchanged. That is the only way to test it on data no study here has read.

## 12. Reproduce

From the repo root:

```bash
python research/spy-rsi2-dip-buy/research/counts.py
```

```bash
python research/spy-rsi2-dip-buy/research/backtest.py --reason "reproduce"
```

```bash
python research/spy-rsi2-dip-buy/research/verify.py
```

```bash
python research/spy-rsi2-dip-buy/research/posthoc.py
```

```bash
python research/spy-rsi2-dip-buy/research/charts.py
```

- `counts.py` writes the pre-lock checks to `counts.json`.
- `backtest.py` runs the self-test and refuses to run if `RULES.md` no longer matches `RULES.lock`. It writes `results.json`, `daily.csv`, `trades.csv`, the placebo draws, and a `RUNLOG.md` entry.
- `verify.py` replays every trade independently.
- `posthoc.py` writes `posthoc.json` and `rolling_sharpe.csv`.
- `charts.py` draws the figures from those files.

Each script runs in under ten seconds. The seeds are 20260926 (direction), 20260927 (timing), and 20260928 (bootstrap). A rerun on the same store reproduces every number.

### References

- Campbell, J. Y., S. J. Grossman, and J. Wang (1993). Trading Volume and Serial Correlation in Stock Returns. *Quarterly Journal of Economics* 108(4).
- Cheng, M., and A. Madhavan (2009). The Dynamics of Leveraged and Inverse Exchange-Traded Funds. *Journal of Investment Management*.
- Connors, L., and C. Alvarez (2008). *Short Term Trading Strategies That Work*. TradingMarkets Publishing.
- McLean, R. D., and J. Pontiff (2016). Does Academic Research Destroy Stock Return Predictability? *Journal of Finance* 71(1).
- Nagel, S. (2012). Evaporating Liquidity. *Review of Financial Studies* 25(7).
- Wilder, J. W. (1978). *New Concepts in Technical Trading Systems*. Trend Research.
