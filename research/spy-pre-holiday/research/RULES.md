# SPY pre-holiday: pre-registered rules

Written 2026-10-02, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `research/spy-pre-holiday/research/counts.py`, output `counts.json`. Coverage rows, `corporate_action` rows, and the NYSE calendar only. `bars()` was not called. No price, return, or P&L was read. For SPY, QQQ, and IWM, each symbol has 3,959 daily bars, first session 2011-01-04, last session 2026-10-01, no duplicate dates, and no daily bar that is not an NYSE session. `nyse_sessions` over the same window has 3,962 dates. The three dates in the calendar but not in the bars are 2012-10-29, 2012-10-30 (Hurricane Sandy), and 2018-12-05 (national day of mourning). Those three are coverage status `missing` with 0 bars. 2025-01-09 is also `missing` with 0 bars and is already a holiday in `mdq` (`SPECIAL_CLOSURES`), so it is not in `nyse_sessions`. 2021-12-31 has a daily bar; it is the only `partial` daily row in the window. Each symbol has 149 zero-bar coverage rows, 4,108 coverage rows, and 4,103 `complete`. `corporate_action` is empty for all three: no splits and no dividends. Applying `is_pre_holiday` to the bar dates, with no prices, gives 146 pre-holiday sessions, 123 with date before 2024-07-01 and 23 with date from 2024-07-01 through 2026-10-01. The first is 2011-01-14. The last is 2026-09-04. No NYSE pre-holiday in the window lacks a bar. The three symbols share those dates. Eight of the 146 are in `mdq.EARLY_CLOSES`: 2019-07-03, 2019-12-24, 2020-12-24, 2023-07-03, 2024-07-03, 2024-12-24, 2025-07-03, 2025-12-24. If the evaluation window starts at 2011-01-14, it has 3,951 sessions: 3,385 before 2024-07-01 and 566 from 2024-07-01 through 2026-10-01.
- **Earlier studies on the same instruments, periods, or mechanism:** Reports were read, including `spy-overnight-premium` `REPORT.md` and `RULES.md`. That study's `daily.csv` and `trades.csv` were not read. None of the earlier rules is this rule. This rule is long the SPY cash session on the last session before a full-day NYSE closure, and flat overnight and flat on every other session. `spy-overnight-premium` is the opposite holding period: long every night, flat through the cash session. `spy-rsi2-dip-buy` holds SPY for days, so its P&L mixes gaps and cash sessions. `index-opening-pop-fade` is short SPY from 10:00 to the close on selected days, flat overnight. The QQQ intraday studies are flat by the close. `qqq-atr-martingale` can hold QQQ overnight until a fade covers its cost. `spdr-sector-momentum` is a monthly long/short sector book. No earlier study in this repo tested the pre-holiday cash session.
- **What I already know about the test windows:** The out-of-sample window 2024-07-01 through 2026-10-01 was used by `spy-overnight-premium`. It is not unseen. From that report, over those same sessions, uncosted SPY close-to-close returned +40.52% (Sharpe 0.985, max drawdown −19.88%) and uncosted SPY open-to-close returned +25.14% (Sharpe 0.709, max drawdown −18.22%). The every-night gap, after 1 bp per side, returned +0.27% (Sharpe 0.053, profit factor 1.003) and was Rejected. The full-sample open-to-close path in that study, which starts one week earlier than this evaluation window, returned +139.3% (Sharpe 0.489). So the average cash session on this store was positive, in sample and out of sample, in a rising market. The pre-holiday subset of that cash session was not reported. It is the object of this study. I do not know whether pre-holiday open-to-close returns exceed ordinary-session open-to-close returns. From the same report and from the other equity reports opened for this file: April 2025 was a crash and a rebound, and SPY rose about 10.5% on 2025-04-09. 2022 is inside this study's in-sample and was a down year. `spy-rsi2-dip-buy`, through 2026-09-25, returned +33.8% out of sample (Sharpe 1.29) against SPY buy and hold +41.7% (Sharpe 1.03). `qqq-intraday-trend` was flat overnight and returned +17.4% out of sample on QQQ (Sharpe 0.82); the same rules on SPY had out-of-sample Sharpe −0.36. `index-opening-pop-fade` lost 11.96% out of sample (Sharpe −0.66). `qqq-atr-scale-in` lost 8.81% out of sample (Sharpe −0.57). `qqq-atr-martingale` had out-of-sample Sharpe 1.02 on QQQ, and QQQ buy and hold was about +55.4% out of sample in that window. `igv-small-account-fade` had out-of-sample Sharpe −3.82. `spdr-sector-momentum` returned +6.2% out of sample (Sharpe 0.253) against SPY buy and hold +40.5% (Sharpe 0.985) through 2026-10-01. `spy-rsi2-dip-buy` also reported that stored daily bars from 2024-11-20 onward can differ from the regular-hours minute print. This study uses the stored daily open and daily close anyway.
- **Where the parameters came from:** The hypothesis, the pre-holiday definition, the open-to-close hold, the 2011-01-04 start, the 2024-07-01 split, the 1.0 notional, the QQQ line, the IWM report, and the 15-trade out-of-sample floor were fixed by the request before any holiday return was computed. The parent did not compute returns. The cost is the protocol default for SPY, 1 bp per side, unchanged. The notional grid is the protocol plateau around the primary. The counted 23 out-of-sample pre-holiday sessions did not change the floor. The floor stays 15.

## Hypothesis

SPY's open-to-close return on the last session before a full-day NYSE closure is positive after cost, and higher than the open-to-close return on ordinary sessions.

**Mechanism.** The pre-holiday effect (Ariel 1990; Lakonishok and Smidt 1988). The other side is a trader who flattens before a closure and buys the cash session back only after the holiday. The hold is the regular-hours session, from the open to the close. It is not the overnight gap.

**Known counter-forces.** The average cash session on this store, including ordinary days, was already positive in the out-of-sample window. A long-only cash-session book can look good because the market rose, which is not the same claim as a pre-holiday premium. Early closes are not full-day closures; a session is a trade only when the next weekday is a full-day holiday, even if that session itself closes at 13:00. Weekends are not holidays: a Friday before an ordinary Monday is not a trade. Cash dividends are not in the open-to-close move. `corporate_action` is empty, so no dividend cash is added, and this book is flat overnight, so an ex-dividend gap is not held. The stored daily open and close are used even where they may differ from the 09:30 and 15:59 prints. The report states these. It does not adjust for them and it does not drop those sessions.

## Predictions beyond P&L

If the mechanism is right, then:

1. The mean gross open-to-close simple return on pre-holiday sessions is higher than the mean gross open-to-close simple return on all other SPY sessions in the evaluation window. Both means are equal-weighted, uncosted, and are not Sharpe ratios. Open-to-close is `close[t] / open[t] - 1`.
2. That inequality also holds inside the out-of-sample window alone. The two groups are the out-of-sample pre-holiday sessions and the other out-of-sample evaluation sessions.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. A pass of the acceptance table with a failed prediction has not confirmed the mechanism.

## Data

- Instruments: SPY (primary), QQQ (cross-market acceptance line), IWM (reported under the same rules, not a second primary and not a substitute for QQQ). Read with `agent-data/mdq.py`, `bars(symbol, "1d", start="2011-01-04", end="2026-10-01")`, split adjustment left on. There are no splits, so adjustment changes nothing. Dividends are not stored and are not in the open-to-close move.
- Bars: stored daily bars only. Do not read the 1-minute tape for the strategy, the benchmarks, the grid, the placebos, or the cost sweep. A daily bar's `ts` is 09:30 America/New_York. Its close is not known at 09:30.
- Calendar: a session is tradable when it is an NYSE session (`weekday < 5` and not `mdq.is_nyse_holiday`) and the symbol has a daily bar that day. Do not insert a row for a calendar day with no bar. Do not treat a weekend as a holiday. Do not treat an early close as a holiday.
- Named gaps: no SPY, QQQ, or IWM daily bar on 2012-10-29, 2012-10-30, 2018-12-05, or 2025-01-09. 2021-12-31 is a session and is included. It is not a pre-holiday: the next weekday is 2022-01-03, and that Monday is not a holiday. Early closes stay in the sample. They are trades only when `is_pre_holiday` is true.
- Missing bars: leave them missing. No forward fill. None of the 146 pre-holiday sessions is one of the missing dates.
- Sample end: 2026-10-01. Do not use a later bar.
- Data checks the script must pass before it writes results: each of SPY, QQQ, and IWM has exactly 3,959 daily bars; the first date is 2011-01-04 and the last is 2026-10-01; the three date lists are identical; no date is duplicated; 2012-10-29, 2012-10-30, 2018-12-05, and 2025-01-09 are absent; 2021-12-31 is present; every bar date is an NYSE session; every open, high, low, and close is strictly positive; `high + 1e-6 >= max(open, close)` and `low - 1e-6 <= min(open, close)`; `corporate_action` is empty for all three. The pre-holiday bar counts must be 146 full, 123 before 2024-07-01, and 23 from 2024-07-01 on, and the first pre-holiday bar must be 2011-01-14. A failure aborts with no `results.json`, `daily.csv`, or `trades.csv`. The run log records the abort.

## Primary rule

Parameters, all fixed:

- `NOTIONAL = 1.0` (fraction of equity at the open). Source: the request. The grid does not replace it.
- `COST_BPS_SIDE = 1.0` (protocol default for SPY). Round trip = 2 bp of notional on a pre-holiday session. Source: the request, unchanged after any P&L.
- `FIRST = 2011-01-04`, `LAST = 2026-10-01`, `OOS_START = 2024-07-01`.
- No lookback, no threshold, no filter other than the holiday calendar.

```
def is_pre_holiday(session):
    nxt = session + timedelta(days=1)
    while nxt.weekday() >= 5:
        nxt += timedelta(days=1)
    return is_nyse_holiday(nxt)
```

`is_nyse_holiday` is `mdq.is_nyse_holiday`. It already returns false for weekends. It includes the standard full-day closures, Juneteenth from 2022, and `SPECIAL_CLOSURES` (2025-01-09). It does not include early closes.

A trade happens only when session `t` is an NYSE session, the symbol has a daily bar that day, and `is_pre_holiday(t)` is true.

Let the symbol's daily bars in the window be sorted by date. The first evaluation session is the first of those bars for which `is_pre_holiday` is true. For these three symbols that date is 2011-01-14. Evaluation sessions are every later bar through 2026-10-01, including 2011-01-14, including ordinary sessions. That is 3,951 sessions. Bars before 2011-01-14 are not evaluation sessions. The close of the bar immediately before 2011-01-14 is used only as the prior close of the close-to-close benchmark on 2011-01-14. There is no warm-up lookback.

1. **State.** There is no indicator. The book is long the cash session on a pre-holiday evaluation session and flat otherwise. It is flat overnight.
2. **Signal.** The calendar only. The decision does not use the open, the high, the low, the close, or the volume. It uses the date and `is_nyse_holiday` of the next weekday.
3. **Position.** One long position on a pre-holiday session. Enter at that session's open. Exit at that session's close. No short, no flip, no second unit, no position on any other session, no position carried through the close.
4. **Exits.** The only exit is the same session's daily close. Exit reason `session_close`. No stop, no target, no overnight hold.
5. **Fills.** Buy at the daily open (market on open). Sell at the daily close (market on close). Entry clock time is 09:30 America/New_York. Exit clock time is 13:00 America/New_York when the session is in `mdq.EARLY_CLOSES`, and 16:00 otherwise. `trades.csv` writes `YYYY-MM-DD HH:MM` with no offset. `mdq.EARLY_CLOSES` starts in 2019. A pre-2019 early close that is a pre-holiday is still a trade. Its stamp is 16:00. The fill price is still that session's daily close. The stamp does not change any return. Holding time for the report is 3.5 hours when the stamp is 13:00 and 6.5 hours when the stamp is 16:00.
6. **Sizing.** Notional is `NOTIONAL` times equity at the open. Equity starts at 1 before the first evaluation session. After each evaluation session, `equity *= 1 + strategy_net`. Cash earns no interest. Because the book is flat overnight and flat on ordinary sessions, equity changes only on pre-holiday sessions.
7. **Costs.** `cost_side = COST_BPS_SIDE / 10000`. For the primary, `cost_side = 0.0001`. Charged on notional, both sides, on a pre-holiday session only. `gross = close[t] / open[t] - 1`. `strategy_gross = NOTIONAL * gross` on a pre-holiday session and 0 otherwise. `strategy_net = NOTIONAL * (gross - 2 * cost_side)` on a pre-holiday session and 0 otherwise. At `NOTIONAL = 1` and base cost, that is `close[t] / open[t] - 1 - 0.0002` on a pre-holiday session. A flat open-to-close still pays 2 bp. No borrow. No cost on a session that is not a trade.

`daily.csv` has one row per SPY evaluation session. Columns, in order: `date`, `strategy_net`, `strategy_gross`, `open_to_close`, `close_to_close`, `traded`, `equity`. `open_to_close` is the uncosted SPY `close/open - 1` on every evaluation session. `close_to_close` is the uncosted SPY `close / previous_stored_close - 1`, where the previous stored close is the previous daily bar in the window, including a bar before the evaluation window. `traded` is 1 or 0. `equity` is equity after `strategy_net`.

`trades.csv` is the SPY primary only (`NOTIONAL = 1`, 1 bp per side). One row per pre-holiday session. Columns: `side`, `entry_time`, `entry_px`, `exit_time`, `exit_px`, `gross`, `net`, `exit_reason`. `side` is `long`. `gross` is `close/open - 1`. `net` is `strategy_net`. `exit_reason` is `session_close`.

The five notional cells scale this formula. They are not written to `trades.csv`.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Next weekday's holiday status | The published NYSE calendar, before the open. For 2025-01-09, `mdq` records the closure and this study treats that flag as known before the 2025-01-08 open. The study does not model an announcement lag. | The decision to buy the open of session t |
| Daily open | The opening auction, 09:30 ET | Fill price. Not a signal. |
| Daily close | The closing auction, 16:00 ET, or 13:00 on a date in `EARLY_CLOSES`. The daily bar's `ts` is 09:30 and is not the knowledge time. | Fill price of the exit. Not a signal. |
| High, low, volume | Not used by the rule | Data check only |
| Any rolling statistic, z-score, quantile, or volatility target | Not used | Not used |
| Quintile bins of the close-to-close move | After the run, from the evaluation sample | The breakdown table only. Not a filter. |

A one-session fill delay is not run. The position is the session itself. Entering one session later would be the session after the holiday, or an ordinary session, which this study does not test. There is no same-bar-close entry: buying the close would hold the overnight gap, which is the other study.

## Samples

- Warm-up: none. No indicator.
- Bars from 2011-01-04 through 2011-01-13 are loaded and are not evaluation sessions.
- **In-sample:** 2011-01-14 through the last evaluation session strictly before 2024-07-01. 3,385 sessions, of which 123 are pre-holiday trades. The count is a calendar fact.
- **Out-of-sample:** 2024-07-01 through 2026-10-01. 566 sessions, of which 23 are pre-holiday trades. The count is a calendar fact from `counts.py`. It was not used to move the split or the floor.
- The split date matches the earlier index studies. The out-of-sample window was already used by `spy-overnight-premium`, including the open-to-close and close-to-close benchmarks. The pre-holiday subset was not.

Sharpe, CAGR, volatility, drawdown, and the t-stat use every evaluation session in the slice, including the zeros on ordinary days. The acceptance Sharpe is that daily series. The mean return per pre-holiday trade is also reported, so the zeros do not hide the per-trade number. The per-trade mean is not a substitute for the acceptance Sharpe.

Slice metrics restart equity at 1 on the first session of the slice. They do not inherit the other slice's equity. In-sample trades are pre-holiday sessions with date before 2024-07-01. Out-of-sample trades are the rest.

## Benchmarks

Uncosted, on the same SPY evaluation sessions:

- SPY open-to-close, every session: `close[t] / open[t] - 1`.
- SPY close-to-close: `close[t] / close[t_prev] - 1`, previous stored daily close.

No cost and no notional scaling on either benchmark. QQQ and IWM books also record their own open-to-close and close-to-close paths in `results.json`. Those paths are not the acceptance benchmarks. The charted benchmarks are the two SPY series.

## Secondary candidates

None.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample, at base cost and `NOTIONAL = 1`, for the strategy and both SPY benchmarks: total return, CAGR on a 252-session year, annualized volatility, Sharpe, max drawdown, t-stat, trades, win rate, profit factor, average net trade in bp, exposure. Also the mean gross open-to-close per pre-holiday trade, in bp.
- Sharpe is the mean of the daily simple returns divided by their sample standard deviation (`ddof = 1`) times √252, with a zero risk-free rate. If the sample standard deviation is 0, the Sharpe is 0. CAGR uses a 252-session year: `ending_equity ** (252 / n_sessions) - 1`. A non-positive ending equity leaves CAGR blank. Max drawdown is the minimum of compounded equity over its running peak, minus 1, with equity starting at 1. The t-stat is the mean daily return divided by its standard error (`sd / sqrt(n)` with `ddof = 1`).
- Profit factor is the sum of positive dollar P&L divided by the absolute sum of negative dollar P&L. Inside a slice, equity restarts at 1 and trades are applied in date order. Dollar P&L of a trade is equity before the trade times `net`. A zero-net trade is in neither sum. If there is a winner and no loser, profit factor is null and line 1 treats the profit-factor half as passed. If every trade has zero net, profit factor is 0. Win rate is the share of trades with `net > 0`. Average net trade is the mean of trade `net` in basis points. Exposure is the share of evaluation sessions with a trade.
- The five notionals scale every daily strategy return by the same positive constant, and the zeros stay zero, so the five Sharpe ratios are identical by arithmetic. The in-sample grid test is then all five cells or none. Total return is not identical, because equity compounds. The script still runs all five cells. It does not select one.
- Breakdowns: calendar year; weekday; side; exit reason; quintile of the same-session SPY close-to-close return. Quintiles are five equal-count bins on the full evaluation sample, sessions sorted by `close_to_close` then by date, bin `rank * 5 // n + 1`. The bins are a table, not a filter.
- Costs: multipliers 0, 0.5, 1, 2, and 3 of the base 1 bp per side, at notional 1. Report full-sample Sharpe, out-of-sample Sharpe, and full-sample total return. Break-even cost per side is the bisection, on the full primary sample at notional 1, of the cost in bp where the compound return crosses to less than or equal to 0. Search from 0 to 50 bp. If the return is already less than or equal to 0 at 0 bp, there is no break-even. If it is still positive at 50 bp, record that. Fill delay: not run, for the reason in the look-ahead audit.
- Direction placebo on gross returns: keep each pre-holiday session's place in the daily series and multiply that session's gross strategy return by an independent ±1. Ordinary sessions stay 0. 2,000 draws, seed `20261091`. Compare the actual gross Sharpe of the daily series with the draws. `p = (1 + count of draws with Sharpe >= actual) / 2001`.
- Timing placebo: 500 draws, seed `20261093`. Each draw picks the same number of sessions as there are pre-holiday sessions (146), uniformly without replacement from the evaluation sessions, and puts the uncosted open-to-close long on those sessions and 0 on the rest. Gross Sharpe of that daily series, same p formula with 501. This is reported. It is not an acceptance line.
- Block bootstrap: circular block bootstrap of the daily `strategy_net` series, block length 20, 2,000 draws, seed `20261092`. Report the 2.5 and 97.5 percentiles of the full-sample Sharpe.
- Plateau grid on in-sample: notional in `{0.5, 0.75, 1.0, 1.25, 1.5}` at base cost. Five cells. Out-of-sample Sharpe and return are shown for selection bias only. Nothing is selected. If the five in-sample Sharpes or the five out-of-sample Sharpes have sample standard deviation 0, their rank correlation is undefined. That is not a failure.
- Cross-market: the identical rule on QQQ. IWM is reported and does not replace QQQ. Also report the Pearson correlation of the three daily `strategy_net` series.
- Verification: the self-test below, before the store is opened. `verify.py` shares no signal code with `backtest.py`, recomputes every SPY pre-holiday trade from the store, and matches side, entry time, entry price, exit time, exit price, gross, net, and exit reason. It also matches `strategy_net` on every evaluation session. A seeded sample of 40 evaluation sessions uses seed `20261094`. Matching all trades is the check; the sample is the protocol's explicit draw, and it is redundant if every session matches.

### Self-test

The self-test builds hand prices. It does not open the store. It must reject the run if any case fails.

- `is_pre_holiday(2024-03-28)` is true. Thursday before Good Friday. Not an early close. A trade. Exit stamp 16:00.
- `is_pre_holiday(2024-01-12)` is true. Friday before Monday 2024-01-15. A trade. Exit stamp 16:00.
- `is_pre_holiday(2024-01-05)` is false. An ordinary Friday. Not a trade. `strategy_net` is 0 even if the close differs from the open.
- `is_pre_holiday(2024-11-29)` is false. That Friday is an early close. Not a trade.
- `is_pre_holiday(2024-07-03)` is true. That Wednesday is an early close and the next weekday is a holiday. A trade. Exit stamp 13:00.
- `is_pre_holiday(2022-06-17)` is true. Friday, skipping the weekend, before Monday 2022-06-20.
- `is_pre_holiday(2021-12-31)` is false. A session before the first pre-holiday in the hand-built book is not an evaluation row.
- `is_pre_holiday(2025-01-08)` is true. `is_nyse_holiday(2025-01-09)` is true and that Thursday is not a trade.
- Cost on both sides, notional 1: open 100 and close 101 gives gross `0.01` and net `0.0098`. Open 100 and close 100 gives gross `0` and net `-0.0002`. Open 100 and close 99 gives gross `-0.01` and net `-0.0102`.
- Cost scales with notional: notional 1.5, open 100, close 101, net `0.0147`. Notional 0.5, open 100, close 100, net `-0.0001`.
- The block-index helper and the Sharpe helper match the formulas above on a hand-built series.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 1 bp per side and notional 1. Otherwise the status is **Rejected** when line 6 passes, and **Inconclusive** when line 6 fails. Line 6 failing sets **Inconclusive** even if other lines also fail. **Void** is only for an implementation or data defect that cannot be fixed without changing these rules.

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 2× base cost (2 bp per side).
5. Out-of-sample Sharpe > 0 on QQQ under the identical rule. IWM is not this line.
6. At least 15 out-of-sample trades. The calendar count is 23. The floor stays 15. Below 15, the verdict is **Inconclusive**.

The change from the protocol's 100-trade floor: the rule trades about nine sessions a year, and the out-of-sample window is about 27 months, so a 100-trade bar would require a different calendar rule. The floor was not lowered to the counted 23.

A failed line fails the strategy. The per-trade mean, a grid cell, IWM, a single holiday, and a single year cannot replace a failed line.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; QQQ or IWM as the primary; a single weekday or a single holiday; dropping early closes that are holiday eves; adding early closes that are not holiday eves; treating every Friday as a holiday eve; holding the overnight gap; adding the day after the holiday; a different cost, fill, or sample split; the 1-minute print; a dividend adjustment the store does not have.
