# QQQ holdings OBV divergence: pre-registered rules

Written 2026-10-03, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `counts.py` (kept in this folder, output in `counts.json`). It loaded split-adjusted daily bars for the 101 names, QQQ, and SPY from 2021-10-04 through 2026-10-02 and counted sessions only. It did not print a price, a return, a forward return, a chart, or a signal. `nyse_sessions(2021-10-04, 2026-10-02)` has 1,255 sessions. The evaluation calendar `2022-10-03` through `2026-10-02` has 1,004 sessions. All 101 names resolve and have at least one bar. Eighty-nine names have a bar on every one of the 1,255 sessions and a last session of 2026-10-02. Later first sessions, with no hole after that first bar: CEG 2022-01-19 (1,181 bars), WBD 2022-04-04 (1,129), GEHC 2022-12-15 (952), ARM 2023-09-14 (766), ALAB 2024-03-20 (637), NBIS 2024-10-21 (489), SNDK 2025-02-13 (411), CRWV 2025-03-28 (381), FER 2023-08-01 (635 bars), HONA 2026-06-15 (77). Holes inside the first-to-last span: ALNY one session, 2023-09-13 (1,254 bars, last session still 2026-10-02); FER 162 sessions, 2023-08-02 through 2024-05-02; SPCX 104 sessions, first 2024-11-18 and last 2026-06-11, including 2026-04-07 through 2026-05-22 and 2026-05-26 through 2026-06-11. 2026-05-25 is not a session. QQQ and SPY each have 1,254 bars, first 2021-10-04, last 2026-10-01, and are missing only 2026-10-02. No outcome was measured.
- **Earlier studies on the same instruments, periods, or mechanism:** There is no earlier OBV study in this repo. `qqq-holdings-earnings` listed these same 101 names on 2026-10-03 and was not run: it did not read prices and it computed no return. It is not this rule. `low-liq-high-vol-mean-reversion` is a weekly reversal on a different small-cap screen and was rejected. `qqq-bollinger-adding` is an intraday QQQ band fade and was rejected (out-of-sample Sharpe −0.89 through 2026-09-25). Neither is this rule. Constituent OBV paths have not been studied.
- **What I already know about the test windows:** The out-of-sample window is **not unseen at the index level.** House studies already report 2024-07-01 through 2026-10-01 for several ETFs, and the intraday QQQ reports describe QQQ's own path through 2026-09-25. From those reports, before any return in this study: QQQ buy-and-hold from 2024-07-01 through 2026-09-25 was about +55.4%, with a Sharpe near 0.97 to 1.01 and a max drawdown near −23% to −24%. SPY close-to-close from 2024-07-01 through 2026-10-01 was about +40.5% to +41.7%, Sharpe near 1, max drawdown near −19% to −20%, including about +10.5% on 2025-04-09. April 2025 was a tariff crash and a rebound. Calendar 2022, inside this study's in-sample window, was a down year for the equity indexes in those reports. `qqq-intraday-trend` was a paper-trading candidate on QQQ (out-of-sample Sharpe 0.82) and negative on SPY (Sharpe −0.36); its edge was flat after May 2025 while QQQ rose. This study's window runs through 2026-10-02, which is one session past 2026-10-01 and a few sessions past 2026-09-25. I do not know the OBV divergences of these 101 names. Earlier reports also say stored SPY daily closes can differ from the 15:59 regular-hours print from November 2024. This study uses the stored daily open and close anyway. That minute comparison was not redone before the lock.
- **Where the parameters came from:** The user's study design, fixed before any return on these names. W = 5, a swing separation of 10 to 60 sessions, and a 20-session hold are round a priori choices, not estimated on this sample. The 5 bp single-stock cost, the 1 bp QQQ/SPY cost, the grid, the 2024-07-01 split, and the seed 20261003 were fixed the same way. None of these is to be changed after seeing returns.

## Hypothesis

On the current QQQ equity holdings, a Granville on-balance-volume divergence at a confirmed swing predicts the open-to-open drift over the next 20 sessions: a lower price low with a higher OBV low is bullish, and a higher price high with a lower OBV high is bearish.

**Mechanism.** Joseph E. Granville, *Granville's New Key to Stock Market Profits* (Prentice-Hall, 1963), defined on-balance volume as a cumulative line that adds volume on an up close and subtracts it on a down close. His claim was that volume leads price, so a price swing that is not confirmed by OBV is the point where the other side is still trading the price extreme after cumulative volume has turned. Those traders are stop-outs, margin calls, and price-only trend rules. They keep paying if that flow is mechanical and if the OBV disagreement is not already crowded. This study takes that disagreement, on both sides, and holds until the swing fails or 20 sessions pass. The swing itself is not Granville's. It is a causal fractal: the pivot is not known until W later closes exist. That is only so the rule does not read the future. It is not a fitted window.

**Known counter-forces.** A lower low on higher OBV can still be a trend that is being accumulated, and the failure exit is that case. OBV counts shares, not dollars, and it ignores where the close sits in the bar. The universe is end-of-sample index membership, so it keeps names that survived to 2026-10-02. Dividends are not in the series. The book is equal absolute weight, not the index. The out-of-sample window is already known to be a strong index advance, which helps the long side and hurts the short side. Both sides stay in the primary anyway.

## Predictions beyond P&L

If the mechanism is right, then, on the full sample of primary trades:

1. Both the long side and the short side have a gross profit factor above 1. OBV divergence is symmetric, so neither side is optional. Gross profit factor is the sum of strictly positive trade gross divided by the absolute sum of strictly negative trade gross. A side with no trades, or with no losing trade, is not testable. The prediction is consistent only if both sides are defined and both are above 1. Otherwise it is not consistent, or not testable if either side is undefined.
2. Failure exits are a minority of exits. The swing marked exhaustion, so a later close through that swing should be the less common exit. The share is failure exits divided by (failure exits + time exits). `sample_end` is not in the denominator. Minority means strictly below one half. If there is no failure exit and no time exit, the prediction is not testable.
3. Trades whose absolute swing-price gap is strictly above the full-sample median have a higher average gross return than trades strictly below the median. The gap is the absolute difference between the two swing closes. The median uses the ordinary even-count average of the two middle values. Trades equal to the median are in neither group. Average means the arithmetic mean of trade gross. If either group is empty, the prediction is not testable. The gap is not a filter and it is not a new rule.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. The script records the score. A pass on P&L with a failed prediction has not confirmed the mechanism.

## Data

- Instruments: the 101 equity holdings of Invesco QQQ, CUSIP 46090E103, business date 2026-10-02, fixed as NVDA, AAPL, MSFT, MU, AMD, AMZN, META, GOOGL, TSLA, SPCX, GOOG, INTC, AVGO, WMT, CSCO, LRCX, PLTR, AMAT, COST, PANW, NFLX, CRWD, KLAC, TXN, SNDK, MRVL, LIN, AMGN, ADI, QCOM, STX, SHOP, GILD, ASML, TMUS, PEP, WDC, ISRG, ARM, FTNT, VRTX, BKNG, SBUX, ADP, LITE, CDNS, ADBE, SNPS, MAR, DDOG, CEG, CSX, MELI, MNST, APP, WBD, DASH, CTAS, INTU, CMCSA, MDLZ, REGN, ROST, MPWR, TER, ORLY, ABNB, HON, AEP, NXPI, ALAB, MSTR, FAST, NBIS, PCAR, BKR, FANG, PDD, HONA, PYPL, XEL, ADSK, RKLB, MCHP, CCEP, EXC, KDP, CRWV, IDXX, FER, TTWO, ODFL, TRI, WDAY, PAYX, ROP, AXON, DXCM, ALNY, GEHC, CPRT. Both GOOGL and GOOG are included. Cash, pending dividends, and the Nasdaq future are not. Read with `agent-data/mdq.py`, split-adjusted. Dividends are not adjusted. A long does not receive the dividend and a short does not pay it. The ex-date open is the stored open, so the ex-date gap is in the open-to-open return. This is not a total-return test. Published end-of-sample weights are not used.
- Membership is the end-of-sample list, not a historical constituent tape. **Index membership is look-ahead.** A name that was not in QQQ on the signal date is still eligible. No name is dropped after seeing results.
- Bars: stored daily bars. A daily bar's `ts` is 09:30 America/New_York. Its close is not known until the session close (16:00, or 13:00 on an early close). Signals use the close and fills use a later open, or a same-bar close only in the labelled upper bound.
- Calendar: evaluated sessions are the NYSE sessions from 2022-10-03 through 2026-10-02 inclusive (1,004 sessions). Warm-up bars are those before 2022-10-03. 2025-01-09 is not a session. Early closes stay in the calendar; the daily bar is the session bar. 2021-12-31 is before the evaluation window. If the store has a bar that day it may affect warm-up OBV only.
- Missing bars: not a price and not forward-filled. The session contributes 0 for that name. A fill waits for the next open that exists. The open-to-open step that is eventually earned uses the last real open while the position was on, booked on the session of the later open. Details are under Primary rule.
- QQQ and SPY have no bar on 2026-10-02. The benchmark return that session is 0. It is not dropped and it is not forward-filled. The cross-market books cannot fill on a session with no bar.
- Data checks the script must pass before it writes results: 101 names plus QQQ and SPY resolve; QQQ and SPY have a bar on every evaluated session except 2026-10-02; the evaluation list has 1,004 sessions, first 2022-10-03 and last 2026-10-02; every open used as a divisor is strictly positive. A failed check aborts before `results.json` is written.

## Primary rule

Parameters, all fixed:

- `W = 5`. Swing half-width. Round a priori choice, not estimated here.
- `MIN_SEP = 10`, `MAX_SEP = 60`. Inclusive distance between the two swing sessions. Round a priori choice.
- `HOLD = 20`. Time exit. Round a priori choice.
- `COST = 0.0005`. Five basis points per side of single-stock notional. The protocol's 1 bp is for QQQ and SPY, not for these names. The store has no quotes. This rate is not changed after the run.
- `CROSS_COST = 0.0001`. One basis point per side for the QQQ and SPY books.
- `FILL_LAG = 1`. Next printed open. The delay check uses 2. The upper bound uses the signal close.
- Seed `20261003` for the direction placebo, the timing placebo, the bootstrap, and the verification sample.

1. **OBV.** For each name, bars are the stored daily bars from the first bar through 2026-10-02, in session order. A hole is omitted. It is not a zero bar. OBV starts at 0 on the first stored bar. That bar has no prior close, so it is neither up nor down and OBV stays 0. After that, an up close adds that bar's split-adjusted volume, a down close subtracts it, and an equal close leaves OBV unchanged. Volume is the mdq adjusted volume at full precision. The comparison is exact.

2. **Swings.** Index the name's own bars `0 .. n-1`. A swing low at bar index `p = t - W` is confirmed at index `t`, and not before, when `p >= W` and `close[p]` is strictly below every close in `p-W .. p-1` and strictly below every close in `p+1 .. t`. A swing high is the mirror, strictly above both sides. Each side has W closes. One bar is tested when `t` arrives, the bar at `p`. A bar cannot be both a strict high and a strict low. The swing is not known at `p`. It is known at the close of `t`. Neighbors are printed bars, not empty sessions. A hole is not a neighbor and it does not count toward W.

3. **Divergence.** When a swing low is confirmed, compare it with the nearest prior confirmed swing low, if one exists. Distance is `calendar_index(newer swing session) - calendar_index(older swing session)` on `nyse_sessions(2021-10-04, 2026-10-02)`. It is the distance between the swing sessions, not the confirmation sessions. Consecutive sessions have distance 1. The pair qualifies when `10 <= distance <= 60`, the newer swing close is strictly below the older swing close, and the newer swing OBV is strictly above the older swing OBV. That is a bullish divergence, known at the confirmation close. Bearish is the mirror on swing highs: newer close strictly above, newer OBV strictly below, same distance test. If the nearest prior swing is outside 10 to 60, the signal does not walk back to an older swing. Swings confirmed in the warm-up may be the prior swing. A confirmation before 2022-10-03 does not itself fire a trade. If a bullish divergence and a bearish divergence would both be acted on at the same confirmation session, take neither. The scanner can emit at most one from a single close; the book drops the session if both are ever present.

4. **Position.** If the name is already in a position, or it already has a signal waiting for its entry open, ignore the new signal. No adds and no flips. A name becomes flat again at the exit open, so the exit session's close may fire a new signal. Warm-up does not open positions.

5. **Exits,** checked at the close, filled at the next open, in this order:
   - Failure. A long fails when a close strictly after the entry decision, including the entry session's close, is strictly below the newer swing low's close. A short fails when a close is strictly above the newer swing high's close. The fill is the next printed open after that close. An equal close does not fail.
   - Time. Exit at the open of the evaluation session whose index is `entry_index + 20`. The entry session counts as 0. If that session has no bar, the fill waits for the next printed open after it.
   - If both would fill on the same open, the reason is failure.
   - `sample_end`. If the sample ends with no exit open, the position stays in the book through the last evaluation session. The trade is recorded with exit reason `sample_end`, exit price equal to the last printed open while the position was held, and exit time equal to that open. No closing trade is filled, so no exit cost is charged, and no open-to-close mark is added.

6. **Fills.** The primary entry is the first printed open of that name strictly after the confirmation session, and only if that session is inside the evaluation window. `FILL_LAG = 2` skips one printed bar and fills on the second printed open after the confirmation. A lag that runs off the end of the sample does not fill. The same-bar upper bound fills the entry at the confirmation close and is specified under Reported checks. A missing bar delays the fill. It is not filled at a made-up price.

7. **Sizing.** At each evaluation session the target is the set of names the book holds that session, with sign. A name is held on session `S` when it has filled and `entry_index <= index(S) < exclusive_end`. For a filled exit, `exclusive_end` is the exit session's index. For `sample_end`, `exclusive_end` is past the last session, so the last session is held. Equal absolute weight. The absolute weights sum to 1 when any name is held, and to 0 otherwise. The book is not dollar-neutral. A short has a negative weight. Cash earns 0. There is no extra borrow fee. Equity compounds. The daily account return is the simple weighted open-to-open return minus the cost below.

8. **Open-to-open accounting.** The entry open earns nothing before it. The exit open earns the step that ends there. For a held name, the step booked on session `D` uses the previous printed bar `P` of that name such that the position was held at `P` and `P` is before `D`. If `D` has a bar and `D` is either a held session or the exit session, the contribution is `signed_weight_at_P * (open_D / open_P - 1)`. The weight is the target weight on session `P`, the last session where this name actually printed, not a target that changed on a later session where this name had no bar. If `D` has no bar, the contribution is 0 and the gap stays open until the next real open. Sessions with no position contribute 0. The first evaluation session has no step ending on it from inside the window.

9. **Costs.** On session `D`, cost is `0.0005 * cost_multiple * sum(|w_D - w_prev|)`, where `w` is the signed target weight of every name and `w_prev` is 0 before the first session. This charges entry, exit, and a resize when the count of held names changes. It does not charge price drift, because the target is reset from the holding set rather than from drifted share counts. A hole does not remove a name from the holding set and does not by itself create turnover. `sample_end` does not create exit turnover. A trade's gross is the sum of its contributions. A trade's net is that gross minus the cost assigned to its own absolute weight changes, including resizes while it is open. The sum of those assignments equals the book's cost. Profit factor, win rate, and average net trade use these nets. The direction placebo uses the gross pieces, not the nets.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Daily close and volume | Session close, even though `ts` is 09:30 | OBV and swing confirmation at that close; a primary fill no earlier than the next open |
| Swing at bar `p` | Close of bar `p+W`, after both sides exist | Divergence tested at that confirmation close |
| Prior swing | Its own earlier confirmation | The next divergence, never before it confirmed |
| End-of-sample membership | 2026-10-02, after the sample | Every session. This is look-ahead and it is not removed |
| Swing-gap median | Full sample, after the trades exist | Prediction 3 only. Not a signal and not a filter |
| QQQ close | That session's close | Benchmark return for that session. 2026-10-02 has no close and the benchmark is 0 |
| Holding set | Previous closes, plus the clock for the time stop | Weights at the next open |

Rolling statistics in the signal use only bars already printed. Nothing in the signal is normalized on the full sample.

## Samples

- Warm-up: each name's bars before 2022-10-03. OBV and swings only. No position and no return.
- **In-sample:** 2022-10-03 through 2024-06-28.
- **Out-of-sample:** 2024-07-01 through 2026-10-02.
- The split is the repo's existing out-of-sample start, extended through the last requested session. A trade belongs to the sample of its entry session. A trade that enters before 2024-07-01 is in-sample for profit factor even if it exits later. Daily Sharpe uses the session date. The out-of-sample window is not unseen at the index level, as written under Prior exposure.

## Benchmarks

- Uncosted QQQ close-to-close on the same 1,004 sessions. The return on 2022-10-03 uses the previous QQQ close. The return on 2026-10-02 is 0 because QQQ has no bar. No cost and no dividend.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at base cost: the protocol defaults, compound, Sharpe as mean / sample standard deviation × √252 with a zero risk-free rate. Flat sessions are 0.
- Breakdowns: calendar year; long versus short; exit reason; quintile of the QQQ close-to-close move. These are not filters.
- Costs: multiples 0, 0.5, 1, 2, and 3 of the 5 bp rate. The positions do not change with the multiple.
- Fill delay: entry on the second printed open after the signal instead of the first. Same exits, counted from the actual entry. Same-bar close fill is an upper bound only, not an acceptance input. In that bound the entry price is the confirmation close, the exit price is the close that decides the exit (the failure close, or the close of the session `HOLD` evaluation sessions after the entry session, failure first), and the account earns close-to-close while the position is on. It is labelled and it is not tradable.
- Direction placebo on gross contributions: 2,000 draws, seed 20261003. Each trade's pieces share one sign. p = (1 + count of draws greater than or equal to the actual gross Sharpe) / 2001.
- Timing placebo: 500 draws, seed 20261003. Each draw places the same number of entries as the primary trade count. An entry is a name and an evaluation session drawn uniformly from sessions where that name has a bar, a prior close, and a later session. Sides are long or short with equal probability. A name cannot overlap a position it already has in that draw. The failure level is the close of the name's last bar before the entry. The time exit and the next-open rule are the same as the primary. The score is the gross Sharpe of that cost-free book against the primary gross Sharpe, with the same p formula. This is not an acceptance line.
- Block bootstrap: 20-session circular blocks of the net daily path, 2,000 draws, seed 20261003, 95% interval of the full-sample Sharpe. Not an acceptance line.
- Plateau grid on in-sample Sharpe: `W` in {3, 5, 8} and `HOLD` in {10, 20, 40}. Nine cells. The primary is `W = 5`, `HOLD = 20`. Out-of-sample Sharpe is shown and is not used to select. Nothing is selected from the grid.
- Cross-market: the identical divergence rule on QQQ alone and on SPY alone. Position size is +1, −1, or 0. Cost is 1 bp per side. Acceptance uses out-of-sample Sharpe only.
- Verification: a store-free self-test (bullish, bearish, both on one day, a swing that is not confirmed until the later bars exist, a signal ignored while a position is open, a missing bar, a time exit, and a failure exit), then `verify.py`, which does not import `backtest.py`. It matches every primary trade on side, entry time, entry price, exit time, and exit price. If a full match is ever too slow, a seeded sample of at least 40 trades uses seed 20261003. The intended check is the full list.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 5 bp per side. These are the protocol defaults. None is loosened.

1. Out-of-sample Sharpe ≥ 0.50 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 2× base cost.
5. Out-of-sample Sharpe > 0 on at least one of QQQ and SPY under the identical rule.
6. At least 100 out-of-sample trades, counted by entry session. Below that, the verdict is **Inconclusive**, including when other lines also fail.

A failed line fails the strategy. There is no secondary.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone; one symbol; a swing-gap filter or any other gap cutoff; a time-of-day, volatility, or regime filter; a different cost, fill, hold, or sample split; the delay book; the same-bar upper bound; the QQQ or SPY book alone; dropping a late listing or a name with holes; index weights; dollar neutrality; a dividend adjustment; a borrow fee added after the fact.
