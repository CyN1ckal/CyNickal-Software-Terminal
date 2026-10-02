# Intraday channel trend: QQQ, SPY, and IGV

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** Failed the pre-registered paper-trading tests. |
| Instruments | QQQ, SPY, and IGV, equal weight, regular hours, flat every night |
| Data | 2021-09-27 to 2026-09-25, read via `agent-data/mdq.py` |
| Rules | [`research/intraday-channel-trend/research/RULES.md`](../research/RULES.md), written before any run |
| Code | [`research/intraday-channel-trend/research/`](../research/) |

## 1. Summary

**Result.** A 15-minute Donchian breakout with a chandelier trail lost money on the equal-weight book. From 27 September 2021 through 25 September 2026 the book returned **−9.1%** after 1 bp per side (Sharpe **−0.14**). From 1 July 2024 through 25 September 2026 it returned **−12.5%** (Sharpe **−0.56**, profit factor 0.89). With costs set to zero, that out-of-sample window was still down 0.9%.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-09-27 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,254 | 692 | 562 |
| Total return | **−9.1%** | +3.9% | **−12.5%** |
| CAGR | −1.9% | +1.4% | −5.8% |
| Annual volatility | 9.9% | 10.0% | 9.8% |
| Sharpe | **−0.14** | 0.19 | **−0.56** |
| Max drawdown | −27.7% | −19.4% | −21.2% |
| Trades / profit factor | 4,245 / 0.98 | 2,384 / 1.03 | 1,861 / 0.89 |
| Avg net trade | −0.50 bp | +0.65 bp | −1.98 bp |
| *Close-to-close Sharpe (max DD)* | *0.59 (−35.0%)* | *0.40 (−35.0%)* | *0.85 (−22.5%)* |

It failed three of the five acceptance tests written before the first run (§8). QQQ, SPY, and IGV each lost money out of sample. Every cell of the parameter grid did too.

**Why the in-sample profit did not survive:**

1. **The gross edge was already gone out of sample.** The average out-of-sample trade made +0.02 bp before costs. A round trip at the assumed 1 bp per side costs 2 bp. Zero cost still left the out-of-sample book at −0.9%.
2. **In sample the edge was smaller than a round trip.** The average gross trade was +2.65 bp. After 1 bp per side, +0.65 bp remained, and the in-sample Sharpe was 0.19.
3. **The three names are one bet.** Daily strategy returns correlate 0.85 (QQQ–SPY), 0.75 (QQQ–IGV), and 0.72 (SPY–IGV). Averaging them does not create three independent results.
4. **The trailing year was negative most of the time.** A 252-session Sharpe of the book's daily returns was positive in 20% of windows. The low was −2.71 in the year ending 4 May 2026.

**Recommendation.** Do not paper-trade it. The hold-to-close variant, the long side alone, the short side alone, and every grid neighbour are diagnostics. The rules do not allow one of them to replace the primary after the fact.

## 2. The strategy

### Rules

On each regular-hours session, for each symbol, using 15-minute bars anchored at 09:30:

```
At the close of bar i, once eight prior bars exist:
    upper = highest high of the previous 8 bars
    lower = lowest low of those bars
    ATR   = mean true range of the 8 bars ending at i

    LONG  if close > upper
    SHORT if close < lower

While in a position, at each later close:
    stop  = extreme since entry ∓ 2.5 × ATR
    exit  if the close breaks that stop
    flip  if the opposite breakout prints and the stop does not

Fill entries and flips at the next bar's open, and only before 15:30
(12:30 on an early close). Stops may fill after that clock.
Flatten anything still open at the session's last print.
```

- **Why eight bars.** A break of the prior two hours is a move that has left the range the ETF just traded. The first fill of the day lands at 11:45 when the grid is intact.
- **Why the chandelier.** The stop is the classic trend exit: give back 2.5 true-range units from the extreme since entry. It is recomputed every bar, so a wider ATR can move the stop away from price. It is not a one-way ratchet.
- **Why the book is an average.** Each name is sized at 1× its own equity at the open. The book day is the mean of the three. A name with no trade contributes zero. There is no leverage and no rescaling when a name is flat.

### How it trades

| | |
|---|---|
| Sessions with a trade | 1,234 of 1,254 (98%) |
| Trades | 4,245. About 1.1 round trips per name per session |
| Time in market | 40% of regular-hours minutes |
| Holding time | median 135 min, mean 139 min. 62% of trades are held to the close |
| Long / short | 2,333 / 1,912 trades. Both sides lose after 1 bp (average −0.14 bp / −0.94 bp) |
| Win rate | 44.9%. Winners average +45.1 bp, losers −37.7 bp |

The win/loss shape is what a trail is built to produce. The average trade is still negative once the round trip is paid.

## 3. Method

- **Data.** QQQ, SPY, and IGV 1-minute bars came from `data/market-data.sqlite` via `agent-data/mdq.py`, then 15-minute bars from `mdq.resample` (buckets from 09:30, no bar invented for an empty bucket). Prices are split-adjusted. Dividends are not. The book is flat overnight, so it earns no dividend and takes no overnight gap. The three names share the same 1,254 sessions. QQQ's minute counts match the early-close calendar: 390 on a full day and 211 on each date in `mdq.EARLY_CLOSES`. No minute bar was dropped for a bad price. Every session, including IGV, has a 09:30 bucket and a closing bucket. IGV's only corporate action is a 5-for-1 split on 7 March 2024; the ex-date bar is already post-split, and no position is held across it.
- **Pre-registration.** [`RULES.md`](../research/RULES.md) fixed the following before the first run:
  - the rules and the primary parameters (N = 8, K = 2.5, 1 bp per side, next-bar fill);
  - the in-sample / out-of-sample split (2024-06-28 / 2024-07-01);
  - costs, the grid, and every check in §§4–7;
  - five acceptance criteria.

  Nothing in this report was used to choose a different rule.
- **Fills.** A signal uses the close of a 15-minute bar. The fill is the next real bar's open. The final exit uses that session's last 15-minute close.
- **Costs.** 1 bp of notional per side. A flip is two trades, so it pays four sides. No borrow, and no interest on idle cash.
- **Returns.** Daily simple returns; flat sessions count as zero. Sharpe uses a zero risk-free rate and √252. CAGR uses a 252-session year.
- **Verification.** `backtest.py` replays hand-built sessions before it opens the store: a stop on the next open, a blocked 15:30 entry, a flip, a missing bucket, a two-bar delay that keeps the original order, and the early-close cutoff. On the live tape, QQQ on 9 April 2025 broke the prior eight-bar high at the 12:45 close (423.26 vs 422.98) and filled at the 13:00 open, 423.36, which is the trade in `trades.csv`.

## 4. Results

![Growth of $1](figures/equity.svg)

![Drawdown](figures/drawdown.svg)

The close-to-close line is the equal-weight price return of the three names, including the overnight gap. Open-to-close is the intraday drift, first print to last print. Neither benchmark is charged the strategy's costs, and neither includes dividends. The book's correlation with open-to-close is 0.09 and with close-to-close is 0.11.

| | Full: return / Sharpe / max DD | In-sample Sharpe | Out-of-sample: return / Sharpe / max DD |
|---|---|---:|---|
| **Channel book, 1×, 1 bp** | −9.1% / **−0.14** / −27.7% | 0.19 | −12.5% / **−0.56** / −21.2% |
| Same rules, chandelier off | −17.3% / −0.31 / — | 0.04 | −16.8% / −0.76 / — |
| *Close to close* | *+68.0% / 0.59 / −35.0%* | *0.40* | *+40.7% / 0.85 / −22.5%* |
| *Open to close* | *+33.0% / 0.40 / −23.9%* | *0.49* | *+8.4% / 0.29 / −16.8%* |

Per name, at 1 bp per side:

| | In-sample return / Sharpe | Out-of-sample return / Sharpe | Full return / Sharpe |
|---|---|---|---|
| QQQ | +9.6% / 0.35 | −15.4% / −0.63 | −7.3% / −0.08 |
| SPY | −5.1% / −0.16 | −5.2% / −0.22 | −10.0% / −0.19 |
| IGV | +6.8% / 0.26 | −16.9% / −0.66 | −11.3% / −0.14 |

SPY lost on both sides of the cut. QQQ and IGV were positive in sample and lost more than the book out of sample.

![Calendar-year return](figures/by_year.svg)

| Year | Book | Sharpe | Max DD | Close to close |
|---|---:|---:|---:|---:|
| 2021 (from Sep 27) | +0.9% | 0.42 | −3.4% | +4.1% |
| 2022 | **+6.7%** | 0.55 | −14.5% | **−29.9%** |
| 2023 | −2.5% | −0.31 | −6.9% | +45.0% |
| 2024 | −0.4% | −0.03 | −5.5% | +24.1% |
| 2025 | −3.8% | −0.24 | −11.9% | +14.1% |
| 2026 (to Sep 25) | −9.7% | −2.46 | −11.4% | +12.2% |

2024 contains both the last months of the in-sample window and the first months of the out-of-sample window. 2025 and 2026 are entirely out of sample. 2022 is the year a short-capable intraday book had a falling tape to work with: open-to-close was −11.8% and the book made 6.7%. From 2023 on, close-to-close rose every year and the book lost every year. The drawdown from 11 May 2022 to 4 September 2026 is −27.7%. Of 61 months, 35 were negative. The best month was April 2025 (+8.5%); the worst was May 2025 (−6.1%).

The April 2025 gain is one afternoon. On 9 April the book was long all three names from 13:00 to the close. QQQ went from 423.36 to 466.00, about +1,005 bp after cost. The largest loser in the file is IGV on 21 September 2022, long from 53.71 to 52.03 on the split-adjusted scale, about −315 bp. QQQ was stopped once that day and re-entered.

## 5. Is the loss a fluke?

### Placebo

The test keeps every trade's entry and exit and its cost, and multiplies the gross price return by a coin flip. Of 2,000 redraws (seed 20260926), the random books averaged a Sharpe of −0.94. That is the cost of the same round trips with no directional edge. The actual full-sample Sharpe, −0.14, beat all but a thin right tail: p = 0.042, and the null's 95th percentile was −0.19.

![Placebo](figures/placebo.svg)

That p-value clears the pre-registered line of 0.05. It says the direction of the full-sample trades beat a coin flip, because the in-sample gross edge was real and the coin flips throw it away while still paying 1 bp. It does not say the out-of-sample trades had a gross edge. The cost table below is that measurement: out of sample the average trade made +0.02 bp before costs.

### Bootstrap

A circular block bootstrap of the book's daily returns (20-session blocks, 2,000 draws, seed 20260927) gives a 95% interval for the full-sample Sharpe of **−1.07 to +0.62**. The point estimate misses the hurdle. The interval still includes a gain. The out-of-sample t-statistic of the mean daily return is −0.84. The decision in §8 uses the pre-registered hurdles, which the point estimates miss, rather than a claim that the loss is precisely measured.

### Parameter plateau

N is the channel length in 15-minute bars. The true-range window is the same length. K is the chandelier multiple. The outlined cell is the primary, N = 8 and K = 2.5. Nothing was selected from the grid.

![Parameter grid](figures/grid.svg)

- **In sample, 17 of 20 cells had a positive Sharpe.** The primary sits on a plateau, not on a one-cell spike.
- **Out of sample, all 20 cells were negative.** The least-bad cell was N = 6, K = 2.0, at −0.45.
- **Two cells have a positive full-sample total return:** N = 8, K = 2.0 at +1.2%, and N = 12, K = 2.0 at +3.6%. Both lose money out of sample (Sharpe −0.45 and −0.69). The rules leave them as neighbours, not as replacements.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 bp | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.43 | 0.14 | **−0.14** | −0.72 | −1.28 |
| Out-of-sample Sharpe | 0.00 | −0.28 | **−0.56** | −1.13 | −1.68 |
| Full-sample return | +20.7% | +4.7% | **−9.1%** | −31.5% | −48.4% |
| Out-of-sample return | −0.9% | −6.9% | **−12.5%** | −22.7% | −31.7% |

The path does not depend on the cost. Only the P&L does. Out of sample the book is flat before costs and loses as soon as a cost is charged. At 2 bp per side the full-sample return is −31.5%, which fails the acceptance test that required a gain at that cost.

Filling one bar later, or filling at the signal close (an upper bound a live order cannot achieve), does not change the out-of-sample loss:

| Fill | Full sample | Out of sample |
|---|---|---|
| Next bar's open (primary) | −9.1%, Sharpe −0.14 | −12.5%, Sharpe −0.56 |
| One extra bar later | −9.1%, Sharpe −0.16 | −12.4%, Sharpe −0.60 |
| At the signal close | −6.8%, Sharpe −0.10 | −12.1%, Sharpe −0.54 |

## 6. Where the result comes from

![By open-to-close quintile](figures/move_quintiles.svg)

Sessions are bucketed by the equal-weight open-to-close move:

| Quintile | Sessions | Mean open-to-close | Mean book day |
|---|---:|---:|---:|
| 1, weakest | 251 | −1.54% | **+13.1 bp** |
| 2 | 251 | −0.44% | −12.7 bp |
| 3 | 250 | +0.08% | −15.9 bp |
| 4 | 251 | +0.54% | −6.1 bp |
| 5, strongest | 251 | +1.50% | **+18.8 bp** |

The book is paid on the large up days and the large down days. The three middle quintiles, which are most sessions, lose more than the tails make. That is a trend-following shape whose convexity is too small for the costs.

Exit reason, full sample, at 1 bp. "Sum of trade returns" adds each trade's return on that symbol's equity at the session open. It is the right total for comparing exits. It is not the compounded book return.

| Exit | Trades | Win rate | Average | Sum of trade returns |
|---|---:|---:|---:|---:|
| Held to the close | 2,641 | 69.0% | +25.8 bp | +6.82 |
| Chandelier stop | 1,293 | 5.7% | −44.9 bp | −5.80 |
| Flipped | 311 | 2.9% | −39.7 bp | −1.23 |

Trades that reach the close are the trends. Stops and flips give that sum back. Turning the chandelier off, so a position ends only on an opposite breakout or at the close, makes the book worse: −17.3% full sample, −16.8% out of sample. The trail reduced the loss. It did not produce a gain.

![By entry hour](figures/by_entry.svg)

Entries from 13:00 to 13:59 are the only hour with a clearly positive average (+3.4 bp, 1,078 trades). Entries at 14:00 and 15:00 lose (−3.9 bp and −2.6 bp). That split was reported because the rules asked for it. It was not used to drop an hour.

## 7. Checks that do not rescue it

| Check | What changed | Out-of-sample book |
|---|---|---|
| Primary | N = 8, K = 2.5, next-bar fill, 1 bp | −12.5%, Sharpe −0.56 |
| Chandelier off | Same entries, hold to a flip or the close | −16.8%, Sharpe −0.76 |
| One extra bar of delay | Fill two bars after the signal | −12.4%, Sharpe −0.60 |
| Signal-close fill | Upper bound, not a live order | −12.1%, Sharpe −0.54 |
| Drop thin IGV sessions | Zero any name-session under 90% of expected minutes | unchanged, −12.5% |

IGV has 471,094 one-minute bars against about 487,270 for QQQ and SPY. The gaps are inside the day, so an eight-bar channel on a thin session can span more than two hours of clock time. The 90% filter removes 150 IGV sessions and no QQQ or SPY sessions. All 150 sit before July 2024, which is why the out-of-sample book does not move. The filter also uses the completed day's bar count, so it could not have been applied at 11:45. The full-sample book goes from −9.1% to −6.2% under it, and remains a loss.

At zero cost out of sample, SPY was +7.4% (Sharpe 0.39) while QQQ was −4.3% and IGV was −5.9%. SPY's gross Sharpe is already under the 0.5 hurdle, and at 1 bp SPY's out-of-sample result is −5.2%.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| Out-of-sample Sharpe | ≥ 0.50 | −0.56 | ❌ |
| Out-of-sample profit factor | ≥ 1.10 | 0.89 | ❌ |
| Placebo p-value (full) | ≤ 0.05 | 0.042 | ✅ |
| In-sample Sharpe | > 0 | 0.19 | ✅ |
| In-sample grid cells with Sharpe > 0 | ≥ 60% | 85% (17 of 20) | ✅ |
| Full-sample return at 2 bp/side | > 0 | −31.5% | ❌ |
| Out-of-sample names with Sharpe > 0 | ≥ 2 of QQQ, SPY, IGV | 0 | ❌ |

The placebo line passes because a costly coin flip is a low bar, as §5 explains. The return lines fail. A failed line fails the strategy.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence |
|---|---|
| **No out-of-sample gross edge** | Average out-of-sample trade +0.02 bp before costs. The book is −0.9% at zero cost |
| **Edge decay inside the sample** | Trailing 252-session Sharpe peaked at +1.01 and bottomed at −2.71 (year to 4 May 2026). It was positive in 20% of windows |
| **One factor, three tickers** | Pairwise correlations of the strategy returns are 0.72 to 0.85. Book volatility is 9.9% against SPY's own 9.2% |
| **Costs** | In sample, the gross edge was 2.65 bp and the round trip was 2 bp. Out of sample there is nothing to pay a spread with |
| **Almost always in a trade** | 98% of sessions have at least one fill. Quiet days are where §6 shows the book loses |
| **Short sample** | Five years of regular-hours history. No 2008 or 2020 |
| **Closing print** | The model exits at the last 15-minute close. The official close is the 16:00 auction |
| **IGV gaps** | 150 sessions have fewer than 90% of the expected minutes. Dropping them does not change the out-of-sample result |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests that were written to decide that question, and the pre-registered neighbours failed with it.

A later sample, or a different rule, would be a new study with its own rules file written before the run. This one stops at the result above.

## 11. Reproduce

From the repo root:

```bash
python research/intraday-channel-trend/research/backtest.py
```

```bash
python research/intraday-channel-trend/research/charts.py
```

`backtest.py` runs the self-test, then writes `summary.json`, `daily.csv`, and `trades.csv` in about 25 seconds. `charts.py` writes `research/intraday-channel-trend/report/figures/*.svg`. The placebo seed is 20260926 and the bootstrap seed is 20260927, so a rerun on the same store gives the same numbers. `daily.csv` is the primary book at 1 bp per side, with the two benchmark returns in the same rows.
