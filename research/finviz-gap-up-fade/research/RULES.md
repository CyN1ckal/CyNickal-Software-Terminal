# Opening-gap fade: Finviz replication

Written 2026-09-26, before any return was computed on these names. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

The trading rule is the rule locked in `research/small-cap-gap-up-fade/research/RULES.md`. This study changes the universe only.

## Prior exposure

- **Looks before this file.** `screen.py` read the Finviz free screener, ticker view only. It did not read a performance, gap, or change column. `fetch_bars.py` then downloaded daily bars for the sampled names with `ingest --timeframe 1d --from 20160104 --to 20260925`, which the request to run the backtest required. `counts.py` then counted bars and opening gaps. It did not print an open-to-close return, a P&L, or a hit rate.
  - Small/micro screen: US stocks only, market cap under $2B, price over $5, average volume 100,000 to 1,000,000, Finviz shortable. 795 names. Mid screen: the same filters at $2B to $10B. 429 names.
  - Names already in `small-cap-gap-up-fade` were removed. Every 16th remaining small name is the primary book. Every 14th remaining mid name is the cross book. Alphabetical order, strides fixed before any of these names' returns were read.
  - Dropped after the download, before any return, because they had fewer than 252 daily bars: LMAT (0 bars), MESH (125), NUCL (148), QVCG (34), SPTX (102), IOND (43), LCLN (89). Not replaced.
  - Primary capped gaps (open / previous close − 1 at least 5% and below 100%): 1,419, of which 943 are before 2024-01-02 and 476 are from that date. Cross: 381, of which 301 and 80. Opens at a double or more, excluded by the cap and not by dropping the name: BXC 2016-06-14 (9.34), DEC 2023-12-05 (18.37), GYRE 2016-12-15 (1.07) and 2022-05-23 (1.50), PRPO 2017-01-12 (2.55) and 2017-06-13 (31.00).
- **Earlier study.** `small-cap-gap-up-fade` ran this same rule on 27 even names from a September 2026 Koyfin screen of $87–398 million market cap. It was a paper-trading candidate. Out of sample, 2024-01-02 through 2026-09-25, the book multiplied by 22 after 20 bp (Sharpe 2.60, profit factor 2.35, 165 trades, max drawdown −45%). The full-sample path went from 1 to 39,156 (Sharpe 1.89, max drawdown −81.8%). The same signals entered at the close lost money (out-of-sample Sharpe −1.84). Only 37.7% of trades reached the prior close. The full-sample multiple depended on in-sample SMTI prints, including days under $100,000 of dollar volume. Removing those days left the out-of-sample Sharpe at 2.50. Those names are not in this book.
- **What I already know about the window.** The out-of-sample dates are not unseen as a market. SPY's Sharpe on them was about 1.20 in the earlier studies, and the window includes the April 2025 tariff crash and rebound. These Finviz names' open-to-close returns conditional on a 5% gap have not been read. The price floor of $5 and the shortable flag are responses to that earlier study: the dust prints and the unpriced borrow. They were set before this sample's returns were computed.
- **Where the parameters came from.** The 5% gap, the 100% cap, the 20 bp, the open fill, and the close cover are the locked rule, unchanged. The Finviz filters are the universe the first study could not reach: more shortable names above $5, with average volume in a band that is still not large-cap volume, plus the mid-cap book the store did not have.

## Hypothesis

On this new small-cap sample, a stock whose opening auction clears at least 5% above the previous close, and below a double, has a negative open-to-close return large enough that shorting the auction and covering at the close has an out-of-sample Sharpe of at least 0.5 after 20 bp per side.

**Mechanism.** The same claim as the first study. Impatient buyers pay up at the open of a thinner stock, and the short supplies the auction. Berkman, Koch, Tuttle, and Zhang (2012) and Lou, Polk, and Skouras (2019) are the sources. This sample is the test of whether that result was the 27 Koyfin names or the rule. Novy-Marx and Velikov (2016) are why the spread stays 20 bp.

**Known counter-forces.** The first study's delay book lost money, so a missed auction is fatal. The gap usually did not fill. Borrow is still unpriced, though Finviz currently marks these names shortable, which is a snapshot, not a history. The list is whoever Finviz listed on 2026-09-26. Recent listings that clear 252 bars are mostly an out-of-sample path. Mid caps may be too liquid for the concession. A double-or-larger open is still excluded.

## Predictions beyond P&L

Scored on the primary book, the same way as the first study:

1. The mean of close / open − 1 on signal name-days is lower than on other primary name-days with a usable bar and a positive previous close. Days with a gap at or above 100% are in neither set.
2. Among primary trades, the mean gross short return is higher when the gap is at or above the median gap than when it is below.
3. Among primary trades with a trailing dollar-volume figure, the mean gross short return is higher in the half at or below the median dollar volume. A half under 30 trades is not testable.
4. The mean gross short return on the primary trades is higher than on the control names (QQQ, SPY, IGV, AAPL, AMZN, META, GOOGL, ADBE, AAL) under the same gap rule. Fewer than 30 control trades makes this not testable.
5. Among primary trades with a same-day SPY overnight gap, the mean gross short return is higher when the stock's gap exceeds SPY's by at least 5 percentage points. A half under 30 trades is not testable.
6. The share of primary trades whose low is at or below the previous close is greater than one half.

Gross short return is (entry − exit) / entry. Medians average the two middle values when the count is even. The boundary trade follows the "at or above" or "at or below" wording.

## Data

- **Primary (43).** AARD, AIP, AMRC, APEI, ASYS, BBCP, BJRI, BXC, CCB, CLB, CPF, CTO, DEC, DSP, EPC, FBYD, FMNB, FVCB, GLIBK, GYRE, HRTG, IIIN, IPX, KELYA, KRRO, LZB, MOV, MYE, NLOP, OCSL, ORN, PESI, PRPO, RGR, RTB, SCSC, SKYH, SWIM, TITN, TRST, UMH, VOXR, WMK.
- **Cross, mid cap (29).** AAMI, AGYS, ARLP, BANR, BHF, CACC, CHEF, CSW, DLB, EPR, FELE, GFF, HGTY, HXL, KALU, LTC, MH, MTH, NIC, ORA, PFSI, PRAX, RNST, SITE, SSB, TFSL, TRNO, VC, WK.
- **Controls.** QQQ, SPY, IGV, AAPL, AMZN, META, GOOGL, ADBE, AAL. Not a candidate.
- **Bars.** Daily, 2016-01-04 through 2026-09-25, `agent-data/mdq.py`, split-adjusted. Dividends are not adjusted. A daily close is known at 16:00 ET, or 13:00 ET on an early close. Entry uses the open. The cover uses the close.
- **Calendar.** `mdq.nyse_sessions`. Evaluation sessions are 2016-01-05 through 2026-09-25. 2016-01-04 is warm-up. Early closes are `mdq.EARLY_CLOSES`.
- **Missing bars.** Not forward-filled. The previous NYSE session must have a bar.
- **Usable bar.** Open, high, low, and close positive, with `low <= open <= high` and `low <= close <= high`.
- **History floor.** 252 daily bars. The script stops before writing results if a kept name is short of that or has a nonpositive open or close.
- **Dollar volume for prediction 3 only.** Median of close × volume on the 21 NYSE sessions before the signal, at least 15 observations. Not a filter.

## Primary rule

Unchanged from the first study:

- `GAP_MIN = 0.05`, `GAP_MAX = 1.00`, `COST_BPS = 20`, `COST_MULT = 1`.
- Seeds: direction `20260926`, bootstrap `20260927`, timing `20260928`. Direction 2,000 draws, timing 500, bootstrap 2,000 draws of 20-session blocks.

1. Before the open, a limit to short rests at `prior_close * 1.05`. If the open is below that limit, cancel. If the open is at or above the limit and the ratio `open / prior_close` is strictly below 2, fill at the open. The order does not stay working after the open.
2. One short per name per session. No long, no flip.
3. Cover at that session's close. Reason `session_close`. No stop. High and low are not used for the primary.
4. Equity starts at 1. Each of the `n` names that signal gets notional `E / n`, sized before costs. Gross exposure is 1 on a signal day and 0 otherwise. A flat day returns 0.
5. Entry cost and exit cost are each `COST_MULT * 20 / 10000` of that side's notional. Gross price P&L is `shares * (open - close)`. If equity would be wiped, that day's return is −1, equity stays 0, and no later day opens a trade.
6. Sharpe is the mean daily simple return divided by the sample standard deviation, times √252, zero rate. Flat days are in the series.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Previous close and the limit | Prior official close | The order resting before the open |
| Today's open | Opening auction | Cancel, or fill at that print |
| Today's close | Official close | The cover only |
| Today's low | Official close | Secondary accounting only |
| Trailing dollar volume | Prior close | Prediction 3 only |
| SPY open and previous close | SPY's open | Prediction 5 and the breakdown only |
| Finviz membership, price, average volume, shortable flag | The 2026-09-26 snapshot | The candidate list only |

## Samples

- **Warm-up.** 2016-01-04, and any later session on which a name still has no previous close.
- **Out-of-sample.** Sessions on or after 2024-01-02 through 2026-09-25.
- **In-sample.** 2016-01-05 through 2024-01-01.
- **Why.** Same split as the first study on this rule, so the replication is on the same dates and on different names. The market path of those dates is known. These names' gap results are not. A trade belongs to the sample of its entry date.

## Benchmarks

Uncosted, same sessions, same equity floor. None is an acceptance line.

- **Unconditional intraday short** of the primary names with a usable bar: mean of `(open − close) / open`.
- **Equal weight, close to close,** of primary names with a usable bar and a positive previous close.
- **SPY close to close.** Zero when SPY has no return. Also report the strategy and SPY from the first evaluation session that has a SPY return. That slice is coverage, not a new sample.

## Secondary candidate

**Gap-fill limit.** Same entry, names, size, and 20 bp. Cover at the previous close when the low is at or below it (`gap_fill`), otherwise at the close. Its line, which does not replace a failed primary: out-of-sample Sharpe ≥ 0.5, profit factor ≥ 1.10, and at least 100 out-of-sample trades.

## Reported checks

- Full, in-sample, and out-of-sample metrics at the base cost for the primary, the secondary, the mid-cap book, and the controls. Profit factor uses trade net returns. Average net trade is the mean net return in basis points.
- Breakdowns on the primary: year, exit reason, gap bucket, quintile of the same-day SPY return. Quintile edges use the full sample of sessions that have a SPY return. Not a filter.
- Cost multipliers 0, 0.5, 1, 2, 3.
- Delay book: short the signal close, cover the next session's open, whole net P&L on the exit session. Reported at 20 bp and at zero. Not a candidate.
- Direction placebo, timing placebo, and block bootstrap, as in the first study.
- Plateau: `GAP_MIN` in {0.03, 0.04, 0.05, 0.07, 0.10}, cap fixed at 1. Five cells. Out-of-sample Sharpes are shown and nothing is selected.
- Verification: the synthetic self-test, and `verify.py` on every primary trade.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 20 bp per side:

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 40 bp per side.
5. Out-of-sample Sharpe > 0 on the mid-cap book under the identical rule.
6. At least 100 out-of-sample trades. The pre-lock count is 476, so the floor stays. Below 100 the verdict is **Inconclusive**.

An undefined Sharpe or profit factor fails the line that needs it. The secondary's line does not promote it. The mid-cap book does not replace a failed primary.

## Not done in this study

The report will not promote a grid cell, the gap-fill secondary, the delay book, the mid-cap book, the controls, one name, the other Finviz names that the stride did not draw, gap-downs, a dollar-volume filter, a lower spread, or a borrow fee added after the run. The full Finviz screens (795 and 429 names) are not the tested book.
