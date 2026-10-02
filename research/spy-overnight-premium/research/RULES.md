# SPY overnight premium: pre-registered rules

Written 2026-10-02, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `research/spy-overnight-premium/research/counts.py`, output `counts.json`. Coverage rows and `corporate_action` rows only. `bars()` was not called. No price, return, or P&L was read. For SPY, QQQ, and IWM, each symbol has 3,959 daily bars, first session 2011-01-04, last session 2026-10-01, no duplicate dates, and no daily bar outside that window. `nyse_sessions` over the same window has 3,962 dates. The three dates in the calendar but not in the bars are 2012-10-29, 2012-10-30 (Hurricane Sandy), and 2018-12-05 (national day of mourning). Those three are `coverage` status `missing` with 0 bars. 2025-01-09 is also `missing` with 0 bars and is already a holiday in `mdq` (`SPECIAL_CLOSURES`), so it is not in `nyse_sessions`. 2021-12-31 has a daily bar; it is the only `partial` daily row in the window (the terminal calendar treats that Friday as a holiday; mdq treats the bar as good). Each symbol has 149 zero-bar coverage rows in the window, 4,108 coverage rows in total, 4,103 `complete`, and `sessions_not_recorded` empty inside the stored span. `corporate_action` is empty for all three: no splits, no dividends, no other types.
- **Earlier studies on the same instruments, periods, or mechanism:** Reports were read. `trades.csv` and `daily.csv` were not. The overlapping studies, and what they found, are listed in the next bullet. None of them is this rule. This rule is long every night and flat through the cash session. It holds only the gap between the stored daily close and the next stored daily open. Studies that are flat at the close do not measure that gap. `spy-rsi2-dip-buy` holds SPY for days, so its P&L mixes gaps and cash sessions. `qqq-15m-turtle-overnight` holds some QQQ overnight gaps chosen by a channel rule on 15-minute bars from 2021-10, which is not every night and not this daily series. `index-opening-pop-fade` S1 uses the prior close in its signal and still covers at the same day's close.
- **What I already know about the test windows:** The out-of-sample window 2024-07-01 through 2026-09-25 was used by the earlier studies below. It is not unseen. This study's out-of-sample window runs four sessions further, through 2026-10-01. Those four sessions were not in the earlier samples. The rest of the out-of-sample window was. From those reports, over 2024-07-01 through 2026-09-25: SPY buy and hold was about +41.7% (Sharpe about 1.03, max drawdown about −19%) and QQQ buy and hold was about +55.4% (Sharpe about 0.97 to 1.01, max drawdown about −23% to −24%). April 2025 was a crash and a rebound. SPY rose about 10.5% on 2025-04-09 and QQQ about 12% the same day, after the tariff pause. 2022 is inside this study's in-sample and was a down year for both (QQQ about −33.5% in 2022 in the portfolio study). I do not know the close-to-open versus open-to-close split on this store, including on the 2011–2021 daily path, which no earlier study in this repo used. The post-2021 path has been used, mostly as a cash-session sample or as a multi-day hold. What those studies reported, in one line each:
  - `qqq-intraday-trend`: QQQ, flat every night. Out-of-sample +17.4%, Sharpe 0.82. The same rules on SPY were negative out of sample (Sharpe −0.36).
  - `intraday-channel-trend`: 15-minute channel on QQQ, SPY, and IGV, flat overnight. Out-of-sample book −12.5%, Sharpe −0.56.
  - `qqq-15m-turtle-overnight`: Turtle on QQQ 15-minute bars, positions carried overnight. Out-of-sample +0.49%, Sharpe 0.09. The gaps that book happened to hold summed to a loss; the cash session made the gains. SPY under the same rules had out-of-sample Sharpe −0.48. That is not a measurement of every SPY night on the daily open and close.
  - `qqq-atr-scale-in`, `qqq-bollinger-adding`, `qqq-atr-band-dip-eod`: QQQ intraday, flat by the close. All three lost money out of sample (Sharpes −0.57, −0.89, −0.40).
  - `qqq-atr-martingale`: QQQ fade that can be held overnight until it covers its cost. Out-of-sample Sharpe 1.02. SPY under the same rules had out-of-sample Sharpe 0.33.
  - `spy-rsi2-dip-buy`: SPY bought and sold at the close, held for days. Out-of-sample +33.8%, Sharpe 1.29, 32 trades, against SPY buy and hold +41.7%. It also reported that stored daily bars from 2024-11-20 onward differed from the regular-hours minute print, including an after-hours close on 2025-04-02. This study does not re-check that before the lock and does not switch to the minute print.
  - `index-opening-pop-fade`: short SPY from 10:00 to the close. Out-of-sample −11.96%, Sharpe −0.66. QQQ out-of-sample Sharpe −1.27.
  - `igv-small-account-fade`: flat overnight. Out-of-sample Sharpe −3.82.
  - `qqq-strategy-portfolio` and `qqq-return-stack`: combinations of the earlier daily return series. QQQ buy and hold in those windows was about +55.4% out of sample and about +99% to +107% from late 2021 through 2026-09-25. They did not isolate the overnight gap.
  - `low-liq-high-vol-mean-reversion`, `small-cap-gap-up-fade`, and `finviz-gap-up-fade` are not SPY or QQQ strategies. Their reports quote a SPY price path over 2016–2026-09-25 (full-sample Sharpe about 0.49, and about 1.20 from 2024-01). That is a different sample and still not the gap.
  - `micro-futures-trend` does not trade SPY shares.
- **Where the parameters came from:** The hypothesis, the every-night rule, the 2011-01-04 start, the 2024-07-01 split, the 1.0 notional, the QQQ line, and the IWM report were fixed by the request for this study before any overnight return was computed. The cost is the protocol default for SPY, 1 bp per side, unchanged. The notional grid is the protocol plateau around the primary, because the rule has no lookback to vary. The house split date matches the earlier index studies. It was not moved.

## Hypothesis

SPY's close-to-next-open return, every session, is positive after cost, and that gap is where the equity premium in this test sits.

**Mechanism.** The long side holds inventory from the cash close to the next cash open and is paid for overnight risk. The other side is a trader who will not hold inventory across the close (a day trader, an intraday desk, or a market maker who flattens) and who buys the open back. The flow that has to trade at the cash open is the other half of the same gap (Lou, Polk, and Skouras 2019; Cooper, Cliff, and Gulen 2008). There is no signal and no filter. If the premium is the overnight hold itself, it should show up on the average night, not on a selected subset.

**Known counter-forces.** Cash dividends are not in the store. An ex-dividend price drop lands entirely in the overnight price return and is not offset by cash, so it pulls this strategy down and does not pull the open-to-close benchmark down. The dates cannot be split out: `corporate_action` is empty, and this study will not import an outside dividend calendar. From late 2024 the stored daily open and close may not be the regular-hours auction prints (`spy-rsi2-dip-buy`); this study uses them anyway. A long-every-night book is also long into weekend, holiday, and event gaps, including the April 2025 window already seen in earlier reports. The report states these. It does not adjust for them and it does not drop those nights.

## Predictions beyond P&L

If the mechanism is right, then:

1. The full-sample mean close-to-open simple return exceeds the full-sample mean open-to-close simple return. Both means are equal-weighted across the same evaluation sessions, uncosted, and are not Sharpe ratios. Close-to-open on exit session *t* is `open[t] / close[t-1] - 1`. Open-to-close is `close[t] / open[t] - 1`.
2. The average net strategy return is positive in at least 8 of the calendar years that contain at least 200 evaluation sessions. The average is the arithmetic mean of `strategy_net` in that year, not the compound return.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. A pass of the acceptance table with a failed prediction has not confirmed the mechanism.

## Data

- Instruments: SPY (primary), QQQ (cross-market acceptance line), IWM (reported under the same rules, not a second primary). Read with `agent-data/mdq.py`, `bars(symbol, "1d", start="2011-01-04", end="2026-10-01")`, split adjustment left on. There are no splits, so adjustment changes nothing. Dividends are not adjusted because they are not stored.
- Bars: stored daily bars only. Do not read the 1-minute tape for the strategy, the benchmarks, the grid, the placebos, or the delayed path. The 1-minute history starts in 2021-09 and is a different series from the daily open and close. A daily bar's `ts` is 09:30 America/New_York. Its close is not known at 09:30.
- Calendar: the session list for a symbol is the dates of its daily bars in the window, sorted ascending. Do not insert a calendar day that has no bar. Do not hold a position across a missing session as if that session's close existed. The trade is from a real close to the next real open, one trade, one cost.
- Named gaps: no SPY, QQQ, or IWM daily bar on 2012-10-29, 2012-10-30, 2018-12-05, or 2025-01-09. 2021-12-31 is a session and is included. Early closes are included. They still have a daily open and a daily close. `mdq.EARLY_CLOSES` lists early closes from 2019 only. A pre-2019 early close, including the day after Thanksgiving and Christmas Eve in 2011–2018, is stamped 16:00 in `trades.csv`. The fill price is still that session's daily close. The stamp does not change any return.
- Missing bars: leave them missing. No forward fill.
- Data checks the script must pass before it writes results: each of SPY, QQQ, and IWM has exactly 3,959 daily bars in the window; the first date is 2011-01-04 and the last is 2026-10-01; the three date lists are identical; no date is duplicated; 2012-10-29, 2012-10-30, 2018-12-05, and 2025-01-09 are absent; 2021-12-31 is present; every open, high, low, and close is strictly positive; `high + 1e-6 >= max(open, close)` and `low - 1e-6 <= min(open, close)`. A failure aborts with no `results.json`, `daily.csv`, or `trades.csv`. The run log records the abort.

## Primary rule

Parameters, all fixed:

- `NOTIONAL = 1.0` (fraction of equity at the entry close). Source: the request. The grid does not replace it.
- `COST_BPS_SIDE = 1.0` (protocol default for SPY). Round trip = 2 bp of notional.
- `FIRST = 2011-01-04`, `LAST = 2026-10-01`, `OOS_START = 2024-07-01`.
- No lookback, no threshold, no filter.

Let `s[0] .. s[N-1]` be the symbol's daily bars in date order. `N` is 3,959 for each name unless the data check aborts.

1. **State.** There is no indicator. The book is long every overnight interval between two consecutive stored daily bars, and flat from each open until that session's close.
2. **Signal.** None. The decision does not use the open, the high, the low, the close, or the volume.
3. **Position.** One long position. Enter at every close that has a later daily bar. Exit at the next daily bar's open. The exit session's close is a new entry when a later bar exists. The first bar is an entry only. The last bar is an exit only. No short, no flip, no second unit, no skip.
4. **Exits.** The only exit is the next session's open. Exit reason `next_open`. No stop, no target, no time stop inside the night, no session filter.
5. **Fills.** Buy at the entry session's daily close (market on close). Sell at the next session's daily open (market on open). Entry clock time is 16:00 America/New_York, or 13:00 when the entry date is in `mdq.EARLY_CLOSES`. Exit clock time is 09:30 America/New_York. `trades.csv` writes `YYYY-MM-DD HH:MM` with no offset. There is no earlier fill than the close, so there is no same-bar upper bound.
6. **Sizing.** Notional is `NOTIONAL` times equity at the entry close. Equity starts at 1. After a trade, `equity *= 1 + net_on_equity`. Cash earns no interest. The open-to-close move is not applied. Because the book is flat from the open to the close, equity at the next entry equals equity just after the previous exit.
7. **Costs.** `cost_side = COST_BPS_SIDE / 10000`. For the primary, `cost_side = 0.0001`. Charged on notional, both sides, every night, including a zero gap. `gross_on_notional = open[i+1] / close[i] - 1`. `net_on_equity = NOTIONAL * (gross_on_notional - 2 * cost_side)`. At `NOTIONAL = 1`, that is `open[t] / close[t-1] - 1 - 0.0002`, booked on the exit date. No borrow (there is no short). No extra cost on a weekend: Friday close to Monday open is one trade and one round trip.

`daily.csv` has one row per evaluation session, which is every session except `s[0]`. Columns, in order: `date`, `strategy_net`, `strategy_gross`, `bh_c2c`, `open_to_close`, `delayed_net`, `equity`. `strategy_gross = NOTIONAL * gross_on_notional`. `equity` is equity after `strategy_net`. `strategy_net` on a session with no trade does not occur in this rule.

`trades.csv` is the primary only (`NOTIONAL = 1`, 1 bp per side). One row per night. Columns: `side`, `entry_time`, `entry_px`, `exit_time`, `exit_px`, `gross`, `net`, `exit_reason`. `side` is `long`. `gross` is `gross_on_notional`. `net` is `net_on_equity`. `exit_reason` is `next_open`.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Decision to be long | Before the close. It does not depend on any price. | The close of every session that has a following daily bar |
| Daily close | The closing auction, 16:00 ET, or 13:00 on an early close. The daily bar's `ts` is 09:30 and is not the knowledge time. | Fill price of the market-on-close entry. Not a signal. |
| Daily open | The next session's opening auction, 09:30 ET | Fill price of the exit |
| High, low, volume | Not used by the rule | Data check only |
| Any rolling statistic, z-score, quantile, or volatility target | Not used | Not used |
| Full-sample quintile bins | Computed after the run for a table | Not a trading input |
| Universe membership | Fixed as SPY, QQQ, and IWM before the run | The whole window |
| Dividend cash amount | Not in the store | Not used. Not imputed. |

A signal that used the close would fill no earlier than the next open. This rule has no such signal. The close is the fill. That is look-ahead only if the close is used to decide. It is not.

## Samples

- Warm-up: none. The first trade needs `s[0]`'s close and `s[1]`'s open. The first evaluation session is `s[1]`.
- **In-sample:** the first evaluation session through the last evaluation session before 2024-07-01. A trade is in-sample when its exit date is strictly before 2024-07-01.
- **Out-of-sample:** 2024-07-01 through 2026-10-01, by exit date. The trade that enters at the 2024-06-28 close and leaves at the 2024-07-01 open is out of sample. The last session, 2026-10-01, is an exit and not an entry.
- The split is the house date used by the earlier index studies. Their out-of-sample window through 2026-09-25 is not unseen. See prior exposure. The 2011–2021 daily path was not a sample in those studies.

Slice metrics restart equity at 1 on the first session of the slice. They do not inherit the other slice's equity level. In-sample and out-of-sample trade lists are the trades whose exit dates fall in the slice. Profit factor uses dollar P&L compounded from equity 1 inside that same slice.

## Benchmarks

Uncosted. Same evaluation sessions. Notional 1. Both are compounded from equity 1 on the slice being reported.

- SPY close-to-close buy and hold: on exit session `s[k]`, `close[k] / close[k-1] - 1`. Over the full sample this is the price change from the first close to the last close. It omits dividends, as the strategy does.
- SPY open-to-close, the cash-session complement: on `s[k]`, `close[k] / open[k] - 1`. The strategy's contribution on that cash session is 0.

QQQ and IWM use their own close-to-close and open-to-close series the same way. The acceptance table does not use a benchmark Sharpe.

## Secondary candidates

None. The delayed path below is a check. It has no acceptance line and cannot be promoted.

## Delayed fill

Pre-registered. Not the primary.

The primary entry is the close. There is no earlier print in this rule to call an upper bound.

One session later, the entry is the next open and the exit is the open after that. With bars `s[0] .. s[N-1]`, the delayed trades are, for `j = 1 .. N-2`:

- Buy at `open[j]` at 09:30 of `s[j]`. Sell at `open[j+1]` at 09:30 of `s[j+1]`.
- `gross = open[j+1] / open[j] - 1`.
- `delayed_net = 1.0 * (gross - 2 * 0.0001)`, booked on `s[j+1]`.
- The first evaluation session, `s[1]`, has `delayed_net = 0`.
- The last primary trade, close of `s[N-2]` to open of `s[N-1]`, has no delayed counterpart. There is no open after the last bar.

That is `N - 2` delayed trades. The held interval is open to the next open, so it includes a cash session and the following overnight. It is a different trade. Metrics for it are reported on the `delayed_net` column, including the leading zero, for the full sample, in-sample, and out-of-sample. It is not written to `trades.csv`. It is not an acceptance input.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample, at base cost and `NOTIONAL = 1`: the protocol defaults. Sharpe is the mean of the daily simple returns divided by their sample standard deviation (`ddof = 1`) times √252, with a zero risk-free rate. If the sample standard deviation is 0, the Sharpe is 0. CAGR uses a 252-session year. A non-positive ending equity leaves CAGR blank. Max drawdown is the minimum of compounded equity over its running peak, minus 1. The t-stat is the mean daily return divided by its standard error. Profit factor is the sum of positive dollar P&L divided by the absolute sum of negative dollar P&L inside the slice; a zero-net trade is in neither sum; if there are no losing trades and there is a winner, profit factor is recorded as null and line 1 treats it as passed. Win rate is the share of trades with `net > 0`. Average net trade is the mean of trade `net` in basis points. Exposure is 1 on the evaluation sessions (a position every night) and 0 during the cash session. Both are stated.
- Also stored, because the report may not do its own arithmetic: trade count, trades per 252 sessions, median and mean holding time in hours from the stamped entry clock to the stamped exit clock, average winner and average loser in basis points, count of longs, count of shorts, count of each exit reason.
- Breakdowns: calendar year of the exit date (sessions, compound strategy return, Sharpe, max drawdown, mean `strategy_net`, compound close-to-close, compound open-to-close); long versus short; exit reason; weekday of the exit date (Monday's row is the weekend gap when the previous bar was a Friday); quintile of the exit session's close-to-close return. Quintile method: sort evaluation sessions by `bh_c2c` ascending, then by date ascending, and assign quintile `floor(rank * 5 / n) + 1` with rank starting at 0. Quintile 1 is the most negative close-to-close moves. For each quintile store the count and the mean of `strategy_net`, `bh_c2c`, and `open_to_close`. None of these breakdowns is a filter.
- Ex-dividend nights: not testable. The store has no dividend rows for these three symbols. No outside calendar is added.
- Costs: multipliers 0, 0.5, 1, 2, and 3 times the base 1 bp per side, at `NOTIONAL = 1`. `net_on_equity = gross_on_notional - 2 * (m * 0.0001)`. Store full-sample Sharpe, out-of-sample Sharpe, and full-sample total return at each multiplier.
- Fill delay: the delayed path above. No upper bound.
- Direction placebo on gross returns: 2,000 draws, seed `20261041`. `rng = numpy.random.default_rng(20261041)`. `signs = rng.choice(numpy.array([-1.0, 1.0]), size=(2000, n))` with `n` the full-sample evaluation count, in that order, one call. Draw `d` multiplies the full-sample `strategy_gross` vector, at `NOTIONAL = 1`, by `signs[d]`. Compare each draw's Sharpe to the actual full-sample gross Sharpe. `p = (1 + count of draws with Sharpe >= actual) / 2001`. Store the null mean and the 95th percentile (`numpy.percentile`, 95).
- Timing placebo: not applicable, and not run. Every evaluation session is already a trade, with the same exit (the next open). A random-entry draw with the same count and the same exit is the same set of trades. There is no non-trade evaluation session to draw from.
- Block bootstrap: circular blocks of 20 evaluation sessions, 2,000 draws, seed `20261042`, on the full-sample `strategy_net` vector. `rng = numpy.random.default_rng(20261042)`. `n_blocks = ceil(n / 20)`. `starts = rng.integers(0, n, size=(2000, n_blocks))`, one call. Draw `d` concatenates, in order of `starts[d]`, the blocks `r[(start + k) mod n]` for `k = 0 .. 19`, then keeps the first `n` values. Store the 2.5, 50, and 97.5 percentiles of those Sharpes (`numpy.percentile`).
- Plateau grid on in-sample Sharpe: `NOTIONAL` in `{0.5, 0.75, 1.0, 1.25, 1.5}`. Cost stays 1 bp per side and scales with notional: `net_on_equity = f * (gross_on_notional - 0.0002)`. Five cells. Out-of-sample Sharpe of each cell is stored and is not used to select. The primary remains 1.0. Nothing is selected from the grid.
- Cross-market: the same rules on QQQ. IWM is reported on the same rules and does not satisfy line 5 if QQQ fails it. Also store the Pearson correlation of the three `strategy_net` series. The date lists are identical under the data check, so the correlation has no missing-session alignment choice.
- Verification: the self-test below, before the store is opened. `verify.py` re-implements the trades without importing `backtest.py` and matches every SPY night on side, entry time, entry price, exit time, and exit price, and matches `strategy_net` on every evaluation session. It also draws 40 exit indices with `numpy.random.default_rng(20261043)` and checks those explicitly. Prices match when `math.isclose` is true at relative tolerance `1e-9` and absolute tolerance `1e-6`. A mismatch is a bug.

### Self-test, before the store

Hand-built sessions. No database. The script aborts if any case fails.

1. **Normal night.** Monday 2024-06-03 close 100 (open 99, high 100, low 99) and Tuesday 2024-06-04 open 101, close 102 (high 102, low 101). One trade. Entry `2024-06-03 16:00` at 100. Exit `2024-06-04 09:30` at 101. Gross `0.01`. Net `0.0098`. Reason `next_open`. Side `long`. `strategy_net` on 2024-06-04 is `0.0098`. Close-to-close is `0.02`. Open-to-close is `102/101 - 1`. 2024-06-03 is not a row.
2. **Missing sessions skipped.** 2012-10-26 close 50 and 2012-10-31 open 51, with no row on 2012-10-29 or 2012-10-30. One trade, not two or three. Entry `2012-10-26 16:00` at 50. Exit `2012-10-31 09:30` at 51. Gross `0.02`. Net `0.0198`. A second pair, 2018-12-04 close 80 to 2018-12-06 open 80, is also one trade, net `-0.0002`, and does not invent 2018-12-05.
3. **Last sample night.** Three sessions A, B, and C. Trades are A close to B open, and B close to C open. C's close is not an entry. There is no row after C. Trade count is 2.
4. **Cost on both sides.** A night with open equal to the prior close has gross 0 and net `-0.0002`, not `-0.0001` and not 0. The same flat gap at notional 1.5 has net `-0.0003`.
5. **Delayed path is not the primary.** Session D1 open 90 close 100, D2 open 110 close 110, D3 open 111 close 111. The first primary trade enters at 100 and exits at 110, not at 90. `delayed_net` on D2 is 0. `delayed_net` on D3 is `111/110 - 1 - 0.0002`. The primary entry price is not D1's open.
6. **Early-close stamp.** Entry on 2024-11-29, which is in `mdq.EARLY_CLOSES`, is stamped `2024-11-29 13:00`. The price is still that session's daily close. The next bar 2024-12-02 exits at `2024-12-02 09:30`.
7. **Sample cut.** An exit on 2024-06-28 is in-sample. An exit on 2024-07-01 is out-of-sample.
8. **Bad bar.** A session with `high < close` fails the data check. The check is pure and does not need the store.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 1 bp per side and `NOTIONAL = 1` on SPY. If line 6 fails, the status is **Inconclusive**, not Rejected and not a candidate. If line 6 holds and any other line fails, the status is **Rejected**. If every line holds, the status is **Paper-trading candidate**.

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0. That is at least 3 of the 5 notionals. A cell at exactly 0 does not count.
4. Full-sample total return > 0 at 2× base cost per side (2 bp per side, 4 bp per night).
5. Out-of-sample Sharpe > 0 on QQQ under identical rules. IWM is not this line. A positive IWM Sharpe does not pass line 5 if QQQ's is not positive.
6. At least 100 out-of-sample trades. The floor is not lowered. This rule trades almost every night, so the floor is not the constraint the rule was written around.

No line differs from the protocol default except line 5, which names QQQ alone. The reason is the request: IWM is reported and does not replace QQQ.

A failed line fails the strategy.

## Not done in this study

The report will not promote any of these in place of the primary: a grid notional; QQQ or IWM as the primary; the delayed open-to-open path; a long-only subset of an already long-only rule; a short side; a trend filter; dropping ex-dividend nights; trading only the turn of the month; a different cost; a different sample split; the 15:59 minute print in place of the daily close; an FOMC filter; a volatility gate; an earnings skip. A breakdown that looks better than the full book is a description, not a new rule.
