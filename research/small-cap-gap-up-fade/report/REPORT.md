# Opening-gap fade: small-cap shorts

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Paper-trading candidate.** Passed all 6 pre-registered tests. Not for live capital. |
| Instruments | 27 small-cap names, short the opening auction, flat by the close. The other 28 names on the same screen are the cross-market book |
| Data | Daily bars 2016-01-04 → 2026-09-25, evaluated 2016-01-05 → 2026-09-25, read via `agent-data/mdq.py`. No 1-minute bars exist for these names. Medium-cap names are not in the store |
| Rules | [`research/small-cap-gap-up-fade/research/RULES.md`](../research/RULES.md), locked 2026-09-26 18:34 UTC, sha256 `632baf194960` |
| Code | [`research/small-cap-gap-up-fade/research/`](../research/) · 1 store run (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Shorting the opening auction after a gap of at least 5%, and covering at the close, made money out of sample on the small-cap screen. From 2 January 2024 through 25 September 2026 the book multiplied by **22.0** after 20 bp per side (Sharpe **2.60**, profit factor 2.35, 165 trades, max drawdown −45.0%). The average out-of-sample trade made **+237 bp** net. Over the full window, 5 January 2016 through 25 September 2026, equity went from 1 to **39,156** (Sharpe **1.89**, max drawdown **−81.8%**). That full-sample multiple is the compound of a book that is often one name at 100% of equity, and it is the wrong number to underwrite. The out-of-sample trade is the result.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2016-01-05 → 2026-09-25 | → 2024-01-01 | 2024-01-02 → |
| Sessions | 2,698 | 2,012 | 686 |
| Total return | **+3,915,504%** | +177,882% | **+2,100%** |
| CAGR | +169% | +155% | +211% |
| Annual volatility | 62.6% | 66.7% | 48.2% |
| Sharpe | **1.89** | 1.74 | **2.60** |
| Max drawdown | −81.8% | −81.8% | −45.0% |
| Trades / profit factor | 782 / 1.82 | 617 / 1.71 | 165 / 2.35 |
| Avg net trade | +177 bp | +161 bp | **+237 bp** |
| *Short every open, Sharpe (max DD)* | *−0.52 (−77.3%)* | *−0.62 (−77.3%)* | *−0.23 (−37.5%)* |
| *Equal-weight screen Sharpe (max DD)* | *0.69 (−43.0%)* | *0.85 (−41.9%)* | *0.16 (−23.8%)* |
| *SPY Sharpe (max DD)* | *0.49 (−25.4%)* | *0.13 (−25.4%)* | *1.20 (−19.9%)* |

It passed all 6 acceptance tests written before the first run (§8).

**Why it is only a paper-trading proposal.** The same evidence carries these risks:

1. **The fill is the opening auction.** A limit rests overnight at 5% above the prior close and, if the auction trades through it, sells at the open. The same signals, entered at that day's close and covered at the next open, lost money: out-of-sample Sharpe **−1.84**, return **−66%**, and the gross trade was about **+1.5 bp**. At zero cost the full-sample delay book still lost 98% (Sharpe −0.40). Selling after the open is a different trade, and that trade failed.
2. **The tested size is one name, fully invested, on most days.** **(post hoc)** 519 of 618 signal days had a single name. The continuous path fell **81.8%** from 25 February 2022 to 9 August 2023, then recovered. Out of sample the restarted drawdown is −45%. Annualised volatility is 48% out of sample.
3. **The full-sample multiple is an in-sample compound, much of it in SMTI.** **(post hoc)** SMTI is 178 of the 617 in-sample trades and 6 of the 165 out-of-sample trades. Its in-sample average gross trade was +535 bp. Summed across trades, SMTI is 57% of the book's gross returns and JILL is another 19%. Zeroing every day that contains a print under $100,000 of dollar volume cuts the full-sample Sharpe from 1.89 to **0.80** and the full-sample multiple from 39,156× to **18×**. The out-of-sample Sharpe stays **2.50**. The dust prints inflate the long compound. They do not produce the out-of-sample pass.
4. **Two years lost money, and the holdout book was nearly wiped before it made its out-of-sample Sharpe.** 2022 returned −61% (Sharpe −1.71). 2023 returned −19% (Sharpe −0.09). The cross-market book, same rules, returned −79% in sample with a −97% drawdown, then Sharpe 3.15 out of sample. Line 5 only required that out-of-sample Sharpe to be positive.
5. **The gap does not fill.** Only 37.7% of trades traded back through the prior close, so prediction 6 is not consistent. The average signal day fell 2.17% from open to close. The concession is partial. Borrow and locates are unpriced, and a gap-up morning is when a locate is hardest. Medium-cap stocks were not tested. The candidate list is a September 2026 snapshot.

**Recommendation.** Paper-trade the locked rule forward of 25 September 2026, on a small account, with the order and the review rules in §10. The number to watch is the average net trade, which was +237 bp out of sample, not the 39,156× compound. Do not sell after the open. Do not put live capital on it.

## 2. The strategy

### Rules

```
Before the open, rest a limit to short at prior_close × 1.05.
If the opening auction prints at or above that limit, and below prior_close × 2:
    sell at the opening print
    cover at that session's close
    size 1/n of equity on each name that filled, n = that day's count
Pay 20 bp of notional on the open and 20 bp on the close.
A gap at or above a double is skipped.
```

- **Why the auction:** the open is the information. A marketable limit that was resting before the auction is filled at the clearing price. A sale sent after the print cannot claim that price, and the delay book shows what that later sale did.
- **Why 5%:** the user's threshold, used as given. The grid varies it. Nothing was selected from the grid.
- **Why these names:** the even tickers, A to Z, from the September 2026 Koyfin screen already used by `low-liq-high-vol-mean-reversion`, after the same four names were dropped. Market caps on that snapshot run from $87 million to $398 million. The odd tickers are the cross-market book. There is no medium-cap cross-section in the store.
- **Why 20 bp:** the spread already locked for this screen in that study, from the cost problem in Novy-Marx and Velikov (2016). No borrow is charged, because the short is covered at the close.

### How it trades

| | |
|---|---|
| Signal days | 618. **(post hoc)** 519 of them are a single name. The busiest day has 12 |
| Trades per year | 73 full sample, 61 out of sample |
| Time in market | 22.9% of sessions. On a signal day the book is in from the open to the close |
| Holding time | One session. Every trade exits the day it opens |
| Long / short | 782 shorts, 0 longs |
| Win rate | 56.5% full sample (average winner +698 bp, average loser −499 bp). 73.9% out of sample |

## 3. Hypothesis and predictions

Retail and attention-driven buyers pay up at the open of a thin stock after a large overnight gap. The short supplies shares into that auction. Berkman, Koch, Tuttle, and Zhang (2012) find positive overnight returns and negative trading-day returns in stocks that are hard to value, costly to arbitrage, and recently in the retail eye. Lou, Polk, and Skouras (2019) find an offsetting reversal between the overnight and intraday components. These names trade about $1–6 million a day on the snapshot, which is the costly end of that claim. The test charges 20 bp a side because a short-horizon reversal can be real gross and still fail net (Novy-Marx and Velikov 2016).

The open-to-close concession is in the data. The stronger claim, that the auction price trades back through the prior close, is not.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Signal days fall more from open to close than other days | Signal-day mean open-to-close −2.17% (782 days). Other days +0.05% (64,805 days) | Consistent |
| Larger gaps revert more | Mean gross short return +3.06% at or above the median gap (7.7%), +1.28% below it. 391 trades each | Consistent |
| Thinner names pay more | Mean gross +3.66% in the half at or below the median trailing dollar volume ($897,101), +0.40% above it. 376 trades each | Consistent |
| Liquid controls pay less | Primary mean gross +2.17% (782 trades). Controls +0.60% (38 trades) | Consistent |
| Idiosyncratic gaps pay more than market-wide gaps | 245 trades with excess gap ≥ 5 percentage points, 19 with a smaller excess. The market half is under 30 | Not testable |
| Price trades back through the prior close | 295 of 782 trades (37.7%) have a low at or below the prior close | Not consistent |

Four predictions match the partial-fade mechanism. The gap-fill prediction does not. A passing P&L with that miss means the book is paid for a partial reversal during the session, and it is not paid for a round trip back to the previous close.

## 4. Method

- **Data.** Daily bars, split-adjusted, dividends not adjusted. The pre-lock count found no dividend rows on the screen names. Every primary and cross name had at least 252 daily bars. No kept bar had a nonpositive open or close, and none had a high or low out of order. A signal requires a bar on the previous NYSE session; 920 primary name-days had a bar today and none yesterday, and those days are not signals. 2021-12-31 is a session. 2025-01-09 is not. QQQ and SPY had no 5% gap in the stored daily history (from about 16 September 2021). The controls together had 38.
- **Pre-registration.** `RULES.md` was locked at sha256 `632baf194960e46b6ff21ffc25f607b8f60bd7e2531b97555c12fdd9b99c1d44` before the run. It was not committed before the run. The out-of-sample window is the one `low-liq-high-vol-mean-reversion` already used on these names. That study lost 56.6% out of sample, and its short leg lost 37.7 bp a trade, on a one-week hold. This open-to-close rule had not been scored. The QQQ window from 1 July 2024 is also known: QQQ rose, and the ETF fades in the earlier studies lost money.
- **Fills and costs.** Base fill is the opening print, for a limit that was resting at +5%. Cost is 20 bp a side of the traded notional. A half-spread of 20 bp on a name that trades $1–6 million a day is the figure the sibling study locked. A gap-day open can be wider. The sweep charges up to 60 bp a side. Borrow is zero in every row.
- **Returns.** Daily simple returns on the NYSE calendar. A session with no position is 0. Sharpe is the mean divided by the sample standard deviation, times √252, with a zero rate. Profit factor is the sum of positive trade net returns divided by the absolute sum of negative ones.
- **Verification.** The self-test covered the 5% boundary, the double cap, a missing prior session, an unusable bar, two-name weights, compounding, ruin, the gap-fill exit, the delay book, and the early-close label. `verify.py` is a separate implementation and matched all 782 primary trades and all 2,698 session returns.
- **Runs.** One store run, reason `initial`, at 2026-09-26 18:44 UTC. The rules hash in `results.json` is the lock hash. No bug fix, and no second set of headlines. The signal count matched the pre-lock count (782 primary, 806 cross, 38 control) before any P&L was written.

## 5. Results

![Growth of $1](figures/equity.svg)

Log scale. The fade compounds from 1 to 39,156. The equal-weight screen ends near 4, SPY near 1.7, and shorting every open loses money. The vertical line is 2 January 2024. Most of the multiple is in sample. Out of sample the same book multiplies by 22.

![Drawdown](figures/drawdown.svg)

The worst drawdown is −81.8%, from the 25 February 2022 peak to the 9 August 2023 trough, which is still in sample. The out-of-sample window then has its own −45% drawdown.

| Strategy (20 bp) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | +3,915,504% / 1.89 / −81.8% | 1.74 | +2,100% / 2.60 / −45.0% |
| Gap-fill limit (secondary) | +4,836,949% / 2.03 / −82.5% | 1.88 | +2,007% / 2.70 / −42.8% |
| *Short every open* | −71.7% / −0.52 / −77.3% | −0.62 | −16.5% / −0.23 / −37.5% |
| *Equal-weight screen* | +316% / 0.69 / −43.0% | 0.85 | +3.4% / 0.16 / −23.8% |
| *SPY* | +73% / 0.49 / −25.4% | 0.13 | +62.5% / 1.20 / −19.9% |

The secondary clears its own out-of-sample line (Sharpe 2.70, profit factor 2.37, 165 trades). It exits at the prior close when the low gets there, which is 37.7% of trades, and at the close otherwise. Its out-of-sample return is a little lower than the primary's. The rules do not promote it over the primary.

SPY's stored daily history starts on 16 September 2021, so the SPY column is a zero before the first close-to-close return on 17 September 2021. From that date through the end, the strategy's own Sharpe on the restarted window is 1.15 and SPY's is 0.71. The equal-weight screen's out-of-sample Sharpe, 0.16, matches the sibling study's screen on this window. The benchmark code is reading the same market.

![Calendar-year return](figures/by_year.svg)

The axis is dominated by 2018 (+1,210%) and 2019 (+887%). Those are in sample. The two losing years are 2022 and 2023. 2026 is a partial year, through 25 September.

| Year | Strategy | Sharpe | Max DD | Trades | Equal-weight screen | SPY |
|---|---:|---:|---:|---:|---:|---:|
| 2016 | +274% | 2.36 | −21.8% | 68 | +61% | — |
| 2017 | +33% | 0.89 | −31.3% | 54 | +26% | — |
| 2018 | +1,210% | 3.34 | −43.3% | 86 | −12% | — |
| 2019 | +887% | 3.12 | −31.8% | 89 | +31% | — |
| 2020 | +302% | 2.22 | −38.3% | 183 | +56% | — |
| 2021 | +121% | 1.71 | −29.3% | 53 | +43% | +6% |
| 2022 | −61% | −1.71 | −65.6% | 38 | −28% | −19% |
| 2023 | −19% | −0.09 | −51.5% | 46 | +6% | +24% |
| 2024 | +155% | 2.16 | −25.3% | 59 | −2% | +23% |
| 2025 | +266% | 2.63 | −45.0% | 79 | +5% | +16% |
| 2026 | +136% | 3.63 | −6.7% | 27 | +0.5% | +13% |

SPY is blank before its history starts. A zero in the stored series for those years is an empty benchmark, not a flat market.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each trade's size and flips the sign of its gross dollar P&L. Two thousand draws, seed 20260926. None of them reached the actual gross Sharpe of 2.22. The null mean is −0.06. p = 0.0005, which is 1/2,001. The timing placebo draws the same number of names at random from the names that could have traded, and shorts them open to close with the same costs. Five hundred draws, seed 20260928. None reached the actual net Sharpe of 1.89. The null mean is −0.83. p = 0.002. Random open-to-close shorts of these names lose money. The gap filter is what the placebos are rejecting.

A costed coin flip would have been a low bar. This null is the gross sign flip, and a second null that keeps the open-to-close short and drops the gap. Both sit below the actual path.

### Bootstrap

Circular block bootstrap of the primary's net daily returns, 20-session blocks, 2,000 draws, seed 20260927. The 95% interval of the full-sample Sharpe is **1.19 to 2.56**. The out-of-sample t-statistic of the mean daily return is 4.30.

### Parameter plateau

![Parameter grid](figures/grid.svg)

All five in-sample cells have Sharpe above 0 (100%, against a 60% bar). The primary, 5%, is the middle cell. In sample the 4% cell is the highest, at 1.81. Out of sample every cell is positive, and the out-of-sample Sharpe falls as the threshold rises, from 3.66 at 3% to 1.15 at 10%. The 3% cell has 540 out-of-sample trades. Nothing was selected from the grid. The out-of-sample panel is there to show the selection bias that picking the in-sample winner would have produced: the 4% cell's out-of-sample Sharpe is 3.06, against the primary's 2.60.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× (10 bp) | **1× (20 bp)** | 2× (40 bp) | 3× (60 bp) |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 2.24 | 2.07 | **1.89** | 1.54 | 1.18 |
| Out-of-sample Sharpe | 2.97 | 2.79 | **2.60** | 2.22 | 1.82 |
| Full-sample return | +43,667,243% | +13,091,627% | **+3,915,504%** | +347,653% | +30,487% |
| Out-of-sample return | +3,650% | +2,773% | **+2,100%** | +1,188% | +652% |
| Average net trade, full sample | +217 bp | +197 bp | **+177 bp** | +138 bp | +98 bp |

The average gross trade is +217 bp, so 40 bp of round-trip cost at the base spread takes a slice and leaves a positive trade. At 60 bp a side the out-of-sample Sharpe is 1.82 and the full-sample return is still positive. The sweep does not cross zero. That is the spread. It is not a locate fee.

The latency test is the delay book. Entering at the signal close and covering at the next open, the full-sample Sharpe is −0.83 and the return is −99.8% (760 trades, average gross −21 bp). Out of sample the Sharpe is −1.84. At zero cost the full-sample delay Sharpe is −0.40. There is no gross edge left once the open has printed.

### Other markets (identical rules)

| Book | IS Sharpe | OOS Sharpe | OOS return | Full return / profit factor | OOS trades |
|---|---:|---:|---:|---|---:|
| Cross-market screen | 0.07 | **3.15** | +17,166% | +3,536% / 1.32 | 268 |
| Liquid controls | 0.25 | 1.07 | +20% | +29% / 1.20 | 12 |

The cross-market book is the acceptance line, and its out-of-sample Sharpe is positive. Its in-sample return is −79%, its in-sample profit factor is 0.92, and its full-sample max drawdown is −97%. The out-of-sample pass sits on top of a book that had already been nearly wiped. The controls are QQQ, SPY, IGV, AAPL, AMZN, META, GOOGL, ADBE, and AAL, from about September 2021. QQQ and SPY contributed no signals. Thirty-eight trades is enough for the mean comparison in prediction 4 and too few for a second verdict. Their average gross trade, +60 bp, is below the primary's +217 bp.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Quintiles of the same-day SPY close-to-close return, from the first session with a SPY return (17 September 2021). Flat strategy days are in the mean. The highest SPY quintile is the negative bar.

| SPY quintile | Sessions | SPY mean day | Strategy mean day | Trades |
|---|---:|---:|---:|---:|
| 1, lowest | 252 | −1.43% | +42 bp | 48 |
| 2 | 252 | −0.36% | +14 bp | 48 |
| 3 | 253 | +0.06% | +29 bp | 43 |
| 4 | 252 | +0.51% | +41 bp | 60 |
| 5, highest | 252 | +1.46% | **−14 bp** | 65 |

The fade loses money, on average, on the market's strongest days. Sessions before SPY's history, 1,437 of them and 518 trades, are not in these quintiles. None of this was used to filter the rule.

By gap, which is also a pre-registered breakdown:

| Gap | Trades | Avg gross | Avg net | Win rate |
|---|---:|---:|---:|---:|
| 5% to 10% | 548 | +163 bp | +124 bp | 58.0% |
| 10% to 20% | 183 | +116 bp | +77 bp | 48.6% |
| 20% to 50% | 45 | +1,031 bp | +993 bp | 64.4% |
| 50% to 100% | 6 | +2,094 bp | +2,058 bp | 100% |

The 10–20% bucket is weaker than the 5–10% bucket. The median split in prediction 2 still goes the predicted way, because the tail above 20% pulls the upper half up. Six trades in the top bucket, all winners, are a thin tail. Every primary exit is `session_close`.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| Out-of-sample Sharpe and profit factor | ≥ 0.5 and ≥ 1.10 | 2.60 and 2.35 | ✅ |
| Direction placebo, full sample | p ≤ 0.05 | 0.0005 | ✅ |
| In-sample Sharpe, and share of grid cells above 0 | > 0, and ≥ 60% | 1.74, and 5 of 5 | ✅ |
| Full-sample return at 40 bp per side | > 0 | +347,653% | ✅ |
| Cross-market out-of-sample Sharpe | > 0 | 3.15 | ✅ |
| Out-of-sample trades | ≥ 100 | 165 | ✅ |

The placebo is on gross returns. A costed coin flip would have been a low bar, and this one is not that test. Line 5 passing does not mean the cross-market book was a stable confirm: its in-sample path drew down 97%. The secondary's own line also passed. It does not replace the primary.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

**(post hoc)** The 252-session Sharpe ranges from −2.21 to +4.55. The last window, ending 25 September 2026, is +3.15. The negative stretch is the 2022–2023 drawdown.

| Risk | Evidence | What the paper account does about it |
|---|---|---|
| The fill has to be the auction | Delay book, out-of-sample Sharpe −1.84. Gross delay trade about +1.5 bp out of sample and −21 bp full sample | If the auction does not fill, the weight stays in cash. The account does not sell later in the day |
| Single-name sizing | 519 of 618 signal days are one name. Max drawdown −81.8% over 18 months. Out-of-sample volatility 48% | The paper account uses the tested weight, 100% split across that day's names, and the account is small in dollars |
| Concentration in a few names and a few prints | **(post hoc)** SMTI is 57% of summed gross returns, almost all in sample. The 10 best days are 23% of log growth. Zeroing them leaves full-sample Sharpe 1.63. Zeroing the 10 best out-of-sample days leaves out-of-sample Sharpe 1.86 | The review watches the average trade, not the compound. A new study would have to drop a name before the lock |
| Dust prints in the long compound | **(post hoc)** 191 trades have same-day dollar volume under $100,000. SMTI on 10 May 2019 opened at 5.00, closed at 2.75, and traded $3,000. Zeroing days under $100,000 leaves full-sample Sharpe 0.80. Out-of-sample Sharpe stays 2.50, because only 3 of those days are out of sample | Log the day's dollar volume. A paper fill on a few-thousand-dollar print is the backtest's fill, and it is not a size you can scale |
| The holdout book | Cross-market in-sample return −79%, max drawdown −97%, then out-of-sample Sharpe 3.15 | Line 5 is recorded as passed. The in-sample wipe is in the report and is not averaged away |
| Borrow | Charged at zero. The sibling study's one-week shorts on these names lost money after a 5% borrow | A refused locate is a skip, logged. The backtest did not drop those names, so the paper log is the record, not a quiet change to the rule |
| The list is a snapshot | Market cap and the candidate list are September 2026. Names that died earlier are absent. Point-in-time caps are not in the store | The paper book is this list. A point-in-time universe is a new study |
| Medium caps | None in the store. The screen tops out at $398 million | The verdict is about this screen |
| Gap does not fill | 37.7% of lows reach the prior close | The exit stays the close. The gap-fill limit is not the recommended order |
| Regime | Loses on the strongest SPY days (quintile 5, −14 bp a day). 2022 and 2023 lost money | A down year is not a stop. See §10 |

## 10. Deployment proposal (paper trading)

Paper only. The account follows the locked rule on the 27 primary names, forward of 25 September 2026.

- **Account.** A small paper account. Each signal day, split 100% of that account equally across the names that fill. Most days in the backtest were one name. The dollar size stays small because several signal days traded only a few thousand dollars and the backtest charges no impact.
- **Order.** Before the open, a limit to short at the previous close times 1.05. If the opening auction prints below the limit, or at or above a double of the previous close, cancel. If it prints through the limit, the fill is the auction price. Cover with a market-on-close order. If the auction does not fill the order, that weight stays in cash. Do not reallocate it to the names that did fill, and do not sell later in the session. The delay test is why.
- **Locate.** If the borrow is refused, skip the name and log it. Do not replace it with another ticker.
- **Early closes.** The cover is the official close, 13:00 ET on the early-close dates in `mdq.EARLY_CLOSES`.
- **What is not a stop.** A losing year is not a stop. 2022 lost 61% and 2023 lost 19% inside a path that still passed the tests. A drawdown as deep as the backtest's −82% is not a stop either: that drawdown ended in August 2023, and the out-of-sample window came after it. A negative six-month stretch is not a stop.
- **What is a stop.** Stop the paper test, and do not quietly change the rule, if the paper account's drawdown from its own peak exceeds 90%, or if the average net trade over the first 100 paper trades is negative. Ninety percent is past the worst drawdown in this sample. A negative average over 100 trades would be outside the out-of-sample experience of +237 bp.
- **Review.** 25 September 2027, or 100 paper trades, whichever comes later. The review reads the average net trade, the share of auction fills actually obtained, the refused locates, and the dollar volume of the days that filled. It does not refit the 5% threshold.
- **Ledger.** Log every order, fill, cancel, and refused locate into a terminal ledger so the paper trades can be compared with this study's `trades.csv` columns: symbol, entry session, entry price, exit price, gap.

There is no live-capital proposal.

## 11. Post hoc (not part of the verdict)

These were computed after the locked run, from `daily.csv`, `trades.csv`, and the same daily bars. None of them changes §8.

- **Single-name days.** 519 of 618 signal days. The maximum on one day is 12.
- **Drawdown dates.** Peak equity 6,229 on 25 February 2022, trough 1,131 on 9 August 2023, depth −81.8%.
- **Best days.** The 10 best sessions, all in sample, are 23% of the full-sample log growth. Zeroing them leaves Sharpe 1.63 and a multiple of 3,562×. Zeroing the 20 best leaves Sharpe 1.39. Zeroing the 10 best out-of-sample days leaves out-of-sample Sharpe 1.86 and a multiple of 6.4×. The out-of-sample pass is not ten days.
- **Dollar volume of the print.** 154 trades under $25,000 of same-day dollar volume, 191 under $100,000, 221 under $250,000. The largest gross trade, SMTI on 10 May 2019, opened at 5.00 and closed at 2.75 on $3,000 of volume. NAGE on 26 February 2021, also in the top ten by gross return, traded about $580 million. The tail is a mix of untradeable prints and real-volume crashes. Zeroing every day that contains a sub-$100,000 print (183 days, 3 of them out of sample) leaves full-sample Sharpe 0.80 and out-of-sample Sharpe 2.50.
- **By symbol.** SMTI's 184 trades have a mean gross return of +5.2%, and they are 57% of the sum of gross trade returns. 178 of those trades are in sample. All 27 names appear at least once out of sample. The busiest out-of-sample names are VFF (16), BOOM (14), NAGE (11), and OSUR (11).
- **Rolling Sharpe.** Minimum −2.21, maximum +4.55, last +3.15.

### Ideas for a new study

Each of these was suggested by this sample. Each needs its own `RULES.md`, and data this study has not used: later sessions, or a universe that was not this September 2026 list.

- The same auction short with a minimum dollar volume on the prior sessions, or a refusal to count a print under a stated size. The full-sample multiple moves when those days are removed. The out-of-sample window mostly does not contain them, so a new test needs a new window or a new list.
- A point-in-time small-cap universe, so the book is not the companies that were still listed and still in the band in September 2026.
- Medium-cap names. This store does not have them.
- A cost that includes a locate, or a rule that skips names that would have been hard to borrow. This study priced the borrow at zero.

## 12. Reproduce

From the repo root:

```bash
python research/small-cap-gap-up-fade/research/backtest.py
```

```bash
python research/small-cap-gap-up-fade/research/verify.py
```

```bash
python research/small-cap-gap-up-fade/research/posthoc.py
```

```bash
python research/small-cap-gap-up-fade/research/charts.py
```

`backtest.py` reruns the self-test, checks the rules hash, and writes `results.json`, `daily.csv`, and `trades.csv`. A second store run appends to `RUNLOG.md`. `verify.py` replays every primary trade. `posthoc.py` writes `posthoc.json`. `charts.py` writes the SVGs in `report/figures/`. Seeds are 20260926 for the direction placebo, 20260927 for the bootstrap, and 20260928 for the timing placebo. On the same store the numbers match.

The pre-lock counts are in `research/counts.py`. They do not compute a return.

### References

- Berkman, H., Koch, P. D., Tuttle, L., and Zhang, Y. J. (2012). Paying attention: Overnight returns and the hidden cost of buying at the open. *Journal of Financial and Quantitative Analysis*, 47(4), 715–741.
- Lou, D., Polk, C., and Skouras, S. (2019). A tug of war: Overnight versus intraday expected returns. *Journal of Financial Economics*, 134(1), 192–213.
- Novy-Marx, R., and Velikov, M. (2016). A taxonomy of anomalies and their trading costs. *Review of Financial Studies*, 29(1), 104–147.
