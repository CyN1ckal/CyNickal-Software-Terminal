# QQQ ATR martingale to breakeven

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Paper-trading candidate.** Passed all 6 pre-registered tests. Not for live capital. |
| Instruments | QQQ, 5-minute bars, held overnight until the round trip covers its cost. SPY is the cross-market test. IGV is the same rule, reported only. |
| Data | 2021-10-07 → 2026-09-25, read via `agent-data/mdq.py`. 2021-12-31 has no bars and is a zero day. 2025-01-09 is a closure and is not in the calendar. |
| Rules | [`research/qqq-atr-martingale/research/RULES.md`](../research/RULES.md), locked 2026-09-26 17:42 UTC, sha256 `704c62b3ec69` |
| Code | [`research/qqq-atr-martingale/research/`](../research/) · 2 store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Doubling a QQQ fade until the position can be closed at a non-negative net made money on the mark-to-market account. Out of sample, from 1 July 2024 through 25 September 2026, it added **+1.05** per 1 of starting capital (Sharpe **1.02**, daily profit factor 1.75). Equity entered that window at 2.00 and left it at 3.05. The restarted out-of-sample drawdown was **−21.8%**. Over the full window equity went from 1 to **3.05** (Sharpe **1.15**, close-to-close drawdown **−12.6%**). QQQ buy-and-hold compounded to +107% over the full window and +55.4% out of sample.

Every one of the 2,266 closed campaigns had a non-negative net. None were still open at the sample end. That is the exit rule, and it held. The two predictions that asked for a ruinous path did not. The full-sample drawdown was −12.6%, against a pre-registered bar of −50%. The deepest peak-to-trough hole was 0.30 of starting capital, and the booked gains were 2.05. On QQQ the martingale scratched its way up. It did not dig the hole the design was built to show.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-07 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,247 | 685 | 562 |
| P&L per 1 of starting capital | **+2.05** | +1.00 | **+1.05** |
| Ending equity | **3.05** | 2.00 | **3.05** |
| CAGR | 25.3% | 29.1% | 20.8% |
| Annualized P&L volatility | 35.9% | 24.4% | 46.3% |
| Sharpe | **1.15** | 1.52 | **1.02** |
| Max drawdown | −12.6% | −10.0% | **−21.8%** |
| Daily profit factor | 1.74 | 1.74 | 1.75 |
| Day win rate | 48.2% | 49.9% | 46.1% |
| *QQQ buy and hold Sharpe (max DD)* | *0.74 (−35.3%)* | *0.56 (−35.3%)* | *0.97 (−23.8%)* |

P&L is the change in equity measured in units of starting capital, not a percent of whatever the account was worth at the start of the window. The out-of-sample drawdown is computed on a path that restarts at 1, so it does not inherit the in-sample peak. The full-path drawdown during the out-of-sample dates was −12.6%. It passed all 6 acceptance tests written before the first run (§8).

**What has to sit next to that pass.**

1. **The shape that was pre-registered did not show up on QQQ.** Drawdown −12.6% versus the −50% bar. Booked gains 2.05 versus a hole of 0.30. Predictions 2 and 3 fail. A passing P&L with those failures has not confirmed the martingale mechanism (§3).
2. **One grid neighbor is a blow-up.** Spacing 0.35 and multiplier 1.5, the same family of rule, reached equity **−414**. In-sample Sharpe there was −1.18. Fourteen of the fifteen cells were positive. The primary was not selected from the grid, and the grid is not a plateau of the same outcome (§6).
3. **The same rule on IGV did go through zero.** IGV's equity bottomed at **−1.73**, with gross notional of 682 times starting capital. Its out-of-sample Sharpe was still +0.42. SPY stayed positive (minimum equity 0.56) with notional of 341 and an out-of-sample Sharpe of 0.33, which is the cross-market line and a t-statistic of 0.49.
4. **The typical trade is a three-bar scratch, and the profit is concentrated.** The median hold is 3 bars. 1,931 of 2,266 campaigns used one unit. **(post hoc)** Zeroing the 20 best sessions turns the full-sample P&L from +2.05 to −0.25. The out-of-sample t-statistic is 1.52.
5. **A close can show 21 times starting capital of QQQ.** That happened on 13 December 2023, with equity still at 1.73. The backtest does not liquidate. A broker that did would be running a different rule.

**Recommendation.** Paper-trade the QQQ rule only, in a simulator that can carry the notional, with the halt rules in §10. Do not trade the 0.35 spacing. Do not treat IGV's recovered path as permission to hold a book through a negative equity mark.

## 2. The strategy

### Rules

```
A = Wilder ATR(14), fixed when the campaign opens
S = that session's 09:30 open

On a 5-minute close, if flat and the close is 0.5 A through S,
    buy or short one unit at the next open.
    The unit is 1/3 of starting capital. It does not resize.

While the round trip, including 1 bp each way, is still a loss:
    if the close is another 0.5 A further and under every fill,
    add the next unit at the next open.
    The next unit is twice the previous one.
    Stop adding at 16 units. Do not sell.

When a close would net at least zero after both sides of the cost,
    sell at the next open only if that open is still a non-negative net.
    Otherwise keep the campaign.

Nothing is closed at the cash session, at a stop, or at the cap.
A campaign still open at the last bar stays open and is marked.
```

- **Why double, and why refuse a losing sale.** That is the martingale. The sister study, `qqq-atr-scale-in`, used equal units, a three-unit cap, a quarter-ATR target, and a flatten at the close. It lost 8.81% out of sample. This study keeps the entry and changes the inventory.
- **Why the base unit stays 1/3 of starting capital.** So the first fill matches the sister study, and a drawdown does not shrink the next double.
- **Why sixteen units.** The last double is past any book a broker would finance, and the arithmetic stays finite. The cap did not bind on QQQ.

### How it trades

| | |
|---|---|
| Closed campaigns | 2,266 over 1,247 sessions, none with a negative net. None left open |
| Sessions in a position at the close | 29.0% |
| Holding time | median 3 bars |
| One-unit campaigns | 1,931, realized +0.636 |
| Campaigns that added | the other rows of §7 |
| Largest close-of-day notional | 21 times starting capital |

Realized P&L sums to the full-sample equity gain, 2.05, because nothing was left open.

## 3. Hypothesis and predictions

The entry is an inventory fade of a half-ATR move off the open. The stake is the doubling martingale: each adverse step multiplies the next order, and the sale waits until the fills and the round trip are covered (Dubins and Savage 1965). In a fair game that policy books a run of small wins and carries a drawdown that is the actual risk. Costs and drift are a house edge. The sister study had already found that equal-unit adds, flattened at the close, lost money, and that the side of those trades was worse than a coin flip.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Every closed campaign nets at least 0 | 2,266 closes, 0 losses | **Consistent** |
| 2. Full-sample drawdown at most −50% | −12.6% | **Not consistent** |
| 3. The peak-to-trough hole exceeds booked gains | Hole 0.30, booked gains 2.05 | **Not consistent** |

The account made money and the ruinous-path predictions failed. The result has not confirmed the mechanism the rule was built to expose. What it has confirmed is the exit arithmetic: the book does not realize a loss, because it is not allowed to.

## 4. Method

- **Data.** QQQ, SPY, and IGV, 27 September 2021 through 25 September 2026, resampled to 5 minutes. The ATR is Wilder's 14-day average, carried from the prior close, including onto 24 and 25 September 2026 for QQQ and IGV, which have minutes and no daily bar. Two buy-and-hold marks use a 5-minute close. QQQ and SPY have no corporate actions stored. IGV's 5-for-1 split on 7 March 2024 is adjusted by `mdq`. Borrow is charged at 0, which favors a short held overnight. 2021-12-31 is on the calendar with no bars.
- **Pre-registration.** `RULES.md` fixed the doubling, the breakeven exit, the 16-unit cap, the constant base unit, the mark-to-market Sharpe, and the acceptance lines before any return from this rule was computed. The coverage and level counts are the sister study's, not a new look. The out-of-sample window had already been used by that study and by the earlier intraday studies. `RULES.md` was hash-locked at 17:42 UTC and was **not committed** before the run.
- **Fills and the mark.** A signal at a bar's close is acted on at the next bar's open. An exit that would be negative at that open is cancelled. Equity is marked at the session's last close. A loss that is opened and closed inside one session does not appear in the drawdown. Sharpe is the mean daily P&L per unit of starting capital, divided by its sample standard deviation, times √252. Dividing by a vanishing mark-to-market equity is not the definition, and on QQQ equity never went below 0.99 anyway.
- **Verification.** The self-test covers a scratch that clears 2 bp, a return to the entry price that does not exit, a doubled second unit, a cancelled exit, an add that becomes an exit, a campaign that never recovers, the 16-unit cap, the last-half-hour block, a missing bucket, an overnight add, and the short side. `verify.py` replayed QQQ without importing the engine and matched all 2,266 campaigns. Every closed net in that replay was non-negative.
- **Runs.** Two. The initial run, then the verify replay. The headlines did not change. No bug fix sat between them.

## 5. Results

![Equity per 1 of starting capital](figures/equity.svg)

The martingale's equity and QQQ buy-and-hold both rise. The martingale ends at 3.05 and buy-and-hold near 2.07. The dashed line is 1 July 2024. The line does not show the notional behind it.

![Drawdown of marked equity](figures/drawdown.svg)

The martingale's close-to-close drawdown bottoms at −12.6%. QQQ's is −35.3%. This is the drawdown of equity marked at the close, after any same-day scratch has already been booked.

![Gross notional at the session close](figures/notional.svg)

The close print reached 21 times starting capital. That session, 13 December 2023, still showed equity of 1.73. The notional is the part a financing desk would see. The equity line above does not look like 21 times QQQ.

| | Full: P&L / Sharpe / max DD | IS Sharpe | OOS: P&L / Sharpe / restarted DD |
|---|---|---:|---|
| **QQQ primary** | +2.05 / 1.15 / −12.6% | 1.52 | +1.05 / 1.02 / −21.8% |
| SPY, same rules | +1.65 / 0.39 / −71.5% | 0.73 | +0.87 / 0.33 / −118% |
| IGV, same rules | +6.35 / 0.39 / −158% | 1.55 | +4.56 / 0.42 / −393% |
| *QQQ buy and hold* | *+107% / 0.74 / −35.3%* | *0.56* | *+55.4% / 0.97 / −23.8%* |

SPY's minimum equity was 0.56 and its largest close notional was 341. IGV's minimum equity was −1.73 and its largest close notional was 682. Their out-of-sample Sharpes are positive. Their paths are the martingale the QQQ equity line does not display. The restarted drawdowns treat each window's P&L as if it began at 1, which is why SPY's out-of-sample figure is worse than −100% even though the account, which entered the window at 1.77, bottomed at 0.56.

![Year P&L](figures/by_year.svg)

| Year | Sessions | Martingale P&L | Sharpe | Restarted DD | QQQ return |
|---|---:|---:|---:|---:|---:|
| 2021 | 60 | +0.07 | 2.76 | −2.0% | +10.6% |
| 2022 | 251 | +0.43 | 1.52 | −10.7% | −33.1% |
| 2023 | 250 | +0.39 | 1.45 | −9.1% | +53.8% |
| 2024 | 252 | +0.32 | 1.39 | −10.2% | +24.8% |
| 2025 | 250 | +0.56 | 0.91 | −25.7% | +20.2% |
| 2026 | 184 | +0.28 | 1.51 | −9.8% | +21.2% |

The martingale's year column is P&L per 1 of starting capital. QQQ's column is that year's compounded price return. 2021 starts on 7 October and 2026 ends on 25 September. Every calendar year in the sample has positive martingale P&L, including 2022.

## 6. Is it real?

### Placebo

![Direction placebo](figures/placebo.svg)

The direction test keeps each campaign's dates and multiplies that campaign's gross P&L by a coin flip. Costs are left out. Two thousand draws (seed 20260926) averaged a gross Sharpe of −0.00. The 95th percentile was 0.22. The actual gross Sharpe, 1.32, was above all 2,000 (p = 0.0005).

The timing test is not an acceptance line. It keeps the side and moves the entry to a random eligible open in the same session, then uses the same adds and the same breakeven exit. Five hundred draws averaged a gross Sharpe of 0.60, with a 95th percentile of 0.89. The actual 1.32 was above all 500 (p = 0.002). A random minute with this side is already a positive book in this sample. The half-ATR rung is a better minute than that random one. It is not the only minute that makes money here.

### Bootstrap

A circular block bootstrap of the full-sample daily P&L (20-session blocks, 2,000 draws, seed 20260928) gives a 95% interval of **0.84 to 1.65** for the Sharpe. None of the draws were at or below zero. The full-sample t-statistic is 2.57. The out-of-sample t-statistic is 1.52.

### Parameter plateau

![Parameter grid](figures/grid.svg)

Spacing runs from 0.35 to 1.00 ATR and the multiplier from 1.5 to 3. Fifteen cells, judged in sample. Fourteen had a positive in-sample Sharpe. The primary, 0.50 by 2, ranked 3rd, with in-sample Sharpe 1.52. The in-sample best was spacing 0.35 and multiplier 3, in-sample Sharpe 1.61, out-of-sample Sharpe 1.32. Nothing was selected.

The cell that failed is spacing 0.35 and multiplier 1.5. Its in-sample Sharpe was −1.18, its out-of-sample Sharpe was −0.98, and its equity reached **−414**. The primary spacing with the milder multiplier 1.5 stayed slightly positive (in-sample Sharpe 0.31, out-of-sample 0.10) and still reached equity **−2.25**. A positive Sharpe in the grid is not the same fact as an account that stays funded.

### Costs and latency

![Sharpe against cost](figures/costs.svg)

| Cost per side | 0 | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 1.30 | 1.24 | **1.15** | 1.07 | 0.50 |
| OOS Sharpe | 1.09 | 1.07 | **1.02** | 0.96 | 0.96 |
| Full-sample P&L | +2.05 | +2.04 | **+2.05** | +1.90 | +2.15 |
| Minimum equity | 0.99 | 0.99 | **0.99** | 0.99 | 0.70 |

The path is resimulated at each cost, because a higher cost moves the breakeven price. P&L is not monotone in the cost. At 3 bp the full-sample Sharpe is 0.50 and the worst equity is 0.70. The out-of-sample Sharpe is still 0.96.

Filling one bar later gives full-sample Sharpe 0.68 and out-of-sample Sharpe 0.94, with full-sample P&L +2.65. Filling at the signal close, the labelled upper bound, gives 1.15 and 1.01. The out-of-sample line does not depend on getting the signal-bar close.

### Other markets

| | IS Sharpe | OOS Sharpe | Full P&L / min equity / max notional |
|---|---:|---:|---|
| QQQ | 1.52 | 1.02 | +2.05 / 0.99 / 21 |
| **SPY** | 0.73 | **0.33** | +1.65 / 0.56 / 341 |
| IGV | 1.55 | 0.42 | +6.35 / −1.73 / 682 |

Daily P&L correlates 0.24 between QQQ and SPY, 0.03 between QQQ and IGV, and −0.12 between SPY and IGV. These are not one position copied three times. IGV is where the account went through zero and later printed a large gain. That path is inside the identical rules and outside the acceptance line.

## 7. Where the result comes from

![Realized P&L by units](figures/units.svg)

| Units | Closed campaigns | Realized P&L |
|---|---:|---:|
| 1 | 1,931 | +0.636 |
| 2 | 215 | +0.344 |
| 3 | 64 | +0.226 |
| 4 | 33 | +0.338 |
| 5 | 16 | +0.155 |
| 6 | 6 | +0.340 |
| 7 | 1 | +0.012 |

**(post hoc)** The 56 campaigns with 4 or more units are 41.2% of realized P&L. The other 85.2% of campaigns are a single unit. The cap at 16 was never reached. The largest campaign had 7 units.

Sessions classified by the side still on at the close:

| At the close | Sessions | P&L | Sharpe |
|---|---:|---:|---:|
| Long | 141 | −0.83 | −5.41 |
| Short | 221 | −1.18 | −3.02 |
| Flat | 885 | +4.06 | +3.45 |

A day that ends flat is where a winning exit gets booked, including an exit at that morning's open. A day that ends long or short is a day the campaign was still on, and those days lose. The split is the inventory sitting into the bell. It was not used to drop a side. Both sides are required, and both are negative in this particular cut because the gains have been labeled flat.

The overnight gross gap, from the prior close to the next open on legs already held, summed to +0.75. Total P&L was +2.05. The gap is not the whole result, and it is not signed as a loss in aggregate.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| OOS Sharpe | ≥ 0.50 | 1.02 | ✅ |
| OOS daily profit factor | ≥ 1.10 | 1.75 | ✅ |
| Direction placebo | p ≤ 0.05 | 0.0005 | ✅ |
| In-sample Sharpe | > 0 | 1.52 | ✅ |
| In-sample grid cells with Sharpe > 0 | ≥ 9 of 15 | 14 of 15 | ✅ |
| Full-sample P&L at 2 bp | > 0 | +1.90 | ✅ |
| SPY out-of-sample Sharpe | > 0 | 0.33 | ✅ |
| OOS sessions with a position or a close | ≥ 100 | 353 | ✅ |

The status is a paper-trading candidate because those eight rows, which are the six lines, all hold. Prediction 2 and prediction 3 do not, and they were never part of this table. SPY's pass is a Sharpe of 0.33 with a t-statistic of 0.49 and a minimum equity of 0.56.

## 9. Risks and weaknesses

![Trailing Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | What follows |
|---|---|---|
| The ruin path was not the QQQ path | Drawdown −12.6% against a −50% prediction. Hole 0.30 against booked gains 2.05 | Do not describe this pass as evidence that doubling is safe |
| A neighbor blows up | Spacing 0.35, multiplier 1.5, equity −414 | Do not move the spacing. The grid was not a menu |
| Financing | Close notional of 21 on QQQ, 341 on SPY, 682 on IGV. IGV equity −1.73 | A forced sale is a different rule, and it realizes the loss this exit refuses |
| Concentration | **(post hoc)** Zeroing the best 20 sessions leaves P&L −0.25 and Sharpe −0.23. Zeroing the best 10 leaves +0.33 and Sharpe 0.29 | Twenty sessions carry the sign of the full sample |
| A thin out-of-sample t-statistic | 1.52 out of sample. The bootstrap of the full sample is 0.84 to 1.65 | Two years are not a substitute for the paper test |
| The mark is the close | Median hold is 3 bars. An intra-day double that scratches before the bell is not in the drawdown | The −12.6% figure is not the worst mark inside a session |
| The window was already used | The sister fade lost here. QQQ rose about 55% out of sample in the earlier studies | The pass is on a window that had been read |

**(post hoc)** Every trailing 252-session Sharpe in this sample was positive. The lowest was 0.36 and the last, through 25 September 2026, was 1.27. From 1 May 2025, 353 sessions, P&L was +0.64 and the Sharpe was 1.13. The trailing-year figure does not remove the dependence on the best days.

## 10. Deployment proposal (paper trading)

Paper-trade QQQ only, at the locked size: base unit one third of the paper account's starting capital, double at each further half-ATR rung, 1 bp assumed each way, no losing exit, no add past 16 units. The orders are known at the 5-minute close and are sent for the next 5-minute open. An exit is cancelled if that open is no longer a non-negative net. Log every fill against this study's trade file for the overlapping dates.

Do not trade SPY or IGV under this proposal. Do not change the spacing or the multiplier. The in-sample best cell is not the rule.

Halt the paper test, and do not restart it under these rules, if any of these happen:

- Gross notional at a session close exceeds 21 times starting capital. That is the largest QQQ close in the sample. Past it, this study has no observation.
- Mark-to-market equity falls 30% from its paper peak. The QQQ backtest's full-path drawdown was 12.6%, and the restarted out-of-sample figure was 21.8%.
- A broker or a simulator limit forces a sale while the net is negative. That sale is the liquidation this backtest refused to model. The paper test is then a different strategy.
- A closed campaign prints a negative net. The rule cannot do that. A negative close means the implementation left the rule.

A quiet month of scratches is what 1,931 of the campaigns look like. It is not a reason to add size. Review the log after six months or 100 closed campaigns, whichever comes first. The out-of-sample t-statistic was 1.52, so the review is not a victory lap if the paper book is ahead.

## 11. Post hoc (not part of the verdict)

Zeroing the best and worst sessions, the trailing 252-session Sharpe, and the window from May 2025 are in §9. The 41.2% of realized P&L from the 56 campaigns with four or more units is in §7. All of it was computed after the run by `posthoc.py`. None of it changes §8.

### Ideas for a new study

The spacing 0.35, multiplier 1.5 cell is a different rule that lost a large multiple of capital on this sample. It needs its own pre-registration and data this study has not used. So does any cap, stop, or liquidation level. Choosing one from the grid now would be choosing it because of these results.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-atr-martingale/research/backtest.py
```

```bash
python research/qqq-atr-martingale/research/verify.py
```

```bash
python research/qqq-atr-martingale/research/posthoc.py
```

```bash
python research/qqq-atr-martingale/research/charts.py
```

`backtest.py` runs the self-test, refuses to open the store if `RULES.md` has changed, and writes `results.json`, `daily.csv`, `trades.csv`, and a `RUNLOG.md` entry. The full run takes under a minute. Seeds are 20260926 for the direction placebo, 20260927 for the timing placebo, and 20260928 for the bootstrap. A rerun on the same store produces the same headlines.

### References

- Dubins, Lester E., and Leonard J. Savage (1965). *How to Gamble If You Must*. McGraw-Hill.
- Gao, Lei, Yufeng Han, Sophia Zhengzi Li, and Guofu Zhou (2018). "Market Intraday Momentum." *Journal of Financial Economics*.
- Hendershott, Terrence, and Mark S. Seasholes (2007). "Market Maker Inventories and Stock Prices." *American Economic Review, Papers and Proceedings*.
- Wilder, J. Welles Jr. (1978). *New Concepts in Technical Trading Systems*. Trend Research.
