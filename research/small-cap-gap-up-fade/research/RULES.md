# Opening-gap fade: small-cap shorts

Written 2026-09-26, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `research/small-cap-gap-up-fade/research/counts.py` listed instruments and counted opening gaps. It did not print a close, an open-to-close return, a P&L, or a hit rate.
  - The store's equities are the September 2026 Koyfin screen used by `low-liq-high-vol-mean-reversion`, plus AAPL, AMZN, META, GOOGL, ADBE, and AAL. `$SPX` has no bars. QQQ, SPY, and IGV are the ETFs. No small-cap name has a 1-minute bar. ADBE and GOOGL have 19 sessions of 1-minute bars; the other controls have none. A gap fade cannot be tested inside the session. The only prices are the daily open and the daily close.
  - Screen market caps, from `screener.csv`, in the file's units of millions: minimum 86.52, median 281.90, maximum 398.45. Thirty-four names are under 300, twenty-five are from 300 to under 2,000, and none are at or above 2,000. Volume notional on that snapshot runs from about $1.02 million to $6.31 million. Price-change columns were not read. There is no medium-cap cross-section in the store. Medium cap, taken as roughly $2–10 billion, is not tested.
  - NYSE sessions from 2016-01-04 through 2026-09-25: 2,699. HLS, TCS, and CVO are not in the store. PARK has 204 daily bars and is not used.
  - Primary names, gap = today's open / previous session's close − 1, previous session meaning the previous NYSE session: 784 gaps of at least 5% (619 before 2024-01-02, 165 from that date). Of those, 548 are in [5%, 10%), 183 in [10%, 20%), 45 in [20%, 50%), 6 in [50%, 100%), and 2 at or above 100%. Capping below 100% leaves 782, of which 165 are from 2024-01-02, on 138 sessions. The two excluded primary opens are JILL on 2020-09-01 (adjusted gap 1.201) and NAGE on 2016-04-13 (adjusted gap 2.258). Neither is a split ex-date, and the raw gap matches the adjusted gap.
  - Cross names: 808 gaps of at least 5% (540 / 268), and 806 below 100% (538 / 268) on 206 out-of-sample sessions. The two excluded opens are AVNW on 2016-06-14 (adjusted gap 9.382) and OPI on 2019-01-02 (adjusted gap 2.830). Same check: not split ex-dates, raw matches adjusted.
  - JILL also has several opens in the 50–80% range in 2020 (2020-06-16, 2020-07-16, 2020-09-14, 2020-10-13). Those stay inside the cap. They were counted, not scored.
  - Controls, same 5% count, daily history only from about 2021-09-16: QQQ 0, SPY 0, IGV 2, AAPL 2, AMZN 8, META 10, GOOGL 7, ADBE 3, AAL 6. Together 38, of which 12 are from 2024-01-02. None reached 20%.
  - No kept bar had a nonpositive OHLC, and none had a high or low that violated open/close order. Primary name-days with a bar today and no bar on the previous NYSE session: 920. Cross: 694. Those days are not signals. Dividend rows on the kept names: 0. Splits are the ones listed in the sibling study (FSBW, JILL, SMTI, AIV, AVNW, BNED, CZFS, PERI, TBCH, ZH) plus the control splits the count printed.
- **Earlier studies.** `low-liq-high-vol-mean-reversion` used these exact names, the same even/odd split, and the out-of-sample window that starts 2024-01-02. It was rejected. Out of sample the weekly quintile reversal returned −56.6% after 20 bp per side and a 5% borrow (Sharpe −1.13, profit factor 0.78, 521 trades). The short leg's average net was −37.7 bp. That is a one-week hold, not an opening gap. The equal-weight screen's out-of-sample Sharpe was 0.16. SPY's was 1.20. Intraday ETF fades were also rejected: `igv-small-account-fade` (out-of-sample Sharpe −3.82) and, as recorded in that study's rules because the folder is not in the tree now, `qqq-intraday-reversion` (out-of-sample Sharpe −0.76 on QQQ). `intraday-channel-trend` lost 12.5% out of sample (Sharpe −0.56). `qqq-atr-scale-in` lost 8.81% out of sample (Sharpe −0.57). The two paper-trading candidates, `qqq-intraday-trend` and `qqq-atr-martingale`, are QQQ trend and QQQ pullback rules, not this screen.
- **What I already know about the test windows.** The out-of-sample window 2024-01-02 through 2026-09-25 is not unseen for these names: the weekly reversal lost money on it, including on shorts. The overlapping QQQ window from 2024-07-01 is also not unseen. It contains the April 2025 tariff crash and rebound. QQQ buy-and-hold returned about +55% from 2024-07-01, and QQQ trend-following made money while the ETF fades did not. I have not seen an open-to-close return conditional on a 5% gap, for these names or any others.
- **Where the parameters came from.** The 5% gap is the user's threshold, used as given. It was not chosen by looking at a return. The 100% cap is the data check above: an open at a double or more is not treated as a usable auction for a resting limit, and the four such opens are listed. The 20 bp per side is the figure already locked for this screen in the sibling study, from the cost problem in Novy-Marx and Velikov (2016), not from these gap days. The even/odd books are the sibling's alphabetical split, reused so this study does not redraw the holdout after the fact.

## Hypothesis

On this screen, a stock whose opening auction clears at least 5% above the previous session's close, and below a double, has a negative open-to-close return large enough that shorting the auction and covering at the close has an out-of-sample Sharpe of at least 0.5 after 20 bp per side.

**Mechanism.** The opening print in a thin small cap is pushed up by impatient buyers, often retail attention after overnight news, who pay more than a patient holder would. The short supplies shares into that auction. During the session the price gives the concession back. Berkman, Koch, Tuttle, and Zhang (2012, Journal of Financial and Quantitative Analysis) find positive overnight returns and negative trading-day returns, concentrated in stocks that are hard to value and costly to arbitrage and that have attracted retail attention. Lou, Polk, and Skouras (2019, Journal of Financial Economics) find an offsetting reversal between the overnight and intraday components: a portfolio of overnight winners has negative intraday alpha. The book here trades about $1–6 million a day, which is the costly-to-arbitrage end of that claim. Novy-Marx and Velikov (2016) are why the test charges a real spread: a short-horizon reversal can be true gross and still fail net.

**Known counter-forces.** The gap can be the information, in which case the close is higher than the open. These names' one-week short book already lost money in the sibling study, so the same names can be expensive or hard to borrow on a spike; this test charges no locate and no borrow. The screen is a September 2026 snapshot, so the list is survivorship. A market-wide gap is beta, not single-name attention. There are no medium-cap names to test the other half of the user's request. There are no intraday prints, so a stop cannot be simulated without inventing a path.

## Predictions beyond P&L

If the mechanism is right, then, on the primary book:

1. The mean of close / open − 1 on signal name-days is lower than the mean of close / open − 1 on primary name-days that have a usable bar and a positive previous-session close but are not signals. The gap is the filter. Days with a gap at or above 100% are in neither set.
2. Among primary trades, the mean gross short return is higher when the gap is at or above the median gap than when it is below the median. Larger openings revert more.
3. Among primary trades with a trailing dollar-volume figure, the mean gross short return is higher in the half at or below the median dollar volume than in the half above it. The concession is larger in the thinner name. A half with fewer than 30 trades makes this prediction not testable.
4. The mean gross short return on the primary trades is higher than the mean gross short return on the control names under the identical gap rule. Fewer than 30 control trades makes this not testable. The pre-lock count is 38, so the comparison is expected to be scored. The count itself is already known and is not the prediction.
5. Among primary trades with a same-day SPY overnight gap, the mean gross short return is higher when the stock's gap exceeds SPY's gap by at least 5 percentage points than when the excess is smaller. A market-wide open is not the attention concession. A half with fewer than 30 trades makes this not testable. Trades before SPY's daily history are left out of both halves.
6. The share of primary trades whose low is at or below the previous close is greater than one half. The auction price trades back through the level the limit was set against.

Each one is scored *consistent*, *not consistent*, or *not testable*. A passing P&L with failed predictions has not confirmed the mechanism. Gross short return is (entry − exit) / entry, before costs. Medians use the average of the two middle values when the count is even. The boundary trade goes in the half the sentence names with "at or above" or "at or below."

## Data

- **Screen.** The sibling study's kept names. Primary (27): ACCO, AUDC, BGS, BOOM, CHCT, CLW, CYH, DSX, ELME, FNWD, FSBW, FXNC, HDSN, III, IMMR, JILL, LMNR, NAGE, OSUR, PTLO, RM, RWAY, SMTI, STRT, VFF, XPER, ZUMZ. Cross (28): AIV, AVNW, BNED, CCCC, CION, CZFS, EGAN, FNKO, FRAF, FSTR, GCO, HLLY, HRZN, IIIV, INGN, LE, LOVE, OPI, OVBC, PERI, RAIL, RMNI, SAR, SPOK, TBCH, UIS, VNDA, ZH. Controls, not a candidate: QQQ, SPY, IGV, AAPL, AMZN, META, GOOGL, ADBE, AAL.
- **Bars.** Daily, 2016-01-04 through 2026-09-25, read with `agent-data/mdq.py`, split-adjusted. Dividends are not adjusted. The count found no dividend rows on the screen names. A daily bar's `ts` is 09:30 ET. Its close is known at 16:00 ET, or 13:00 ET on an early close. The strategy uses the open as the auction and the close as the cover. It does not use a close to decide entry.
- **Calendar.** `mdq.nyse_sessions`. 2025-01-09 is not a session. 2021-12-31 is a session; a daily bar on it is eligible. Early closes are `mdq.EARLY_CLOSES`. The official close is 13:00 ET on those dates and 16:00 ET otherwise. Those times label the trade. The price is the daily close either way.
- **Missing bars.** Not forward-filled. A signal needs a bar on the previous NYSE session. An older close is not a substitute.
- **Usable bar.** Open, high, low, and close are positive, `low <= open <= high`, and `low <= close <= high`. Anything else is not a signal and is left out of the benchmarks. The pre-lock count of such bars on the books was zero. The script checks again and stops before writing results if a kept screen name has a nonpositive open or close.
- **History floor.** Every primary and cross symbol must resolve and have at least 252 daily bars in the window. The script stops before writing results if one does not. Controls are exempt from the 252-bar floor; their history starts in September 2021 and the report says so.
- **Dollar volume, for prediction 3 only.** On the 21 NYSE sessions strictly before the signal, the median of close × volume on bars with a positive close and a non-negative volume. At least 15 such bars are required. Adjusted close × adjusted volume is the product the store's split adjustment preserves. This number is not a filter.

## Primary rule

Parameters, all fixed:

- `GAP_MIN = 0.05`. The user's threshold.
- `GAP_MAX = 1.00`. Opens at a double or above are excluded. Source: the four opens listed under prior exposure.
- `COST_BPS = 20` per side. `COST_MULT = 1` on the primary. The multiplier scales the spread. It does not invent a borrow fee.
- `MIN_BARS = 252`.
- Seeds: direction placebo `20260926`, block bootstrap `20260927`, timing placebo `20260928`. Verify replays every primary trade, so it has no sample seed.
- Placebo sizes: direction 2,000 draws, timing 500 draws, bootstrap 2,000 draws of 20-session blocks.

1. **Order, placed before the open.** The previous session's close is known at that session's official close. Before today's open, a limit order to short rests at `prior_close * (1 + GAP_MIN)`. It is not revised with any price from today. If today's open is below that limit, the order is cancelled at the open and there is no trade. If today's open is at or above the limit and strictly below `prior_close * (1 + GAP_MAX)`, the opening auction traded through the limit. The fill is the opening print, not the limit. A marketable limit participates at the clearing price. The order does not stay working after the open: a rally through the limit later in the day is not a fill. That case is also invisible in a daily bar.
2. **Signal.** A primary name on session `d`. Let `p` be the previous NYSE session. Both sessions have bars, `p`'s close is positive, and `d`'s bar is usable. Let `ratio = open_d / close_p`. A signal is `ratio >= 1 + GAP_MIN` and `ratio < 1 + GAP_MAX`. One signal per name per session. No long. No flip. No second entry.
3. **Exit.** Cover at `d`'s close. Reason `session_close`. There is no stop and no target. High and low are not used. The position is flat overnight.
4. **Sizing.** Equity starts at 1. On a session with `n >= 1` signals and equity `E > 0` before the open, each name is allocated notional `E / n`. Shares are `notional / open`, and may be fractional. Every name uses that same `E`, before costs. Gross exposure is 1 on a session with a signal and 0 otherwise. A session with no signal returns 0.
5. **Costs and P&L.** Entry cost is `COST_MULT * COST_BPS / 10000 * notional`. Exit cost is `COST_MULT * COST_BPS / 10000 * shares * close`. Gross price P&L is `shares * (open - close)`. Net P&L is gross minus both costs. The day's return is net P&L / `E`. Equity becomes `E + net P&L`. No borrow, no interest on cash. If `E + net P&L <= 0`, that day's return is −1, equity is set to 0, the day's trades are kept, and no later session opens a trade.
6. **Daily return.** Simple return of equity on every NYSE session from 2016-01-05 through 2026-09-25. 2016-01-04 is warm-up and is not in the series. A session with no position is 0. Sharpe is the mean of those daily returns divided by their sample standard deviation (divisor `n − 1`), times `√252`, with a zero rate.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Previous close, and the limit `prior_close * 1.05` | Official close of the previous session | The order resting before today's open |
| Today's open | Opening auction | Cancel, or fill at that print. Not known when the limit was placed |
| Today's close | Official close, 16:00 ET or 13:00 ET | The cover. Not an entry input |
| Today's low | Official close | Secondary accounting only. Not an entry input |
| Trailing dollar volume | Close of the previous session | Prediction 3 only. Not a filter |
| SPY open and previous close | SPY's opening auction | Prediction 5 and the breakdowns. Not a filter |
| Screen membership and the September 2026 market cap | The snapshot | The candidate list only |
| Quintile edges of SPY's close-to-close return | The full sample, after the run | The breakdown only |

The open is both the information that the limit was filled and the fill price. That is the auction. It is not a market order sent after the print. A trader who waits to see the open and then sells cannot claim this price. The delay book below is that trader.

## Samples

- **Warm-up.** 2016-01-04, and any later session on which a name still has no previous-session close.
- **Out-of-sample.** Evaluation sessions on or after 2024-01-02, through 2026-09-25.
- **In-sample.** Evaluation sessions from 2016-01-05 through 2024-01-01.
- **Why this split.** 2024-01-02 is the first session of a calendar year and is the split the sibling study already used on these names. The most recent years are out of sample. That window is not unseen, as prior exposure says. Daily returns are split by the session date. A trade belongs to the sample that contains its entry session. Primary trades do not cross the boundary.

## Benchmarks

All three are uncosted, start at equity 1, use the same evaluation sessions, and floor a wiped equity at 0 the same way the primary does. None is an acceptance line.

- **Unconditional intraday short.** Each session, equal weight across primary names with a usable bar, return = mean of `(open − close) / open`. A session with no usable name returns 0. This is the open-to-close short without the gap filter.
- **Equal weight, close to close.** Each session, equal weight across primary names with a usable close and a positive close on the previous NYSE session, return = mean of `close / previous close − 1`.
- **SPY, close to close.** The same formula for SPY. A session without a SPY return contributes 0. SPY's first stored daily bar is 2021-09-16, so this benchmark is uninformative before the first session that has a SPY close-to-close return. The report also gives strategy and SPY metrics on the evaluation sessions from that first SPY-return session onward. That slice is a coverage disclosure. Acceptance stays on the full window.

## Secondary candidate

**Gap-fill limit.** The same entry, the same names, the same sizing, and the same 20 bp. At the auction, a limit to cover rests at the previous close. If the day's low is at or below that price, the cover fills at the previous close and the reason is `gap_fill`. Otherwise the cover is the close and the reason is `session_close`. The low is applied at the end of the session to a limit that was resting all day. There is no stop, so a low and a high on opposite sides of the entry do not compete. The fill is the limit, not a better price below it. Its own line, which does not replace a failed primary: out-of-sample Sharpe ≥ 0.5, out-of-sample profit factor ≥ 1.10, and at least 100 out-of-sample trades, at 20 bp per side.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at the base cost, for the primary, the secondary, the cross book, and the control book: the protocol defaults. Profit factor is the sum of positive trade net returns divided by the absolute sum of negative trade net returns. Net return is net dollar P&L divided by entry notional. Average net trade is the mean of those net returns, in basis points. If there is no losing trade, the profit factor is undefined. Exposure is the share of evaluation sessions with a trade. Trades per year is the trade count divided by (sessions / 252).
- Breakdowns, primary only: calendar year; exit reason; gap bucket [5%, 10%), [10%, 20%), [20%, 50%), [50%, 100%); quintile of the same-day SPY close-to-close return. Quintile edges use the full sample of sessions that have a SPY return. Sessions without one are a separate bucket. None of these is a filter.
- Costs: multiplier 0, 0.5, 1, 2, and 3 on the 20 bp. No borrow is scaled because none is charged.
- Fill delay, a separate book, not a candidate. The same signals. The short is filled at the signal session's close and covered at the next session's open, or skipped if that open does not exist or is not positive. The trade's whole net P&L is marked on the exit session. Sizing uses equity after that day's delayed covers and before the new entries. New entries do not change the entry session's return. Reported at multiplier 1 and at multiplier 0.
- Direction placebo on gross price P&L, at the share counts of the base-cost primary: 2,000 draws, seed `20260926`. Each trade's gross dollar P&L is multiplied by an independent ±1. The daily gross P&L is compounded from equity 1 with no costs. `p = (1 + count of draws whose Sharpe is at least the actual gross Sharpe) / 2001`.
- Timing placebo: on each session, the same number of names as the primary signaled, drawn without replacement from the primary names that have a usable bar and a positive previous-session close, shorted open to close with the primary's sizing and costs. 500 draws, seed `20260928`, scored on net Sharpe against the primary's net Sharpe. `p = (1 + count of draws whose Sharpe is at least the actual) / 501`.
- Block bootstrap: 20-session circular blocks of the primary's net daily returns, 2,000 draws, seed `20260927`, the 2.5 and 97.5 percentiles of the full-sample Sharpe.
- Plateau grid on the in-sample window: `GAP_MIN` in {0.03, 0.04, 0.05, 0.07, 0.10}, with `GAP_MAX` fixed at 1. Five cells, the primary among them. Each cell is scored on the same evaluation sessions. Out-of-sample Sharpes are shown and nothing is selected.
- Cross-market: the odd-ticker book, identical primary rules.
- Controls: the nine liquid names, identical primary rules, as one equal-weight book. This is prediction 4's sample and a reported book. It is not acceptance line 5.
- Verification: the synthetic self-test before the store is opened, and `verify.py` replaying every primary trade with no shared signal code. Side, symbol, entry session, entry price, exit session, and exit price must match.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 20 bp per side:

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 2× the base spread (40 bp per side).
5. Out-of-sample Sharpe > 0 on the cross-market book under identical rules.
6. At least 100 out-of-sample trades. Below that, the verdict is **Inconclusive**, including when other lines fail. The pre-lock count of capped primary gaps from 2024-01-02 is 165, so the default floor is unchanged.

An undefined Sharpe or profit factor does not pass the line that needs it. A failed line fails the strategy. The secondary's line does not promote it.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the gap-fill secondary; the delay book; the control book; the cross book; a single name; gap-downs, which were not counted as a candidate and are not a long book; a SPY-flat or idiosyncratic-only filter; a dollar-volume filter; dropping 2020, the JILL cluster, or April 2025; a lower spread; a borrow assumption added after the run; a weight cap that changes single-name days after seeing their variance; any claim about medium-cap stocks, which are not in the store.
