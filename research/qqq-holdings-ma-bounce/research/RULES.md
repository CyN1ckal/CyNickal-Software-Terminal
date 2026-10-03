# QQQ-holdings 150/250 moving-average bounce: pre-registered rules

Written 2026-10-03, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `research/qqq-holdings-ma-bounce/research/counts.py`, written to `counts.json`. It counts daily bars and session spans only. No price, return, forward return, signal count, hit rate, or P&L. NYSE sessions from 2021-10-04 through 2026-10-02: 1,255. Warm-up sessions before 2022-10-03: 251 (2021-10-04 through 2022-09-30). Evaluated sessions: 1,004 (2022-10-03 through 2026-10-02). All 101 tickers below resolve. Ten names have fewer than 249 warm-up bars because they list later (CEG, WBD, GEHC, ARM, ALAB, NBIS, SNDK, CRWV, FER, HONA). Names with at least one missing evaluated session: SPCX 104, SNDK 593, ARM 238, ALAB 367, NBIS 515, HONA 927, CRWV 623, FER 369, ALNY 1, GEHC 52. Those missing-eval counts mix late listings with vendor holes. QQQ and SPY each have 1,254 daily bars, first 2021-10-04, last 2026-10-01, so each is missing the evaluated session 2026-10-02 and no other session in this span. The user-supplied coverage note is the rest of the hole list: ALNY 2023-09-13; FER 162 sessions from 2023-08-02 through 2024-05-02 after a 2023-08-01 start; SPCX 104 sessions including 2026-04-07 through 2026-05-22 and 2026-05-26 through 2026-06-11; 2025-01-09 is the day-of-mourning closure and is not an NYSE session in `mdq`. Only ADBE and GOOGL have any 1-minute bars. This study does not read them.
- **Earlier studies on the same instruments, periods, or mechanism:** Constituent daily paths have not been studied. `spy-rsi2-dip-buy` is a paper-trading candidate on SPY (RSI(2) close-to-close dip buy): out-of-sample Sharpe 1.29 and total return +33.8% from 2024-07-01 through 2026-09-25, timing-placebo p = 0.046 and seed-sensitive. `qqq-atr-band-dip-eod` was rejected: buying QQQ at the open minus one prior-day ATR and holding to the close had a negative gross result, out-of-sample Sharpe −0.40. The house daily studies `spdr-sector-momentum`, `commodity-etf-momentum`, `fx-etf-momentum`, `spy-overnight-premium`, and `treasury-etf-trend` share an out-of-sample window 2024-07-01 through 2026-10-01, and none of the five was a paper-trading candidate. QQQ intraday studies through 2026-09-25 already describe QQQ's path.
- **What I already know about the test windows:** The out-of-sample window below is not unseen at the index level. Those QQQ studies include a strong trend around April 2025 (tariff crash and rebound). In `qqq-intraday-trend`, dropping April 2025 cut the out-of-sample Sharpe from 0.82 to 0.45. In `spy-rsi2-dip-buy`, the 9 April 2025 session was a large positive day. Nothing in that knowledge is used to change a parameter.
- **Where the parameters came from:** The user specified 150 and 250. Those lengths are a priori from the request, not estimated on this sample. They are not changed. The 20-session hold, the 5 bp single-stock cost, the 1 bp ETF cross-market cost, the samples, and the seed 20261003 are fixed by this request before any return.

## Hypothesis

A QQQ constituent in an uptrend earns a positive open-to-open drift after a dip tags the band between its 150-session and 250-session averages and the same close finishes back above the faster average.

**Mechanism.** The 150/250 lengths are the user's, not a published optimum. The economic cousins are the 200-day trend filter (Faber, 2007, uses a 10-month average) and intermediate-horizon momentum (Jegadeesh and Titman, 1993). In an uptrend, a dip into the long-average band is a liquidity event: stops, rebalancers, and short-horizon sellers hit bids. Trend followers and dip buyers take the other side and lift the close back through the faster average. They keep paying if the intermediate trend is still intact. This test uses the prices of the end-of-sample constituents. It is not a claim about a tradable point-in-time QQQ basket.

**Known counter-forces.** The dip can be the start of a break of the long average, which is why a close under SMA250 exits. The 101 names are one Nasdaq factor, so the equal-weight book can be NVDA or the index in disguise. Membership is known only at the end of the sample. Dividends are not in the price, so a cash dividend looks like a gap down. Five basis points may be thin for the least liquid names and thick for the mega-caps. The out-of-sample window has already been read at the index level.

## Predictions beyond P&L

If the mechanism is right, then:

1. Gross edge decays with holding time, so completed trades that exit on the time stop have a lower mean gross account return than completed trades that exit on the trend break. Scored on the primary book, full sample. Consistent only if both groups are non-empty and the time-stop mean is lower. Not testable if either group is empty.
2. The book's mean daily net return is higher in the upper quintiles of QQQ's close-to-close move than in the lower ones. Quintiles are the kit's full-sample equal-count quintiles of the benchmark series below (quintile 1 is the lowest). Upper is quintiles 4 and 5. Lower is quintiles 1 and 2. Consistent if the average of the two upper mean strategy nets is greater than the average of the two lower means. Not testable if any of those four means is missing.
3. The effect does not require a single mega-cap: the equal-weight book's out-of-sample result is not only NVDA. The check rebuilds the primary book with NVDA removed and the other 100 names unchanged. If the primary out-of-sample total return is not strictly positive, the prediction is not testable. If it is strictly positive, the prediction is consistent only if the ex-NVDA out-of-sample total return is also strictly positive.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. A failed prediction does not become a new rule.

## Data

- Instruments: the 101 equity names below, plus QQQ as the benchmark and QQQ and SPY as the cross-market pair. Read with `agent-data/mdq.py`, split-adjusted daily bars (`adjust=True`). Dividends are not adjusted. The bounce is a price rule, so this is not a total-return test. A dividend gap is a real price gap.
- Universe, fixed, and not dropped after results: NVDA, AAPL, MSFT, MU, AMD, AMZN, META, GOOGL, TSLA, SPCX, GOOG, INTC, AVGO, WMT, CSCO, LRCX, PLTR, AMAT, COST, PANW, NFLX, CRWD, KLAC, TXN, SNDK, MRVL, LIN, AMGN, ADI, QCOM, STX, SHOP, GILD, ASML, TMUS, PEP, WDC, ISRG, ARM, FTNT, VRTX, BKNG, SBUX, ADP, LITE, CDNS, ADBE, SNPS, MAR, DDOG, CEG, CSX, MELI, MNST, APP, WBD, DASH, CTAS, INTU, CMCSA, MDLZ, REGN, ROST, MPWR, TER, ORLY, ABNB, HON, AEP, NXPI, ALAB, MSTR, FAST, NBIS, PCAR, BKR, FANG, PDD, HONA, PYPL, XEL, ADSK, RKLB, MCHP, CCEP, EXC, KDP, CRWV, IDXX, FER, TTWO, ODFL, TRI, WDAY, PAYX, ROP, AXON, DXCM, ALNY, GEHC, CPRT. This is the Invesco QQQ equity-holding list, CUSIP 46090E103, business date 2026-10-02, as of the end of the sample. It is not a historical constituent tape. Index membership is look-ahead. The test is these stocks' prices, not a tradable point-in-time QQQ book. Published end-of-sample QQQ weights are not used.
- Bars: stored daily bars. A daily bar's `ts` is 09:30 ET. Its close is not known until 16:00 ET. No coarser bar is built. Early closes still open at 09:30 ET and are ordinary sessions. 2025-01-09 is not a session.
- Calendar: NYSE sessions from `mdq.nyse_sessions`. The daily path has one row per evaluated session, including sessions on which the book is flat.
- Missing bars: no forward-fill. A name is traded only on a session where it has a daily bar. A session with no bar contributes no price and no return for that name.
- Data checks the script must pass before it writes results: the self-test; all 101 names plus QQQ and SPY resolve; no duplicate daily session; no stored bar with a non-positive open, high, low, or close; the evaluated list is the 1,004 sessions above; 2024-06-28 and 2024-07-01 are both sessions and no session sits strictly between them; QQQ and SPY are missing no evaluated session except 2026-10-02.

## Primary rule

Parameters, all fixed:

- SMA_FAST = 150 and SMA_SLOW = 250 (the user's a priori lengths).
- HOLD = 20 evaluated sessions (the user's a priori hold).
- FILL_LAG = 1 session for the primary (signal at the close of t fills at the open of t+1, then later if that session has no bar).
- COST = 5 bp of notional per side, 0.0005 (single stocks; the protocol's 1 bp is for QQQ/SPY-class ETFs).
- SEED = 20261003 for the direction placebo, the timing placebo, and the bootstrap.

1. **Averages.** At a session t that has a close, SMA150 is the mean of the 150 most recent split-adjusted closes of that name ending at t, and SMA250 is the mean of the 250 most recent closes ending at t. Sessions with no bar are omitted, not filled, so a hole makes the window cover more than N NYSE sessions. If fewer than N closes exist through t, that average is undefined. The close of t is inside the window.
2. **Signal.** Evaluated only at the close of an evaluated session, and only when both averages are defined. Trend: `close_t > SMA250_t` and `SMA150_t > SMA250_t`. Touch: `low_t` is inside the closed interval between SMA150_t and SMA250_t. Bounce: `close_t > SMA150_t` on that same session. All three must be true. The signal uses the close only for a fill at a later open. No signal is evaluated on a warm-up session, so the first possible entry is the open of the second evaluated session.
3. **Position.** Long only. One position per name. If the name is already long, or an entry is already waiting for a bar, ignore the signal. No shorts, no adds, no profit target, no ATR stop. After an exit has filled, a later close can start a new trade. A signal on the close that schedules the exit is ignored, because the name is still long at that close.
4. **Exits,** checked in this order, filled at the next open (FILL_LAG = 1), and if that session has no bar, at the next later evaluated session that has an open:
   - Trend: `close_t < SMA250_t`, which requires a bar and a defined SMA250. If the average is undefined, this exit does not fire.
   - Time: the open that is 20 evaluated sessions after the entry session. The entry session counts as 0. Evaluated sessions are NYSE sessions in the sample, including sessions where this name has no bar. If that session has no bar, wait for the next session that has an open.
   - If both would fill at the same open, the reason is trend.
5. **Fills.** Entry price is that session's split-adjusted open. Exit price is the exit session's split-adjusted open. A fill scheduled after the last evaluated session does not happen. A position still open after the last session is not a row in `trades.csv`. Open-to-open steps it has already earned stay in the daily path. There is no sample-end flatten and no mark to the last close.
6. **Sizing.** At each open the target is the set of names that are long after that open's fills. Equal weight. Absolute weights sum to 1 when any name is held, else 0. A held name with no bar stays in the set. Its weight still counts. It cannot be dropped until it has an open.
7. **Costs.** On a session, cost is `0.0005 * sum(|new weight − old weight|)` across names, including notionally rebalanced names that have no bar that day. Cash earns 0. No borrow. The entry open earns no price return. The exit open earns the open-to-open step that ends there, and then the weight goes to 0. The strategy return on session D is the sum of `weight_prev * (open_D / open_prev − 1)` over names held from the previous open, minus that session's cost. `open_prev` is the previous session on which that name had an open, not a filled price. If D has no bar, that name's contribution is 0. Flat days are 0. Daily simple returns. Compound equity. Sharpe is mean / sample standard deviation × √252, with a zero risk-free rate.

Worked time stop: an entry at evaluated index k exits for time at evaluated index k+20 when that session has an open. The position earns the twenty open-to-open steps from k to k+1, …, k+19 to k+20.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Open of session t | 09:30 ET on t | Fill on t, and the open-to-open return that ends at the next open |
| High, low, close of session t | 16:00 ET on t (the stored `ts` is 09:30) | Signal and trend exit at the close of t; fill no earlier than the next open |
| SMA150 and SMA250 | Close of t, from closes through t only | Same close. No full-sample mean, z-score, or quantile |
| Holding clock | Entry session, which is already known | Time-stop open |
| Universe | End-of-sample holdings file, business date 2026-10-02 | Every session. Look-ahead. Disclosed. Not a point-in-time book |
| QQQ published weights | Not used | Not used |
| QQQ close | 16:00 ET | Benchmark and move quintiles only. Not a signal input |

## Samples

- Warm-up: each name's stored daily bars from its first session through 2022-09-30. Indicators only. No warm-up signal.
- **In-sample:** evaluated sessions from 2022-10-03 through 2024-06-28.
- **Out-of-sample:** evaluated sessions from 2024-07-01 through 2026-10-02.
- The split is the one fixed for this batch. 2024-06-29 and 2024-06-30 are not sessions. A trade's `session` is its entry date. It counts as in-sample when that date is on or before 2024-06-28, and out-of-sample when that date is on or after 2024-07-01. The out-of-sample window is not unseen at the index level, as prior exposure says.

## Benchmarks

- Uncosted QQQ close-to-close on the same evaluated sessions. On session D, `close_D / close_prev − 1`, where `close_prev` is the previous NYSE session's QQQ close. If either close is missing, that session's benchmark return is 0. The missing close is not forward-filled. `counts.py` says this zero is 2026-10-02 and no other evaluated session.
- The move quintiles use this same benchmark series.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at the 5 bp cost: the protocol defaults, compound equity, with exposure from the fraction of sessions the book is held into.
- Breakdowns: calendar year; side; exit reason; quintile of the QQQ close-to-close move. These do not filter the rule.
- Costs: multiples 0, 0.5, 1, 2, and 3 of the 5 bp base. The 1× row is the primary.
- Fill delay: the close signal at t fills at the open of t+2 instead of t+1 (FILL_LAG = 2), for both the entry and the trend exit. The time stop stays 20 evaluated sessions after the actual entry session. If the time stop is due before a lagged trend fill, the time stop exits and the trend order is cancelled. A same-bar close fill may be reported only as a labelled upper bound: fill at the signal close, close-to-close steps, trend exit at the decision close, time exit at the close 20 evaluated sessions after that entry close, same 5 bp cost. It is not an acceptance input.
- Direction placebo on gross account pieces of completed trades: 2,000 draws, seed 20261003. One sign per trade, kit formula `p = (1 + count of draws ≥ actual) / 2001`.
- Timing placebo: 500 draws, seed 20261003. Each draw places, per name, the same number of long entries as that name's completed primary trades. Candidates are evaluated sessions that have an open and whose exit under the same trend-then-time rules completes inside the sample. A shuffled candidate list is packed so holding intervals `[entry, exit)` do not overlap for that name. A draw that cannot place the full count is discarded, up to 20,000 attempts. The statistic is the zero-cost equal-weight gross Sharpe. Actual is the zero-cost gross Sharpe of the completed primary trades' pieces.
- Block bootstrap: 20-session circular blocks, 2,000 draws, seed 20261003, on the primary net daily path. Not an acceptance line.
- Plateau grid on the in-sample record: HOLD ∈ {10, 15, 20, 30, 40} at the 5 bp cost. Five cells. The primary is 20. The 150 and 250 lengths are not gridded. Out-of-sample cells are shown for selection bias only. Nothing is selected from the grid.
- Cross-market: the identical signal, exits, and 20-session hold on QQQ alone and on SPY alone, each as a 0-or-1 position, at 1 bp per side (0.0001). These two rows are the cross-market test. They are not part of the 101-name book.
- Ex-NVDA book: the primary rule on the other 100 names. Prediction 3 only. Not a second primary.
- Verification: the synthetic self-test below, and `verify.py` matching every completed trade on side, entry time, entry price, exit time, and exit price. Seed 20261003 is recorded if a sample is used. A full match does not need a sample.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 5 bp per side. These are the six protocol defaults. No secondary.

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 2× base cost per side.
5. Out-of-sample Sharpe > 0 on at least one of QQQ and SPY under the identical rules at 1 bp per side.
6. Minimum sample: at least 100 out-of-sample trades. Below that, the verdict is **Inconclusive**.

A failed line fails the strategy. The kit's status order is the verdict: a short out-of-sample sample is Inconclusive even if other lines also fail.

## Not done in this study

The report will not promote any of these in place of the primary: a grid hold; a 150 or 250 length other than the pair above; the long side re-cut by a filter; a single symbol, including NVDA; a short side; an ATR stop or a profit target; published QQQ weights; dropping a name after seeing results; a time-of-day or regime filter; a different cost, fill, or sample split; the same-bar close fill; the ex-NVDA book as a strategy. A variant suggested by the results needs its own `RULES.md` and data this study did not use.

## Self-test (synthetic, no store)

`backtest.py` runs these cases and aborts before opening the store if any fails. Prices are hand-set. Expected entry price, exit price, and exit reason are literals.

- Entry, then trend exit on the entry session's close, filled at the next open.
- Time exit at entry index + 20, with closes far above SMA250 so the trend exit does not fire.
- A missing bar on a held session contributes 0, and the next open earns the gap from the previous open. A missing time-stop session waits for the next open.
- A trend fill whose session has no bar waits for the next open.
- A second valid signal while the name is already long does not open a second trade.
- A name without 250 closes does not enter.
- A low outside the band does not enter.
- When the trend fill and the time-stop open are the same session, the reason is trend.
- Two names in the band together take weight 1/2 each, and the account return is the equal-weight open-to-open step minus 5 bp on the unit of gross turnover at entry and at exit.
