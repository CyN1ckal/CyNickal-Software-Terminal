# 15-minute Turtle breakout held overnight: QQQ

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** Failed 3 of the 6 pre-registered criteria (4 of 8 lines). |
| Instruments | QQQ (primary); SPY and IGV under identical rules. Regular-hours 15-minute bars. Positions carried overnight, over weekends, and over holidays |
| Data | 2021-09-27 → 2026-09-25, read via `agent-data/mdq.py`. Evaluated 2021-10-25 → 2026-09-25 (1,235 sessions) after a 20-session warm-up. 2021-12-31 has no bars and is a zero-return day. No dividends in the store; prices are not dividend-adjusted |
| Rules | [`research/qqq-15m-turtle-overnight/research/RULES.md`](../research/RULES.md), locked 2026-09-26 16:46:59 UTC, sha256 `d5fb6d5043b0` |
| Code | [`research/qqq-15m-turtle-overnight/research/`](../research/) · 1 store run, plus 1 verification replay (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Turtle System 2 (55-bar entry channel, 20-bar exit channel, 2N stop) on QQQ 15-minute bars, holding positions through the close, made almost nothing out of sample. From 1 July 2024 through 25 September 2026 it returned **+0.49%** after 1 bp per side (Sharpe **0.09**, profit factor 1.02, 229 trades). QQQ itself returned +55.4% over the same period. Over the full evaluation window the strategy returned +38.4% (Sharpe 0.50). Nearly all of that came in the in-sample years.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-25 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,235 | 673 | 562 |
| Total return | +38.4% | +37.7% | **+0.49%** |
| CAGR | 6.9% | 12.7% | 0.2% |
| Annual volatility | 15.9% | 16.4% | 15.3% |
| Sharpe | 0.50 | 0.81 | **0.09** |
| Max drawdown | −24.8% | −12.3% | −24.8% |
| Trades / profit factor | 500 / 1.16 | 271 / 1.26 | 229 / 1.02 |
| Avg net trade | +7.6 bp | +13.1 bp | +1.1 bp |
| *QQQ buy and hold Sharpe (max DD)* | *0.72 (−35.6%)* | *0.51 (−35.6%)* | *1.01 (−22.9%)* |

It failed the out-of-sample Sharpe and profit-factor lines, the direction placebo, and the SPY cross-market line (§8). It passed the in-sample, grid-plateau, 2× cost, and minimum-sample lines.

**Why it failed:**

1. **Holding overnight lost money, which is the opposite of the idea.** Over the full sample, the overnight gaps the strategy held summed to **−33%** of entry notional. That figure was negative for longs (−11%) and for shorts (−22%). The hours the market was open contributed **+81%**. The same rule flattened at every close had a *higher* gross Sharpe (0.73) than the overnight version (0.61). Prediction 1, the core of the hypothesis, is **not consistent** (§3).
2. **Out of sample the gross edge nearly vanished.** The average out-of-sample trade made +3.1 bp before costs. With costs set to zero, out-of-sample Sharpe was still only **0.23**, below the 0.5 threshold. Costs made the result worse, but they are not the cause.
3. **The direction of the trades did not clearly beat chance.** The actual full-sample gross Sharpe was 0.61. Randomizing each trade's direction gave a 95th percentile of 0.70 and **p = 0.075**, above the 0.05 line.
4. **Shorts lost out of sample.** Out-of-sample longs averaged +19.0 bp net over 118 trades. Shorts averaged −17.9 bp over 111 trades.
5. **SPY lost under identical rules** (out-of-sample Sharpe −0.48, full −5.1%). IGV was the exception (out-of-sample Sharpe 1.01). The acceptance table does not use IGV, and a name picked after seeing results cannot replace the primary (§6, §11).
6. **Recent performance was negative.** The strategy lost 11.6% in 2025. **(post hoc)** From May 2025 its Sharpe was −0.34, and the trailing 252 sessions to 2026-09-25 returned −5.3%.

**Recommendation.** Do not trade it. The rules forbid promoting any of these after the fact: IGV, the IS-best grid cell (40/20), the long side alone, the flat-at-close variant, a different cost or fill, or a filter that removes FOMC-style whipsaw days.

## 2. The strategy

### Rules

```
Bars: QQQ 15-minute regular-hours bars (09:30-anchored), concatenated across sessions.
TR_i  = max(H−L, |H−C_prev|, |L−C_prev|)    (C_prev may be yesterday's last bar: the gap counts)
N_i   = Wilder mean of TR over 20 bars       (N_i = (19·N_{i−1} + TR_i)/20)
HI55, LO55 = highest high / lowest low of the previous 55 bars (≈ 2.1 sessions)
HI20, LO20 = the same over the previous 20 bars (≈ 0.8 session)

At every 15-minute close:
  flat:  close > HI55 → buy;  close < LO55 → sell short
  long:  exit if close ≤ entry − 2·N_signal (stop) or close < LO20 (channel);
         reverse to short if also close < LO55
  short: mirror
Every order fills at the next bar's open. A signal on the 15:45 bar fills at the next session's 09:30 open.
No flatten at the close. 1× equity per position, compounding. 1 bp per side (SPY 1 bp, IGV 2 bp).
```

- **Why these counts.** They are Turtle System 2's published daily counts (Faith 2007), applied unchanged to 15-minute bars because no 15-minute default exists. The entry channel then spans about two sessions and the exit channel most of one, so positions naturally carry through the close. That is the user's idea.
- **Why close-confirmed stops.** Every rule decides at a bar close and fills at the next open, including stops. The Turtles used resting stop orders. A gap through the stop overnight is only acted on at the next morning's first close.
- **One unit, no pyramiding.** With one instrument, N-based sizing would only change leverage.

### How it trades

| | |
|---|---|
| Sessions with an entry | 476 of 1,235 |
| Trades | 500 (259 long, 241 short) |
| Time in market | 57% of 15-minute closes; a position was held over 57.5% of overnights |
| Holding time | median 26 bars (one session) / mean 36.4 bars; median 1 overnight, mean 1.42 |
| Long / short | PF 1.36 / 0.97 |
| Win rate | 37%. Average winner +153 bp, average loser −78 bp |
| Exits | 259 channel (avg net +92.9 bp), 223 stop (avg −83.0 bp), 6 channel + reversal, 12 stop + reversal |

## 3. Hypothesis and predictions

The pre-registered claim was that a close outside the previous two sessions' range is followed by continuation over hours to days, and that some of it is paid across the overnight gap. Three sources were cited. Price-insensitive hedging flows push late-day moves further (Cheng & Madhavan 2009; Baltussen, Da, Lammers & Martens 2021). News is incorporated slowly (Gao, Han, Li & Zhou 2018; Moskowitz, Ooi & Pedersen 2012). Momentum returns are earned overnight (Lou, Polk & Skouras 2019). The rules named the counter-forces in advance: overnight drift that shorts pay (Cliff, Cooper & Gulen 2008), overnight reversal after intraday selloffs (Boyarchenko, Larsen & Whelan 2023), and the failed 15-minute breakout study on the same data.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. The overnight hold is paid: summed overnight contribution > 0, **and** the flat-at-close variant has a lower gross Sharpe | Overnight sum **−33.1%** (long −11.1%, short −22.0%; 709 overnights). Flat variant gross Sharpe 0.73 vs primary 0.61 | **Not consistent** |
| 2. Profit concentrates on large-move days: the top quintile of \|QQQ close-to-close\| has the highest mean, and it is positive | Top quintile +50 bp a day; the next highest is +11 bp. The bottom three quintiles are negative | Consistent |
| 3. Trend convexity: positive mean in both the worst and the best quintile of QQQ's signed move | Worst quintile +34 bp, best quintile +28 bp. The middle three average −30, −20, and +3 bp | Consistent |

The rule behaves like a trend follower during the session: it makes money on large days in either direction and pays for them on quiet days. But the premise that motivated holding overnight was wrong on this data. The overnight gaps it held worked against it on both sides. That matches the counter-forces better than the mechanism: shorts carried into the open lost 7.5 bp per overnight on average, and even longs lost 2.7 bp. The mechanism is **not confirmed**.

![Decomposition](figures/decomposition.svg)

The figure splits each trade's gross return into the part earned while the market was open and the part earned across the overnight gaps it held. The overnight bars are negative for both sides.

![Overnight vs flat](figures/overnight_vs_flat.svg)

The flat-at-close variant, a mechanism check and not a candidate, ended the full sample at +20.6% (Sharpe 0.40, 881 trades). It lost 2.2% out of sample (Sharpe −0.05). Neither version works out of sample. The overnight version had the lower gross Sharpe (0.61 vs 0.73) but made far fewer round trips (500 vs 881). Its higher net return came from paying fewer costs, not from earning the gap.

## 4. Method

- **Data.** QQQ, SPY, and IGV 1-minute bars, resampled by mdq into 15-minute buckets anchored at 09:30. Coverage check: each symbol has 1,254 sessions with bars, 26 buckets on every full session and 15 on the 10 early closes. 2021-12-31 was never fetched (the known terminal-calendar bug). It is a zero-return session, and the gap from 12-30 to 01-03 lands on 2022-01-03. IGV's 5-for-1 split (2024-03-07) is back-adjusted. The script's data checks passed for all three symbols.
- **Dividends.** None are in the store, so prices are not dividend-adjusted. A long held into an ex-date is understated by the dividend, and a short is overstated. The pre-registered approximate estimate assumed QQQ ex-dates on the Monday after each quarter-month's third Friday at 0.14%, and SPY on that Friday at 0.32%. QQQ held a position across 11 assumed ex-sessions (5 long, 6 short). The correction sums to −0.14% and moves the full-sample return from +38.4% to +38.2%. For SPY (7 long, 6 short) it moves −5.1% to −4.8%. It does not matter here.
- **Pre-registration.** [`RULES.md`](../research/RULES.md) fixed the rules, parameters, samples, costs, every check, and the acceptance table before any return was computed. Pre-lock looks were coverage, corporate actions, bar counts, and counts of 55-bar breakout onsets (`counts.py`). **Prior exposure:** the out-of-sample window was already used by four studies. I knew QQQ rose 55% over it, that it contains the April 2025 crash, and that session-local 15-minute breakouts had no gross edge there. Choosing QQQ as the primary was informed by `qqq-intraday-trend`. To offset that, the rules required SPY specifically to pass the cross-market line. RULES.md was locked with a hash, but it was **not committed to git** before the first run. The lock time (16:46:59 UTC) precedes the first run-log entry (16:52:02 UTC).
- **Fills and costs.** Every order fills at the next 15-minute bar's open, including overnight. Costs are 1 bp of notional per side for QQQ and SPY (QQQ's one-cent spread is about 0.15 bp), and 2 bp for IGV. No borrow fee was charged on shorts; general-collateral borrow of about 0.25–0.5% a year on the short share would lower the result further. There is no interest on cash.
- **Returns.** Daily simple returns on the NYSE calendar, marked at each session's last 15-minute close. Flat days count as 0. Sharpe is mean ÷ sample SD × √252 with a zero rate. Trades are assigned to the sample of their entry session.
- **Verification.** The self-test covered 10 groups of synthetic cases, with trades and stop levels computed by hand: long and short entries, channel and stop exits, stop precedence, reversals, an overnight fill, a gap through the stop, the end of data, a cancelled last-bar entry, a short session, the warm-up boundary, cost accounting on a reversal, the flat-at-close variant, both fill-delay variants, and the P&L decomposition. It passes. `verify.py` builds its own 15-minute bars from 1-minute data and replays every trade with plain loops. It matched **all 500 QQQ, 522 SPY, and 477 IGV trades** on side, entry time and price, exit time and price, and reason.
- **Runs.** There was one store run of `backtest.py`. The first verify pass matched QQQ and SPY but differed on 15 IGV trades. The difference was only the time *label*: verify named a bucket after its first print (13:31) when the bucket's first minute was untraded, while mdq names it after the bucket open (13:30). Prices, sides, and reasons matched. The fix was in `verify.py`, so the headline numbers did not change. Both steps are in the run log. There were no deviations from the literal rules.

## 5. Results

![Growth of $1](figures/equity.svg)

The strategy beat QQQ through 2022 by making money while QQQ fell. It then went sideways while QQQ rose, and it ended below buy-and-hold. Almost all of its gain came before the out-of-sample boundary.

![Drawdown](figures/drawdown.svg)

The strategy's worst drawdown (−24.8%) came out of sample. It was shallower than QQQ's −35.6% in 2022, but not by much for a strategy with a Sharpe of 0.09 over that window.

| Strategy (1 bp/side) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary, held overnight** | +38.4% / 0.50 / −24.8% | 0.81 | +0.49% / **0.09** / −24.8% |
| Flat at every close (mechanism check) | +20.6% / 0.40 / −16.5% | 0.72 | −2.2% / −0.05 / −16.5% |
| *QQQ buy and hold, price only* | *+99.0% / 0.72 / −35.6%* | *0.51* | *+55.4% / 1.01 / −22.9%* |

The strategy's daily returns had a correlation of −0.01 with QQQ's.

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | QQQ buy and hold |
|---|---:|---:|---:|---:|
| 2021 (from Oct 25) | +3.2% | 1.36 | −5.7% | +7.0% |
| 2022 | **+16.4%** | 0.82 | −12.3% | **−33.5%** |
| 2023 | +11.3% | 0.88 | −7.9% | +53.8% |
| 2024 | +15.1% | 1.13 | −10.3% | +24.8% |
| 2025 | **−11.6%** | −0.72 | −17.0% | +20.2% |
| 2026 (to Sep 25) | +1.8% | 0.23 | −10.8% | +21.2% |

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The placebo keeps every trade's entry and exit times and flips each trade's direction at random, before costs. The series is the fixed-notional gross daily return defined in RULES.md. The 2,000 random books averaged a Sharpe of 0.00, with a 95th percentile of 0.70. The actual 0.61 gives **p = 0.075**. A trend rule's timing is itself built from direction, so the placebo is a fairly strict test. Still, it is the test the rules chose, and it did not pass.

### Bootstrap

A circular block bootstrap of full-sample daily returns (20-session blocks, 2,000 draws) gives a 95% interval of **−0.21 to 1.29** for the Sharpe. 8.1% of draws were at or below zero. The full-sample t-stat of the mean daily return is 1.10; out of sample it is 0.13.

### Parameter plateau

![Parameter grid](figures/grid.svg)

In sample, all 15 cells had a positive Sharpe, so the plateau line passed. The primary ranked 4th of 15 **(post hoc rank)**. Out of sample, 6 of 15 cells were positive, and the mean out-of-sample Sharpe across cells was −0.10. **(post hoc)** The in-sample best cell (40/20, IS 1.00) scored −0.02 out of sample. The rank correlation between in-sample and out-of-sample Sharpe was **−0.42**, so a better in-sample cell tended to do worse out of sample. Nothing was selected from the grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.63 | 0.56 | **0.50** | 0.37 | 0.24 |
| OOS Sharpe | 0.23 | 0.16 | **0.09** | −0.04 | −0.18 |
| Full-sample return | +52.9% | +45.5% | **+38.4%** | +25.2% | +13.3% |

- **Costs.** Out-of-sample Sharpe turns negative between 1 and 2 bp per side. Even at zero cost it is 0.23, below the 0.5 line.
- **Latency.** Filling one bar later gives Sharpe 0.45 full and 0.15 out of sample. The labelled upper bound, filling at the signal close, gives 0.54 and 0.05. Fill timing is not what separates a pass from a fail here.

### Other markets (identical rules)

![Cross-market](figures/cross_market.svg)

| | Cost | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---:|---|
| **QQQ** | 1 bp | 0.81 | **0.09** | +38.4% / 1.16 |
| SPY | 1 bp | 0.31 | **−0.48** | −5.1% / 0.99 |
| IGV | 2 bp | 1.21 | 1.01 | +184.8% / 1.40 |

The daily strategy returns correlate 0.76 (QQQ–SPY), 0.58 (QQQ–IGV), and 0.51 (SPY–IGV). SPY, the market the pre-registered line names, lost out of sample. IGV made money in both halves; **(post hoc)** its trailing-year Sharpe was positive in every 252-session window. IGV is the most volatile and the least liquid of the three, and the same idea failed on the most liquid index. That pattern is worth a new test (§11), but this study selected nothing on it: IGV was not in the acceptance table, and choosing it now would be choosing the symbol after seeing the results.

## 7. Where the result comes from

![By move quintile](figures/quintiles.svg)

| Quintile | 1 | 2 | 3 | 4 | 5 |
|---|---:|---:|---:|---:|---:|
| \|QQQ close-to-close\| range | 0–0.26% | 0.26–0.61% | 0.61–1.07% | 1.07–1.71% | ≥ 1.72% |
| Mean strategy day | −24 bp | −15 bp | −6 bp | +11 bp | **+50 bp** |
| Share of days positive | 22% | 34% | 41% | 52% | 61% |
| Signed QQQ move quintile mean | **+34 bp** (worst) | −30 bp | −20 bp | +3 bp | **+28 bp** (best) |

- **By side.** Full sample: longs +16.3 bp net over 259 trades (PF 1.36), shorts −1.7 bp over 241 (PF 0.97). Out of sample: longs +19.0 bp (118), shorts −17.9 bp (111).
- **By exit.** Channel exits averaged +92.9 bp (259 trades) and stops −83.0 bp (223). Reversals out of a stop averaged −179 bp over 12 trades.
- **By overnights held.** Trades closed the same session averaged −74 bp (129); those held one night −54 bp (202); two nights +39 bp (74); three to five nights +207 bp (89). This is how a trend rule's winners look: they run and get held longer. It is not evidence that holding pays.
- **By entry time.** Fills in the 09:45–10:59 hours were modestly positive (+12.5 bp over 142 trades; +12.9 bp over 102). The 12:00 hour was +58 bp (53). The 15:00 hour was −31 bp (51). The 12 fills at the 09:30 open, decided the evening before, averaged −17 bp.
- **Largest days.** Best: 2025-04-04 +6.0% (QQQ −6.2%), 2025-04-09 +4.9% (QQQ +12.0%), 2022-06-13 +4.2% (QQQ −4.6%). Worst: 2025-04-10 −5.8% (QQQ −4.3%; a long entered during the 04-09 rebound and held overnight), 2022-09-21 −5.8% (QQQ −1.8%; FOMC day, three stops and two reversals in 90 minutes), 2025-04-30 −3.5% (QQQ flat).

None of these breakdowns was used to filter the rule.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| 1a. OOS Sharpe | ≥ 0.50 | 0.09 | ❌ |
| 1b. OOS profit factor | ≥ 1.10 | 1.02 | ❌ |
| 2. Direction placebo p, full sample, gross | ≤ 0.05 | 0.075 | ❌ |
| 3a. IS Sharpe | > 0 | 0.81 | ✅ |
| 3b. IS grid cells with Sharpe > 0 | ≥ 60% (9 of 15) | 100% (15 of 15) | ✅ |
| 4. Full-sample return at 2 bp per side | > 0 | +25.2% | ✅ |
| 5. SPY OOS Sharpe, identical rules | > 0 | −0.48 | ❌ |
| 6. OOS trades (minimum sample) | ≥ 100 | 229 | ✅ |

The minimum sample was met, so the verdict is **Rejected**, not Inconclusive. Passing lines 3 and 4 shows the in-sample fit and the full-sample cost tolerance. Most of the full-sample profit came in sample, so neither line says the rule works now.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

**(post hoc)** Trailing 252-session Sharpe for the three names under identical rules. QQQ and SPY turned negative during 2025. IGV stayed above zero. The first point is 252 sessions into the sample.

| Risk | Evidence |
|---|---|
| Decay | **(post hoc)** QQQ's trailing-year Sharpe was positive in 67% of windows; its low was −2.00, in the year to 2026-04-13. Since May 2025: Sharpe −0.34, −7.0% |
| Concentration | **(post hoc)** Zeroing the best 5 / 10 / 20 days takes the full-sample Sharpe to 0.21 / −0.01 / −0.41. Removing April 2025 raises the OOS Sharpe to 0.17 (April 2025 itself was −2.5%) |
| Generalization | SPY OOS −0.48 under identical rules |
| Whipsaw | **(post hoc)** 12 sessions had two or more stop-outs. Trades exiting on those days summed to −22.8% of notional (simple sum) |
| Overnight exposure | The gaps it held cost −33% of notional in total. Stops are only acted on after the first 15-minute close, so a gap can run far past the stop |
| Data | Price-only returns (dividend bias estimated at ≈0.1%); no borrow cost; 5 years of 15-minute history in one regime family, with no 2008 or 2020 |

## 10. No deployment

There is no paper-trading proposal. The primary failed the tests written to decide that question. A different rule, symbol, or later data is a new study with its own RULES.md.

## 11. Post hoc (not part of the verdict)

Everything below was computed after the pre-registered run, in `posthoc.py` from `daily.csv` and `trades.csv`. None of it changes §8.

- **Rolling Sharpe and concentration**: §9.
- **Grid selection bias**: the in-sample best cell fell to −0.02 out of sample. The rank correlation between in-sample and out-of-sample cell Sharpe was −0.42 (§6).
- **By side, gross**: in sample, longs averaged +16.0 bp gross and shorts +14.1 bp. Out of sample, longs averaged +21.0 bp and shorts −15.9 bp. The short side's collapse coincides with the rising market of the out-of-sample window.
- **Cross-market rolling**: IGV's trailing-year Sharpe never fell below 0.12. SPY's was positive in 50% of windows.

### Ideas for a new study

- **The same Turtle rule on IGV.** It was found on this data. IGV was a cross-market check here, not a candidate, and it lost money out of sample under two other intraday trend rules on the same window (`qqq-intraday-trend`, `intraday-channel-trend`). It needs its own RULES.md, a cost basis for a thin tape, and data this study did not use: paper trading, or IGV 15-minute history after 2026-09-25.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-15m-turtle-overnight/research/backtest.py --reason "reproduce"
```

```bash
python research/qqq-15m-turtle-overnight/research/verify.py
```

```bash
python research/qqq-15m-turtle-overnight/research/posthoc.py
```

```bash
python research/qqq-15m-turtle-overnight/research/charts.py
```

- `backtest.py` checks the RULES.md hash against RULES.lock and runs the synthetic self-test (`backtest.py self-test` stops there). It then writes `results.json`, `daily.csv`, and `trades.csv`, and appends to `RUNLOG.md`. It takes a few seconds.
- `verify.py` replays every trade independently.
- `posthoc.py` writes `posthoc.json` without opening the store.
- `charts.py` writes `report/figures/*.svg`.
- `counts.py` is the pre-lock count script.

Seeds: direction placebo 20260926, bootstrap 20260927. Reruns on the same store give identical numbers.

### References

- Baltussen, G., Da, Z., Lammers, S., & Martens, M. (2021). Hedging demand and market intraday momentum. *Journal of Financial Economics*, 142(1).
- Boyarchenko, N., Larsen, L. C., & Whelan, P. (2023). The overnight drift. *Review of Financial Studies*, 36(9).
- Cheng, M., & Madhavan, A. (2009). The dynamics of leveraged and inverse exchange-traded funds. *Journal of Investment Management*, 7(4).
- Cliff, M., Cooper, M. J., & Gulen, H. (2008). Return differences between trading and non-trading hours: Like night and day. Working paper.
- Faith, C. (2007). *Way of the Turtle.* McGraw-Hill. See also "The Original Turtle Trading Rules" (2003).
- Gao, L., Han, Y., Li, S. Z., & Zhou, G. (2018). Market intraday momentum. *Journal of Financial Economics*, 129(2).
- Lou, D., Polk, C., & Skouras, S. (2019). A tug of war: Overnight versus intraday expected returns. *Journal of Financial Economics*, 134(1).
- Moskowitz, T. J., Ooi, Y. H., & Pedersen, L. H. (2012). Time series momentum. *Journal of Financial Economics*, 104(2).
