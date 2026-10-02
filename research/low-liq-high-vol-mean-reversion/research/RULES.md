# Weekly quintile reversal: low-liquidity small caps

Written 2026-09-26, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `select_universe.py` read `screener.csv` and did not read the price-change columns. Daily bars for the qualifying tickers were then downloaded, at the user's request, with `ingest --timeframe 1d --from 20160104 --to 20260925`. `counts.py` then recorded, and only recorded, bar counts, split counts, dividend-row counts, a count of nonpositive prices, and how many weeks the eligibility rule selects a book. It did not print a price, a forward return, a P&L, or a summary of the formation returns. Those counts are in the Data section. The self-test uses synthetic sessions only.
- **Earlier studies.** None of the stored studies use these names. The short-horizon fade in `igv-small-account-fade` was rejected (OOS Sharpe −3.82 after 2 bp). `qqq-intraday-reversion` is named in `research/README.md` and the folder is not in the tree, so there was no result to read. Those are intraday ETF tests. This study is a weekly cross-section of small caps. The mechanism is related only in the broad sense that both are reversal.
- **What I already know about the test windows.** I have read the other studies' out-of-sample window, 2024-07-01 through 2026-09-25, for QQQ, SPY, and IGV. In that window QQQ's buy-and-hold Sharpe was about 1.0, and the path includes the April 2025 tariff crash and rebound. This study's out-of-sample window starts earlier, on 2024-01-02, and these names were not in those studies. The broad equity path of 2024–2026 is not unseen. I also know, from those same reports, that QQQ's large drawdown of about 36% sits in the early part of the stored history. If that lines up with this study's in-sample window, that regime is not unseen either. The parameters below were not chosen by looking at these names' returns.
- **Where the parameters came from.** The one-week formation and the one-week hold are the published weekly reversal (Lehmann 1990; Jegadeesh 1990), used with the next-session open fill this protocol requires. The quintile sort is the a priori way to take the extremes in a book of about 30 names: a decile would often be a single name, and the published effect is in the extremes rather than the middle. The 63-session volatility and dollar-volume band is the user's Koyfin screen, applied to each signal date with data known at that close. The 20 bp spread and the 5% borrow are stated below and were not estimated from these prices. The user's screen filters were set by the user in Koyfin. They were not fit to a backtest in this repo.

## Hypothesis

On this screen, a stock's return over the next week is negatively related to its return over the week just ended, after a 20 bp one-way spread and a 5% borrow on shorts, so a dollar-neutral quintile book has a positive out-of-sample Sharpe.

**Mechanism.** A week of heavy buying or selling in a name that trades only $1–10 million a day moves the price beyond the level a patient holder would set, because the book is thin. The liquidity demander pays for immediacy. The reversal is the payment. Published evidence that the effect is stronger in small, illiquid, high-volatility stocks: Jegadeesh (1990), Lehmann (1990), Avramov, Chordia, and Goyal (2006), and Nagel (2012). The screen's flat revenue growth is there so the book is not a set of high-growth names in which fundamental momentum would be expected to dominate a one-week reversal. Novy-Marx and Velikov (2016) find that short-term reversal often fails once trading costs are charged. That is the risk this test is built to take. The gross effect can be real and the net strategy can still fail.

**Known counter-forces.** The spread and the borrow. Distress continuation, if this week's losers are getting worse rather than bouncing. Cash dividends, which are not removed from the prices, so an ex-dividend drop looks like a loss and can put a payer into the long side. Survivorship: the screen is a snapshot of companies that still exist, in this band, in September 2026. Market cap and revenue growth are not known historically in the store, so the candidate list is fixed. The rolling volatility and dollar-volume filters are point-in-time. The market-cap and revenue filters are not.

## Predictions beyond P&L

If the mechanism is right, then:

1. Both the long leg and the short leg have positive gross price P&L. A result that comes only from the long leg is the small-cap premium, which the equal-weight benchmark already holds.
2. Among trades, the mean gross position return is higher for the half with absolute formation return at or above the median than for the half below it. Larger dislocations revert more.
3. For holds that produce at least four signed session returns, the mean of the first two exceeds the mean of the last two. The concession is earned early in the week.
4. The mean weekly gross book return is higher in weeks whose cross-sectional standard deviation of eligible formation returns is at or above the median of those weeks than in weeks below it. More dispersion means more one-sided flow.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. A pass on P&L with failed predictions does not confirm the mechanism.

## Data

- **Screen.** `screener.csv` at the study root, 59 rows. A row qualifies when the ticker is 1–5 letters, the name does not contain the whole word warrant, preferred, unit, units, ETF, ETN, acquisition, SPAC, or rights, market cap is 50–400 (the file's units, millions), dollar volume is $1–10 million, 3-month volatility is 20–60 percent, and the 1-year revenue CAGR is −10% to +10% inclusive. All 59 rows qualified. Price-change columns were not used. Qualifying tickers sorted A to Z: even indexes are the primary book, odd indexes are the cross-market book. `select_universe.py` writes `universe.json`.
- **Dropped before any return, and not replaced.** HLS, TCS, and CVO: OpenFIGI reported no US listing, so ingest wrote nothing. PARK: 204 daily bars, 2025-12-03 through 2026-09-25, under the 252-bar floor.
- **Primary (27).** ACCO, AUDC, BGS, BOOM, CHCT, CLW, CYH, DSX, ELME, FNWD, FSBW, FXNC, HDSN, III, IMMR, JILL, LMNR, NAGE, OSUR, PTLO, RM, RWAY, SMTI, STRT, VFF, XPER, ZUMZ.
- **Cross-market (28).** AIV, AVNW, BNED, CCCC, CION, CZFS, EGAN, FNKO, FRAF, FSTR, GCO, HLLY, HRZN, IIIV, INGN, LE, LOVE, OPI, OVBC, PERI, RAIL, RMNI, SAR, SPOK, TBCH, UIS, VNDA, ZH.
- **Bars.** Daily, 2016-01-04 through 2026-09-25, read with `agent-data/mdq.py`, split-adjusted. Dividends are not adjusted. The ingest wrote splits and wrote no dividend rows (the count is 0 for every kept name). An ex-dividend price drop stays in the close. Splits on the kept names: FSBW 1, JILL 2, SMTI 1, AIV 2, AVNW 1, BNED 1, CZFS 4, PERI 1, TBCH 1, ZH 1. Every other kept name has none.
- **Calendar.** NYSE sessions from `mdq.nyse_sessions`. A session with no bar for a name is a missing bar, not a zero return, except that a held name with no print is marked at its last price for that day (P&L of zero) until it can be traded. Early closes have no special case: the daily open and the official close are the prints. A bar with open or close at or below zero is treated as missing. The pre-lock count of such bars on the kept names was 0.
- **Pre-lock signal counts, not outcomes.** Primary: 558 planned rebalance dates, 258 of them with a book, 1,118 name-slots, of which 139 weeks and 630 slots fall on an entry date on or after 2024-01-02. Cross-market: 230 weeks with a book, 1,008 slots, 113 out-of-sample weeks, 522 out-of-sample slots. A slot is a name selected for that week. A trade in the results can span more than one week if the side does not change, so the trade count will be lower than the slot count. 630 out-of-sample slots is why the 100-trade minimum is left at the protocol default.
- **Data checks the script must pass before it writes results.** Every primary and cross symbol resolves and has at least 252 daily bars in the window. No kept bar that is marked present has a nonpositive open or close. The self-test has passed.

## Primary rule

Parameters, all fixed:

- `FORMATION_WEEKS = 1` (Lehmann 1990; Jegadeesh 1990).
- `EXTREME_K = 5`. `q = n // 5`. A week with `q < 2` is flat.
- `VOL_LOOKBACK = 63`, `MIN_OBS = 50`, `MIN_RETURNS = 40`, `VOL_LO = 0.20`, `VOL_HI = 0.60`. The window and the band match the screen's three-month volatility. The observation floors are there so a handful of prints cannot produce a volatility.
- `DOLLAR_LO = 1_000_000`, `DOLLAR_HI = 10_000_000`. Median of close times volume on the bars inside the same 63 sessions.
- `COST_BPS = 20` per side. `BORROW_ANNUAL = 0.05`, Actual/365, shorts only. The cost multiplier scales both.
- `MIN_BARS = 252`.
- `Q_MIN = 2`.
- Seeds: direction placebo `20260926`, block bootstrap `20260927`, timing placebo `20260928`.

1. **Weeks.** Sessions are grouped by ISO year and ISO week. A week with no NYSE session does not appear. The signal week `W` is an ISO week. The formation base is the last session of week `W − F`. The signal close is the last session of week `W`.
2. **Eligibility, known at the signal close.** Inside the 63 sessions ending on the signal session, the name must have a bar on at least 50 sessions and at least 40 close-to-close returns between consecutive NYSE sessions whose end session is inside the window. Realized volatility is the sample standard deviation of those returns, divisor `n − 1`, times `√252`. Dollar volume is the median of close times volume on the bars in the window. An even count averages the two middle values. Both must sit inside the bands above. The formation return is `close(W) / close(W − F) − 1`. Either close missing, or either close not positive, and the name is out.
3. **Book.** Sort eligible names by formation return ascending, then ticker ascending. Long the first `q`, short the last `q`. A name with no print at the fill is dropped, and that side's remaining names are rescaled so the longs still sum to +0.5 and the shorts to −0.5. If either side then has no name, the week is flat. Shares are `weight × equity / fill price`. Equity is the equity after the overnight gap has been marked and before this rebalance's costs. Every name opened at that fill uses that same equity.
4. **Entry and exit.** The fill is the open of the first session of week `W + 1`. The book exits at the open of the first session of week `W + 2`. The signal is placed only when both of those weeks exist. If the exit date is not also a new entry, the book is flattened at that open. A name that stays on the same side is one trade: only the change in shares pays the spread. A sign change closes the old trade with reason `flip` and opens a new one; the spread on the closing shares goes to the old trade and the spread on the opening shares goes to the new one. A held name with no print cannot be traded. Its target stays in force, and it trades at the next open that exists. If shares are still open on the last session, they are flattened at that session's close, or at the last marked price if that session has no bar, with reason `end_of_sample`. If equity is not positive at a rebalance, flatten and open nothing, reason `insolvent`.
5. **Borrow and costs.** At the start of a session, before the gap, each short that was held at the previous mark pays `notional × 0.05 × calendar days since the previous session / 365`. The spread is 20 bp of `|change in shares| × fill price`. Costs are deducted from equity and assigned to the trade.
6. **Mark.** On a next-open rebalance, the old book is marked from the previous close to the open, then the new book is traded, then the new book is marked from the open to the close. On any other day the book is marked from the previous close to the close, unless a delayed trade fills at the open, in which case the gap to that open is marked first.
7. **Daily return.** Simple return of equity from the previous session's mark to this session's mark, on every session from the first entry date through the last session. A session with no position is 0. Sessions before the first entry date are warm-up and are not in the return series. The account starts at 1.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Signal close and the 63-session volatility and dollar volume | Close of the signal session (16:00 ET, or the early close) | The next session's open, or later |
| Formation base close | Close of week W − F | The next session's open |
| Fill price | That session's open | The fill itself |
| A daily close | 16:00 ET even though `ts` is 09:30 | No trade decision uses a close before the next session |
| Cross-sectional rank | The same signal close | The same next open |
| Borrow notional | Previous mark | The open of the next session, before the gap |
| Quintile edges of the SPY breakdown | Full sample, after the run | The breakdown only. Not a trading input |
| Screen market cap and revenue CAGR | The September 2026 snapshot | Choosing the candidate list only. Not a trading input |

The fixed candidate list is survivorship. The rolling filters are not.

## Samples

- **Warm-up.** Sessions before the first primary entry date. The first entry needs a valid signal: 63 sessions of history and a prior ISO week.
- **Out-of-sample.** Evaluation sessions on or after 2024-01-02, through 2026-09-25.
- **In-sample.** Evaluation sessions before 2024-01-02.
- **Why this split.** 2024-01-02 is the first session of a calendar year and puts the most recent years in the out-of-sample window. It is not the 2024-07-01 date used by the QQQ studies. The equity path of the overlapping months is still partly known, as disclosed above. Daily metrics are split by the session date. A trade is counted in the sample that contains its entry date. A position that crosses the boundary contributes its later days to the out-of-sample daily return and its trade statistics to the in-sample trade count.

## Benchmarks

- **Equal weight.** Uncosted, daily-rebalanced, close to close, of the primary names that have a close on both that session and the previous one. Missing names are left out of that day's average. No day uses a forward fill of a missing close into the benchmark return.
- **SPY.** Uncosted close to close on the same sessions. A missing SPY print is a zero benchmark return for that day.

The strategy is dollar-neutral. Beating SPY is not an acceptance line.

## Secondary candidate

**Long-only losers.** Same formation, same filters, same weeks, same costs, same primary names. The long quintile only, weights `+1 / q`, no shorts and therefore no borrow. Its own line, which does not replace a failed primary: out-of-sample Sharpe ≥ 0.5, out-of-sample profit factor ≥ 1.10, and at least 100 out-of-sample trades, at the base 20 bp.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at the base cost, for the primary, the secondary, and the cross-market book: the protocol defaults. Sharpe is the mean daily simple return divided by the sample standard deviation, times `√252`, with a zero rate. Flat days are 0. Profit factor is the sum of positive position-level net returns divided by the absolute sum of negative ones. Position-level net return is `(price P&L − spread − borrow) / dollars deployed into the trade`. Average net trade is that mean, in basis points. Exposure is the share of evaluation sessions with a non-zero position at the close.
- Breakdowns: calendar year; long versus short; exit reason; quintile of the same-day SPY return. Quintile edges use the full sample and are not a filter.
- Costs: multiplier 0, 0.5, 1, 2, and 3 on both the 20 bp and the 5% borrow. Fill delay: the open of the second session of the entry week and of the exit week. A week with fewer than two sessions does not take that signal. Upper bound, labelled as such: enter at the signal close and exit at the next week's last close.
- Direction placebo on gross price P&L, sizes held at the base-cost share counts: 2,000 draws, seed `20260926`. `p = (1 + count of draws whose Sharpe is at least the actual gross Sharpe) / 2001`.
- Timing placebo: on each signal week, the same eligible set and the same `q`, assigned at random instead of by the formation return. 500 draws, seed `20260928`, scored on net Sharpe. This replaces a random clock time because the decision time is the week boundary.
- Block bootstrap: 20-session circular blocks, 2,000 draws, seed `20260927`, 95% interval of the full-sample net Sharpe.
- Plateau grid on the in-sample window: formation weeks `{1, 2, 4}` × `K ∈ {4, 5, 8, 10}`, 12 cells, the primary among them. Every cell is scored on the primary book's evaluation dates, with a flat zero before that cell can signal. Out-of-sample Sharpes are shown and nothing is selected.
- Cross-market: the odd-ticker book, identical primary rules.
- Verification: the synthetic self-test, and `verify.py` replaying the primary trades.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 20 bp per side and the 5% borrow:

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 2× the base spread and 2× the borrow.
5. Out-of-sample Sharpe > 0 on the cross-market book under identical rules.
6. At least 100 out-of-sample trades. Below that, the verdict is **Inconclusive**, including when other lines fail.

No line is changed from the protocol defaults. The pre-lock slot count is large enough that 100 trades is the right bar, so it was not lowered.

A failed line fails the strategy. If the out-of-sample trade count is under 100, the status is Inconclusive rather than Rejected.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone; the long-only secondary; a single symbol; dropping the BDCs, REITs, or banks; a dollar-volume or volatility band other than the one above; a different cost, borrow, fill, or sample split; a result read off the price-change columns of the screen.
