# Opening-gap fade: Finviz replication

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Inconclusive.** The account was ruined in sample, so the out-of-sample book has 0 trades. Lines 1 and 4 fail as well. 3 of 6 pre-registered tests passed. |
| Instruments | 43 small-cap names drawn from a Finviz screen, short the opening auction, flat by the close. 29 mid-cap names from the same scan are the cross-market book |
| Data | Daily bars 2016-01-04 → 2026-09-25, evaluated 2016-01-05 → 2026-09-25, read via `agent-data/mdq.py`. No 1-minute bars exist for these names |
| Rules | [`research/finviz-gap-up-fade/research/RULES.md`](../research/RULES.md), locked 2026-09-26 19:13 UTC, sha256 `3f62ec05244d` |
| Code | [`research/finviz-gap-up-fade/research/`](../research/) · 1 store run (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** Out of sample there is nothing to score. The locked book reached equity 0 on 30 July 2020, and the rule opens no later trade. From 2 January 2024 through 25 September 2026 the return series is 686 zeros: return **0%**, Sharpe **undefined**, **0 trades**. The pre-lock count on this universe was 476 out-of-sample signals. None of them were opened. Over the full window equity goes from 1 to **0** (Sharpe **0.80**, t-statistic 2.61, max drawdown **−100%**, 458 trades, all in sample). The t-statistic is positive because the mean session was positive. One session is recorded as −100%, and that session ends the compound.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2016-01-05 → 2026-09-25 | → 2024-01-01 | 2024-01-02 → |
| Sessions | 2,698 | 2,012 | 686 |
| Total return | **−100%** | −100% | **0%** |
| CAGR | — | — | 0% |
| Annual volatility | 65.2% | 75.5% | 0% |
| Sharpe | **0.80** | 0.92 | **—** |
| Max drawdown | −100% | −100% | 0% |
| Trades / profit factor | 458 / 1.17 | 458 / 1.17 | 0 / — |
| Avg net trade | +60 bp | +60 bp | — |
| *Short every open, Sharpe (max DD)* | *−0.07 (−67.2%)* | *0.38 (−35.9%)* | *−1.34 (−61.5%)* |
| *Equal-weight screen Sharpe (max DD)* | *0.76 (−49.9%)* | *0.76 (−49.9%)* | *1.05 (−25.6%)* |
| *SPY Sharpe (max DD)* | *0.49 (−25.4%)* | *0.13 (−25.4%)* | *1.20 (−19.9%)* |

The out-of-sample 0% and the 0% drawdown are an empty window. Continuous equity was already 0 on the first out-of-sample session. CAGR is blank where the compound is zero and the script leaves the rate undefined.

It failed the minimum-sample test written before the first run, and two other lines with it (§8). The verdict is Inconclusive.

**Why the book ends at zero.**

1. **One half-weight short lost more than the account.** On 30 July 2020 the book was short PRPO and MYE, half of equity each, equity 47.98. PRPO had gapped 11.4% (prior close 24.60, open 27.40) and closed at 140.00. The trade's net return is −412.2%, and its net loss is 98.88, which is larger than the equity the day started with. MYE's net result the same day is +0.76. The locked ruin rule records the session as −100%, sets equity to 0, and opens nothing after that. The day's bars are a real print (§4).
2. **The spread is not what wiped it.** At zero cost the full-sample return is still −100% (Sharpe 0.98). At 40 bp per side it is −100%. Every cell of the gap grid also ends in sample at −100%, with 0 out-of-sample trades. Line 4 fails. Line 1 fails because the out-of-sample Sharpe and profit factor are undefined.
3. **The trades that did open had a positive average.** 458 trades, win rate 59.8%, profit factor 1.17, average net +60 bp, in-sample Sharpe 0.92. The direction placebo p is 0.018. All five in-sample grid cells have Sharpe above 0. Those three lines pass. They describe the path up to the session that ended it. They do not fill the 476 unopened out-of-sample signals.
4. **The mid-cap book is a separate check, and its out-of-sample Sharpe is 1.99.** That passes line 5. The mid-cap full-sample average trade is −59 bp and its profit factor is 0.79. Eighty out-of-sample mid-cap trades sit under the primary's 100-trade floor. The acceptance table keeps the verdict on the primary.
5. **The auction is still the only fill with a gross edge, and the gap still does not fill.** The delay book, short the close and cover the next open, has an average gross trade of −364 bp and also ends at −100%, on 91 trades. 198 of 458 primary trades (43.2%) traded back through the prior close, so prediction 6 is not consistent.

**Recommendation.** Do not trade this book. There is no paper account. The rules leave the grid, the gap-fill exit, the delay book, the mid-cap book, the liquid controls, any one name, and the Finviz names the stride did not draw where they are: reported, and off the verdict.

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
If the day's loss would wipe equity, record −100%, leave equity at 0,
and open no later trade.
```

- **Why the auction:** the open is the information. A limit resting before the auction is filled at the clearing price. The same rule, entered at the close, lost money gross on this sample.
- **Why 5%:** the threshold locked in `small-cap-gap-up-fade`, used unchanged. The grid varies it. Nothing was selected from the grid.
- **Why these names:** a Finviz free screen on 26 September 2026, then a fixed alphabetical stride. The construction is in §4. The rule itself is the first study's rule.
- **Why 20 bp:** the same spread as the first study. No borrow is charged. The Finviz shortable flag is a snapshot, not a locate history.

### How it trades

| | |
|---|---|
| Signals counted before the run | 1,419 primary. 458 of them became trades. The rest fall after equity hit 0 |
| Days with an opened trade | 322. **(post hoc)** 254 of them are a single name. The busiest opened day has 15 |
| Trades per year | 43 full sample, 57 in sample, 0 out of sample |
| Time in market | 11.9% of all sessions, 16.0% of in-sample sessions. After 30 July 2020 every session is flat |
| Holding time | One session. Every opened trade exits the day it opens |
| Long / short | 458 shorts, 0 longs. Every exit reason is `session_close` |
| Win rate | 59.8% (average winner +690 bp, average loser −878 bp) |

## 3. Hypothesis and predictions

The claim under test is the first study's claim, on names that study did not use. Impatient buyers pay up at the open of a thinner stock after a large overnight gap, and the short supplies that auction. Berkman, Koch, Tuttle, and Zhang (2012) and Lou, Polk, and Skouras (2019) are the sources. The spread stays 20 bp because a short-horizon reversal can be real gross and still fail net (Novy-Marx and Velikov 2016).

The open-to-close concession is in the 1,419 signal days. The book that tries to harvest it, sized at 100% of equity, does not survive to the out-of-sample window.

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| Signal days fall more from open to close than other days | Signal-day mean open-to-close −2.15% (1,419 days). Other days +0.045% (92,691 days) | Consistent |
| Larger gaps revert more | Mean gross short return +1.09% at or above the median gap (7.25%), +0.91% below it. 229 trades each | Consistent |
| Thinner names pay more | Mean gross +4.36% in the half at or below the median trailing dollar volume ($360,987), −2.39% above it. 227 trades each | Consistent |
| Liquid controls pay less | Primary mean gross +1.00% (458 trades). Controls +0.60% (38 trades) | Consistent |
| Idiosyncratic gaps pay more than market-wide gaps | 0 opened trades have a same-day SPY gap. SPY's stored bars start on 16 September 2021, after the account was already at 0 | Not testable |
| Price trades back through the prior close | 198 of 458 trades (43.2%) have a low at or below the prior close | Not consistent |

Prediction 2 is consistent on the median split, and the difference is 0.18 percentage points. The gap buckets in §7 show a negative average in the 10–20% bucket and in the three trades above 50%. Prediction 3 is the large split. Four predictions match a partial fade. The gap-fill prediction does not, and the account that trades the fade is gone before the holdout.

## 4. Method

- **Universe.** Finviz free screener, ticker view (`v=411`), fetched by `screen.py` with no login and no performance, gap, or change column. Small screen: US stocks only, market cap under $2 billion, price over $5, average volume 100,000 to 1,000,000, Finviz shortable. 795 names, 768 still eligible after the 55 names already used in `small-cap-gap-up-fade` were removed and symbols outside one to five letters were dropped. Mid screen: the same filters at $2–10 billion. 429 names, all eligible. Alphabetical order. Every 16th remaining small name and every 14th remaining mid name. That draw was 48 and 31. `fetch_bars.py` then downloaded daily bars for those 79 symbols, one symbol at a time, `ingest --timeframe 1d --from 20160104 --to 20260925`. All 79 returned. `counts.py` dropped names under 252 daily bars before any return: LMAT (0 bars), MESH (125), NUCL (148), QVCG (34), SPTX (102), IOND (43), LCLN (89). They were not replaced. The tested book is the 43 and the 29 below. The other names on the two screens were not downloaded and were not tested.
- **Primary (43).** AARD, AIP, AMRC, APEI, ASYS, BBCP, BJRI, BXC, CCB, CLB, CPF, CTO, DEC, DSP, EPC, FBYD, FMNB, FVCB, GLIBK, GYRE, HRTG, IIIN, IPX, KELYA, KRRO, LZB, MOV, MYE, NLOP, OCSL, ORN, PESI, PRPO, RGR, RTB, SCSC, SKYH, SWIM, TITN, TRST, UMH, VOXR, WMK.
- **Cross, mid cap (29).** AAMI, AGYS, ARLP, BANR, BHF, CACC, CHEF, CSW, DLB, EPR, FELE, GFF, HGTY, HXL, KALU, LTC, MH, MTH, NIC, ORA, PFSI, PRAX, RNST, SITE, SSB, TFSL, TRNO, VC, WK.
- **Data.** Daily bars, split-adjusted, dividends not adjusted. The pre-lock count of capped gaps (open / previous close − 1 at least 5% and below 100%) was 1,419 primary (943 in sample, 476 out of sample) and 381 cross (301 and 80). Opens at a double or more were excluded and the names were kept: BXC 2016-06-14 (9.34), DEC 2023-12-05 (18.37), GYRE 2016-12-15 (1.07) and 2022-05-23 (1.50), PRPO 2017-01-12 (2.55) and 2017-06-13 (31.00). A signal needs a bar on the previous NYSE session. 2021-12-31 is a session. 2025-01-09 is not. The controls are QQQ, SPY, IGV, AAPL, AMZN, META, GOOGL, ADBE, and AAL, and they produced the same 38 signals as in the first study.
- **The 30 July 2020 print.** Adjusted PRPO bar: open 27.40, high 160, low 25.40, close 140, volume 4,860,145. Raw bar the same session: open 1.37, high 8, low 1.27, close 7, volume 97,202,896. PRPO's splits in the store are 29 April 2019, ratio 0.0666667, and 22 September 2023, ratio 0.05. The 2020 bar is adjusted only by the later split. Twenty times the raw open is the adjusted open, and twenty times the raw close is the adjusted close. Raw open times raw volume and adjusted open times adjusted volume both come to about $133.2 million. The wipe is a real rally on real volume.
- **Pre-registration.** `RULES.md` was locked at sha256 `3f62ec05244db1d0ee201754e52a36006b13b648870e0b8e7fe074a344a22a8a` before any return on these names. It was not committed before the run. `results.json` carries the same hash. The out-of-sample dates are the first study's dates. That study, on a different 27 names, was a paper-trading candidate: out-of-sample Sharpe 2.60, a 22× multiple, max drawdown −45%, and a full-sample path from 1 to 39,156 that leaned on in-sample SMTI prints. Its delay book lost money (out-of-sample Sharpe −1.84). Those names are excluded here. The market path of 2024–2026 was already known, including the April 2025 tariff crash and rebound and an SPY out-of-sample Sharpe of about 1.20. These Finviz names' open-to-close returns conditional on a 5% gap had not been read. The $5 price floor and the shortable flag were set before this sample's returns, because of that earlier study's dust prints and unpriced borrow.
- **Fills and costs.** Base fill is the opening print, for a limit that was resting at +5%. Cost is 20 bp a side of the traded notional. Borrow is zero in every row.
- **Returns.** Daily simple returns on the NYSE calendar. A session with no position is 0. Sharpe is the mean divided by the sample standard deviation, times √252, with a zero rate. Profit factor is the sum of positive trade net returns divided by the absolute sum of negative ones. A day that would take equity through zero is recorded as −1, and later days stay flat.
- **Verification.** The self-test, run on its own in this write-up and also at the start of the store run, covered 13 paths and passed. `verify.py` is a separate implementation and matched all 458 primary trades and all 2,698 session returns.
- **Runs.** One store run, reason `initial`, at 2026-09-26 19:13 UTC. Git HEAD `73b746db`, tree dirty. No second run, and no bug-fix rerun. The paid ingest ran once, before the lock's return computation, for the 79 sampled symbols. It was not run again. Nothing else was written to `data/`.

## 5. Results

![Growth of $1](figures/equity.svg)

Linear scale, because the fade hits zero and a log axis cannot show that. The book peaks at 49.03 on 18 June 2020 **(post hoc)** and is 0 from 30 July 2020 onward. The equal-weight screen of the same names ends at 17.9. SPY ends near 1.7. Shorting every open loses money. The vertical line is 2 January 2024. By then the fade has been at zero for more than three years.

![Drawdown](figures/drawdown.svg)

The fade's drawdown reaches −100% on 30 July 2020 and stays there. The equal-weight screen's worst drawdown is −49.9%.

| Strategy (20 bp) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | −100% / 0.80 / −100% | 0.92 | 0% / — / 0% |
| Gap-fill limit (secondary) | −100% / 0.84 / −100% | 0.97 | 0% / — / 0% |
| *Short every open* | −31.6% / −0.07 / −67.2% | 0.38 | −56.0% / −1.34 / −61.5% |
| *Equal-weight screen* | +1,694% / 0.76 / −49.9% | 0.76 | +77.8% / 1.05 / −25.6% |
| *SPY* | +72.7% / 0.49 / −25.4% | 0.13 | +62.5% / 1.20 / −19.9% |

The secondary uses the same entries and covers at the prior close when the low gets there. It also opens 458 trades, also ends at 0, and fails its own line (out-of-sample Sharpe undefined, 0 out-of-sample trades). Its average net trade is +84 bp. The rules do not promote it.

The equal-weight screen's out-of-sample Sharpe is 1.05. Holding these names close to close made money across the window in which the fade had no capital left. SPY's stored daily history starts on 16 September 2021. From 17 September 2021 through the end, the strategy's restarted Sharpe is undefined and its return is 0%, because equity was already 0. SPY's Sharpe on that overlap is 0.71 and its return is +72.7%.

![Calendar-year return](figures/by_year.svg)

The axis is dominated by 2016 (+1,536%). 2020 is a total loss. Every later strategy bar is zero because the account was already at zero. 2026 is a partial year, through 25 September. The equal-weight screen is positive in most years, including the years the fade could no longer trade.

| Year | Strategy | Sharpe | Max DD | Trades | Equal-weight screen | SPY |
|---|---:|---:|---:|---:|---:|---:|
| 2016 | +1,536% | 4.13 | −17.4% | 90 | +57% | — |
| 2017 | −76.4% | 0.16 | −89.5% | 63 | +156% | — |
| 2018 | +281% | 2.17 | −64.6% | 78 | −9.6% | — |
| 2019 | +77.5% | 1.18 | −40.2% | 74 | +16.9% | — |
| 2020 | −100% | −0.24 | −100% | 153 | +33.6% | — |
| 2021 | 0% | — | 0% | 0 | +17.8% | +6.2% |
| 2022 | 0% | — | 0% | 0 | −23.2% | −19.5% |
| 2023 | 0% | — | 0% | 0 | +96.7% | +24.3% |
| 2024 | 0% | — | 0% | 0 | +24.8% | +23.4% |
| 2025 | 0% | — | 0% | 0 | +19.5% | +16.4% |
| 2026 | 0% | — | 0% | 0 | +19.3% | +13.1% |

Two earlier sessions did most of the damage before the terminal day. On 28 March 2017, GYRE was the only signal, weight 1, gap 55.5%, entry 120.15, exit 225.15, net return −88.0%. The session return is −88.0%. On 12 March 2018, BXC was the only signal, weight 1, gap 17.2%, entry 18.50, exit 28.03, net return −52.0%. The account was still alive after both. It was not alive after PRPO.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

The direction placebo keeps each opened trade's size and flips the sign of its gross dollar P&L. Two thousand draws, seed 20260926. The actual gross Sharpe is 0.66. The null mean is −0.05. p = 0.018. The timing placebo draws the same number of names at random from the names that could have traded and shorts them open to close with the same costs. Five hundred draws, seed 20260928. The actual net Sharpe is 0.80. The null mean is −1.04. p = 0.002.

Both tests are on the 458 trades the book managed to open. They say those trades were not a coin flip. They stop where the account stopped. A path with a positive average trade can still end at zero when one trade loses more than the equity.

### Bootstrap

Circular block bootstrap of the primary's net daily returns, 20-session blocks, 2,000 draws, seed 20260927. The 95% interval of the full-sample Sharpe is **0.15 to 1.69**. The interval sits above zero. The full-sample compound is −100%. Both figures are in the output. The Sharpe interval is about the average day. The compound is about 30 July 2020. The out-of-sample t-statistic is undefined.

### Parameter plateau

![Parameter grid](figures/grid.svg)

All five in-sample cells have Sharpe above 0 (100%, against a 60% bar). The primary, 5%, is the highest in-sample cell, at 0.92. Every cell's in-sample return is −100%, and every cell has 0 out-of-sample trades. There is no out-of-sample curve to draw. Nothing was selected from the grid. Moving the threshold to 3%, 4%, 7%, or 10% does not produce an account that reaches 2024.

| Minimum gap | IS Sharpe | IS trades | IS return | OOS trades |
|---|---:|---:|---:|---:|
| 3% | 0.71 | 1,052 | −100% | 0 |
| 4% | 0.69 | 704 | −100% | 0 |
| **5%** | **0.92** | **458** | **−100%** | **0** |
| 7% | 0.73 | 240 | −100% | 0 |
| 10% | 0.57 | 124 | −100% | 0 |

### Costs and latency

![Cost sensitivity](figures/costs.svg)

The out-of-sample Sharpe is missing at every multiple, so the figure has one line.

| Cost per side | 0 | 0.5× (10 bp) | **1× (20 bp)** | 2× (40 bp) | 3× (60 bp) |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 0.98 | 0.89 | **0.80** | 0.62 | 0.44 |
| Out-of-sample Sharpe | — | — | **—** | — | — |
| Full-sample return | −100% | −100% | **−100%** | −100% | −100% |
| Average net trade, full sample | +100 bp | +80 bp | **+60 bp** | +20 bp | −20 bp |

At 60 bp a side the average opened trade is negative, and the compound is still −100%. At zero cost the compound is also −100%. The average gross trade on the opened book is +100 bp. The session that ends the account loses several times that.

The delay book enters at the signal close and covers at the next open. At 20 bp, 91 trades, full-sample Sharpe −0.52, return −100%, average gross −364 bp, profit factor 0.20. At zero cost the Sharpe is −0.44, the return is −100%, and the average gross is the same −364 bp. The delay book runs out of equity sooner, which is why it has 91 trades rather than 458. There is no gross edge in selling after the open on the trades it did take.

### Other markets (identical rules)

| Book | IS Sharpe | OOS Sharpe | OOS return | Full return / profit factor | OOS trades |
|---|---:|---:|---:|---|---:|
| Mid-cap screen | −0.60 | **1.99** | +484% | +23.2% / 0.79 | 80 |
| Liquid controls | 0.25 | 1.07 | +20.1% | +28.7% / 1.20 | 12 |

The mid-cap book did not ruin. Its in-sample return is −78.9%, its in-sample profit factor is 0.56, its average in-sample net trade is −140 bp, and its full-sample max drawdown is −84.0%. Out of sample it returned +484% (equity 5.84), Sharpe 1.99, profit factor 2.82, max drawdown −16.8%, average net trade +246 bp, 80 trades, win rate 60%. Line 5 required that out-of-sample Sharpe to be positive. It is. The full-sample average mid-cap trade is −59 bp gross of the sign the fade wants: average gross −19 bp, win rate 45.1%, 381 trades. Eighty trades is the whole out-of-sample mid-cap sample. The primary floor was 100, and this line did not use that floor. The mid-cap result stays a check.

The controls are the same 38 gaps as in the first study. Twelve of them are out of sample. Their average gross trade, +60 bp, is below the primary's +100 bp. Thirty-eight trades is enough for prediction 4 and too few for a second verdict.

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

Quintiles of the same-day SPY close-to-close return, from the first session with a SPY return (17 September 2021). Every strategy bar is zero.

| SPY quintile | Sessions | SPY mean day | Strategy mean day | Trades |
|---|---:|---:|---:|---:|
| 1, lowest | 252 | −1.43% | 0 | 0 |
| 2 | 252 | −0.36% | 0 | 0 |
| 3 | 253 | +0.06% | 0 | 0 |
| 4 | 252 | +0.51% | 0 | 0 |
| 5, highest | 252 | +1.46% | 0 | 0 |

All 458 trades, and 1,437 sessions, fall before SPY's stored history. The quintile split was pre-registered and it is empty, because the account died first. None of this was used to filter the rule.

By gap:

| Gap | Trades | Avg gross | Avg net | Win rate |
|---|---:|---:|---:|---:|
| 5% to 10% | 334 | +113 bp | +73 bp | 56.0% |
| 10% to 20% | 101 | −39 bp | −79 bp | 69.3% |
| 20% to 50% | 20 | +779 bp | +741 bp | 75.0% |
| 50% to 100% | 3 | −1,186 bp | −1,229 bp | 66.7% |

The 10–20% bucket wins often and loses money on average. The three trades at 50–100% are a large average loss. The median split in prediction 2 still goes the predicted way, by 0.18 percentage points. Every primary exit is `session_close`.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| Out-of-sample Sharpe and profit factor | ≥ 0.5 and ≥ 1.10 | undefined and undefined | ❌ |
| Direction placebo, full sample | p ≤ 0.05 | 0.018 | ✅ |
| In-sample Sharpe, and share of grid cells above 0 | > 0, and ≥ 60% | 0.92, and 5 of 5 | ✅ |
| Full-sample return at 40 bp per side | > 0 | −100% | ❌ |
| Mid-cap out-of-sample Sharpe | > 0 | 1.99 | ✅ |
| Out-of-sample trades | ≥ 100, else Inconclusive | 0 | ❌ |

The status is Inconclusive because the pre-registered sample floor was missed. Lines 1 and 4 fail on the same fact: after 30 July 2020 there is no account left, so the out-of-sample Sharpe does not exist and the 2×-cost compound is a total loss. A passed placebo says the opened trades beat a sign flip. It does not put a trade into 2024. The secondary's own line fails with the primary. The mid-cap line passes and stays a check.

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

**(post hoc)** The 252-session Sharpe ranges from −1.00 to +5.03 on the 1,152 windows where it is defined. The last window is undefined. Of 2,447 windows, the rest are trailing years of zeros after the account hit zero.

| Risk | Evidence |
|---|---|
| One name can lose more than the account | PRPO, 30 July 2020, weight 0.50, net return −412%, net loss 98.88 against equity of 47.98. The session is recorded as −100%. Equity stays 0 through 25 September 2026 |
| The out-of-sample test was never reached | 476 pre-counted out-of-sample signals, 0 opened. Sharpe and profit factor undefined. Lines 1, 4, and 6 fail |
| The positive average sits next to a total loss | 458 trades, average net +60 bp, profit factor 1.17, full-sample Sharpe 0.80, full-sample return −100%. Bootstrap Sharpe interval 0.15 to 1.69 |
| Single-name sizing | **(post hoc)** 254 of 322 opened days are one name. GYRE on 28 March 2017 was weight 1 and returned −88% |
| Concentration in PRPO | **(post hoc)** PRPO is 134 of 458 trades, mean gross +2.8%, and 82% of the sum of gross trade returns. That sum includes the −411% gross trade. The same name ends the account |
| Gross edge is the auction | Delay book, average gross −364 bp, return −100% at zero cost and at 20 bp |
| Gap does not fill | 43.2% of lows reach the prior close. The secondary also ends at 0 |
| Borrow | Charged at zero. The shortable flag is the 26 September 2026 Finviz snapshot |
| The list is a snapshot | Membership, price, average volume, and the shortable flag are that day's screen. Names that had already left the filters are absent. The stride, not the full 795 and 429, is the tested book |
| The holdout book | Mid-cap in-sample return −78.9%, max drawdown −84%, then out-of-sample Sharpe 1.99 on 80 trades. Full-sample profit factor 0.79 |
| Dust prints | **(post hoc)** 111 opened trades have same-day dollar volume under $100,000. Zeroing those 105 days leaves the full-sample return at −100% and the Sharpe at 0.18. The PRPO session traded about $133 million. Removing the thin prints leaves the wipe in the path |

## 10. No deployment

There is no paper-trading proposal. The primary missed the sample floor written to decide that question, and the account the rule specifies was at zero years before the out-of-sample window opened.

A different size, a different universe, or later data is a new study with its own `RULES.md`. This report does not change the 5% threshold, the 20 bp, the equal weight, or the ruin rule.

## 11. Post hoc (not part of the verdict)

These were computed after the locked run, from `daily.csv`, `trades.csv`, and the same daily bars. None of them changes §8.

- **Drawdown dates.** Peak equity 49.03 on 18 June 2020, trough 0 on 30 July 2020, depth −100%.
- **Single-name days.** 254 of 322 days with an opened trade. The maximum on one opened day is 15.
- **Best days.** The 10 best sessions are all in sample: 18 March 2016, 20 July 2016, 6 December 2016, 9 December 2016, 13 January 2017, 14 February 2017, 29 March 2017, 16 October 2018, 7 December 2018, 27 September 2019. Their share of log growth is blank, because a session at −100% makes log growth undefined. Zeroing the 10 best days leaves Sharpe 0.46 and a return of −100%. Zeroing the 20 best leaves Sharpe 0.23 and a return of −100%.
- **Dollar volume of the print.** 45 trades under $25,000 of same-day dollar volume, 111 under $100,000, 153 under $250,000, none missing. Zeroing days under $100,000 (105 days, none out of sample) leaves Sharpe 0.18 and return −100%. The largest gross trade, GYRE on 29 March 2017, made +41.0% gross on about $77.3 million of dollar volume. The terminal loss is also a high-volume print.
- **By symbol.** PRPO's 134 trades are 82% of the sum of gross trade returns, with a mean gross of +2.8%. BBCP (27 trades) and PESI (31) are the next two by that sum. The −411% PRPO trade is inside the PRPO mean.
- **Rolling Sharpe.** Minimum −1.00, maximum +5.03, last window undefined. 1,152 of 2,447 windows are finite.

### Ideas for a new study

Each of these was suggested by this sample. Each needs its own `RULES.md`, and data this study has not used: sessions after 25 September 2026, or names whose open-to-close gap results were not read here. Scoring any of them on these 43 names, or on these 29 mid-cap names, would reuse a path this study has already seen.

- A cap on the weight of one name, or a rule that skips the day when a single gap would take more than a stated fraction of equity. The locked equal weight is what let one half-weight short take the account through zero. The cap would be a parameter found on this sample. It has to be locked before it sees a fresh book.
- The rest of the Finviz screens. 720 eligible small names and 398 mid names were outside the stride and were not downloaded. They are a different universe. They are not a repair of this one.
- A point-in-time membership list, so the book is not whoever still passed the Finviz filters on 26 September 2026.
- A cost that includes a locate. This study priced the borrow at zero.

## 12. Reproduce

From the repo root:

```bash
python research/finviz-gap-up-fade/research/backtest.py
```

```bash
python research/finviz-gap-up-fade/research/verify.py
```

```bash
python research/finviz-gap-up-fade/research/posthoc.py
```

```bash
python research/finviz-gap-up-fade/research/charts.py
```

`backtest.py` with no argument reruns the self-test, checks the rules hash, and writes `results.json`, `daily.csv`, and `trades.csv`. A second store run appends to `RUNLOG.md`. `backtest.py self-test` stops before the store. `verify.py` replays every primary trade. `posthoc.py` writes `posthoc.json`. `charts.py` writes the SVGs in `report/figures/`. Seeds are 20260926 for the direction placebo, 20260927 for the bootstrap, and 20260928 for the timing placebo. On the same store the numbers match.

`screen.py` rebuilds the Finviz lists. `counts.py` recounts bars and gaps and does not compute a return. `fetch_bars.py` calls the paid ingest for the sampled symbols. The download for this study already ran. Rerunning it spends the API again.

### References

- Berkman, H., Koch, P. D., Tuttle, L., and Zhang, Y. J. (2012). Paying attention: Overnight returns and the hidden cost of buying at the open. *Journal of Financial and Quantitative Analysis*, 47(4), 715–741.
- Lou, D., Polk, C., and Skouras, S. (2019). A tug of war: Overnight versus intraday expected returns. *Journal of Financial Economics*, 134(1), 192–213.
- Novy-Marx, R., and Velikov, M. (2016). A taxonomy of anomalies and their trading costs. *Review of Financial Studies*, 29(1), 104–147.
