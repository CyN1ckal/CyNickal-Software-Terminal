# Treasury ETF time-series momentum: pre-registered rules

Written 2026-10-02, before any return, P&L, forward return, or outcome statistic was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `research/treasury-etf-trend/research/counts.py` only. No price, return, sign, or P&L was printed.
  - The store lists 174 instruments. Of the treasury-like names checked (TLT, IEF, SHY, SHV, BIL, TIP, LQD, HYG, AGG, BND, GOVT, TLH, IEI, VGIT, VGLT, SGOV, TBT, TMV, TMF, ZN, ZB, ZF, ZT), only TLT and IEF are present.
  - TLT and IEF each have 3,959 daily bars, 2011-01-04 through 2026-10-01, no duplicate sessions, no stored splits, and no stored dividends. Each has 0 one-minute bars. SPY has the same 3,959 daily sessions and no stored corporate actions. SPY has one-minute bars; TLT and IEF do not.
  - `mdq.nyse_sessions` includes 2012-10-29, 2012-10-30, and 2018-12-05. Coverage marks those three dates `missing` with 0 bars for TLT, IEF, and SPY. NYSE was closed (Sandy; the national day of mourning). They are skipped. No other book session is missing a TLT or IEF bar against the SPY daily calendar. Daily coverage records 0 unrecorded sessions inside the range.
  - Month-ends on that SPY calendar: 190. Month-ends on which each fund has at least 252 prior own daily bars: 178. That is a count of eligible dates, not a count of long versus short signals.
- **Earlier studies on the same instruments, periods, or mechanism.** None used TLT or IEF. The overlapping mechanism is `micro-futures-trend` (Rejected): a 1/3/12-month blend, volatility-targeted, traded with whole micro contracts on equity indices, Treasury futures (ZT, ZF, ZN, ZB, modelled as micro yields), and commodities, in one book, over roughly 2022-10 through 2026-09. This study does not repeat that blend, that volatility target, that contract rounding, or that multi-asset book. The other studies in `research/` are equity intraday or daily rules, small-cap gap fades, or portfolios of those rules. They do not trade duration.
- **What that futures report already told me about rates.** The OOS window here is not unseen. `micro-futures-trend` OOS is 2024-10 through 2026-09, which sits inside this study's OOS (2024-07-01 through 2026-10-01). Its rates sleeve, the 1/3/12 vol-targeted book on 2Y/5Y/10Y/30Y, had IS Sharpe +0.10 and OOS Sharpe −1.12, and lost $12,676 net over the full test (Sharpe −0.42). In sample the sleeve made +$865. Out of sample it lost $13,541 and did most of the book's damage. By market, full / OOS Sharpe was ZT −0.21 / −0.62, ZF −0.23 / −0.53, ZN −0.42 / −1.03, ZB −0.61 / −1.49. Rates were held every session and flipped often (16–21 flips in 48 rebalances; that flip count is marked post hoc in that report). The loss was larger in the longer-duration contracts. I did not read that study's `research/data`, `daily.csv`, or `trades.csv`, and I have not computed a TLT or IEF return. I therefore do not know these ETFs' 12-month signs. I do know that a related time-series-momentum implementation on Treasury futures lost money on the overlapping window, worst in long duration. The futures rules file, written before that run, also stated as the author's prior knowledge that Treasury yields rose into late 2023 and then ranged. That sentence is prior knowledge, not a measured path in the report.
- **What the equity studies already told me about the same calendar.** Their OOS window, mostly 2024-07-01 through 2026-09-25, was a rising equity market: QQQ buy-and-hold about +55% (Sharpe about 1.0, drawdown about −23%), SPY about +42% (Sharpe about 1.0, drawdown about −19%). It contains the April 2025 tariff crash and the +10.5% SPY session on 2025-04-09. The in-sample window contains the 2022 bear market. `spy-rsi2-dip-buy` also found that stored SPY daily bars from late 2024 are not regular-hours prints (the 2025-04-02 daily close is the after-hours tariff print). This study still uses stored daily bars. TLT and IEF have no one-minute bars, so that check cannot be repeated for them, and the SPY benchmark is not rebuilt from minutes. Mixing a minute-built SPY with daily-bar TLT and IEF would compare two print conventions. The defect is inherited by the SPY close-to-close series and is not a hurdle.
- **Where the parameters came from.** The user specified the universe, the single 252-session lookback, the fixed 1/2 weights, the monthly next-open fill, the 1 bp cost, the sample split, and the acceptance edits below, and asked for the study to be locked and run. The 252-session lookback is the 12-month leg in Moskowitz, Ooi, and Pedersen (2012). It was fixed before any return on these series was computed. The 21-session and 63-session legs are not added, including after reading the futures report.

## Hypothesis

TLT and IEF continue in the direction of their own prior 252-session price return over the next month.

**Mechanism.** Time-series momentum in duration, the 12-month leg of Moskowitz, Ooi, and Pedersen (2012). The other side is a hedger, a mortgage-convexity desk, or a rebalancer selling into a rally and buying into a decline. Those flows are mandate-driven, so they do not stop because the trade has started to hurt.

**Known counter-forces.** Range-bound yields and V-shaped reversals whipsaw a monthly trend rule. The April 2025 tariff shock is already known to be one such reversal in equities; the report states the book's compounded result on the book sessions in that calendar month. Published-anomaly decay (McLean and Pontiff 2016) can shrink a 2012 result. Missing coupons, stated below, push measured long P&L down and measured short P&L up. The futures study is itself evidence that a cousin of this rule lost money on Treasury futures in the overlapping window. None of these edits the rule.

## Predictions beyond P&L

If the mechanism is right, then:

1. TLT and IEF have the same sign of full-sample gross P&L. Gross P&L is the sum of that fund's gap plus open-to-close dollar P&L on the base-cost share path over the full evaluation window, costs not subtracted. A zero sum is not the same sign as a positive or a negative sum.
2. In calendar months where SPY close-to-close is in its worst quintile, the strategy's mean monthly return is above its mean in the middle quintile. That is the crisis-convexity claim. It is not an acceptance line.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- Instruments: TLT and IEF. SPY is the month-end clock and a reported close-to-close series, not a traded leg. Read with `agent-data/mdq.py`, split-adjusted. Neither fund has a stored split. Dividends are not stored.
- No dividends are stored. Coupons and ETF distributions are missing, so a long is understated and a short is overstated by about the distribution yield times exposure. Price volatility of these two funds is large relative to a year's coupon, which is why the universe is these two and not a short-Treasury fund. There is no coupon adjustment.
- Bars: stored daily bars. A daily bar's `ts` is 09:30 ET. Its close is known at 16:00 ET, or 13:00 ET on an early close. No coarser bar is built. One-minute bars are not used.
- Calendar: the book is the SPY daily-bar sessions from 2011-01-04 through 2026-10-01, excluding 2012-10-29, 2012-10-30, and 2018-12-05 even if a bar were present. Those three dates have no bars. Early closes stay in the book; the daily open and close are the session prints, with no separate early-close rule.
- Missing bars: a fund-session with no bar earns 0 that session and is not forward-filled into the signal. The next session does not book the gap across the hole. See Primary rule.
- Data checks the script must pass before it writes results: TLT and IEF each have 3,959 daily bars, first session 2011-01-04, last session 2026-10-01, every open and close positive, and `corporate_actions` empty. If a check fails, the script exits without writing `results.json`, `daily.csv`, or `trades.csv`.

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| `LOOKBACK` | 252 sessions | Moskowitz, Ooi, and Pedersen (2012), the 12-month leg. One lookback. Not a blend. |
| `WEIGHT` | 1/2 | Two-fund book. The denominator stays 2 when one fund is flat. Not rescaled. Not volatility-scaled. |
| `COST_BPS` | 1 per side | Skill default for SPY-class liquidity. TLT and IEF are that class. Not changed after P&L. |
| `SKIP` | 2012-10-29, 2012-10-30, 2018-12-05 | NYSE closures that `nyse_sessions` still lists. |
| Seeds | direction 20261051, bootstrap 20261052, timing 20261053, verify 20261054 | Fixed. |

1. **Signal dates.** The last book session of each calendar month. A book session is a session with a SPY daily bar, excluding `SKIP`.
2. **Signal.** On signal date `t`, for each fund, using that fund's own daily bars in session order, with no forward-filled close: if `t` is not one of the fund's bars, or the fund has fewer than 252 bars strictly before `t`, the target weight is 0. Otherwise let `c0` be the close 252 own bars before `t` and `c1` the close on `t`. The signal is `sign(c1 / c0 - 1)`, and `sign(0) = 0`. The target weight is the signal divided by 2, so `+1/2`, `-1/2`, or 0. The previous target is not carried. A weight of 0 is a real order to be flat.
3. **Position.** Each fund is independent. Shares are constant between fills. There is no stop, no target, and no time exit. A same-sign rebalance resizes the open round trip. A round trip is one fund from an entry (shares leaving 0, or a sign flip) until the shares go flat or the sign flips. The sample-end mark below closes any trip still open; it is not a traded exit.
4. **Exits.** The only exits are a later fill to flat, a later fill that flips the sign, or the end-of-sample mark. Nothing is checked intraday.
5. **Fills.** The fill session is the next book session after the signal date. The fill price is that session's open. If no later book session exists, the signal is not filled and shares do not change. If the fund has no bar on the fill session, that fund is not traded, its pending target expires, its shares stay where they are, and that session earns 0. Sizing uses mark-to-open equity before cost, the same equity for both funds.
6. **Sizing and accounting.** Equity starts at 1, flat, in cash. On each book session, in order:
   - Old shares earn the gap into the open only if this fund had a bar on the previous book session and has a bar today: `shares * (open - previous close)`. Otherwise the gap contribution is 0. A missing bar therefore earns 0 and does not leak into the next gap.
   - Mark-to-open equity is the previous close equity plus the booked gaps.
   - If today is a fill session, each fund that has a bar is set to `target_shares = target_weight * mark_to_open_equity / open`. Both targets use equity before either trade and before cost. Cost is `COST_BPS / 10000` times the absolute notional traded, `|new_shares - old_shares| * open`, summed across funds, and is deducted once. A flip's absolute share change covers both legs, so both legs are charged.
   - New shares earn the open-to-close: `new_shares * (close - open)` if the fund has a bar, else 0.
   - Close equity is mark-to-open equity minus cost plus open-to-close. The session return is close equity divided by previous close equity, minus 1.
   - Fractional shares are allowed. Borrow is 0. Cash interest is 0. Short proceeds are not paid interest. No coupon is booked.
7. **End of sample.** The last book session is marked at its close. Open trips are recorded with exit price equal to that close, exit reason `end_of_sample`, and no extra exit cost. A signal whose fill would fall after the last session does not trade.
8. **Round-trip dollars.** Gap dollars before a fill belong to the old trip. The open-to-close belongs to the shares held after the fill. On a same-sign resize the whole cost stays on that trip. On a flatten, the whole cost stays on the old trip. On a flip, the old trip pays `abs(old_shares) * open * rate` and the new trip pays `abs(new_shares) * open * rate`. Gross dollars omit cost. Net dollars subtract it. Entry price is the open of the entry fill. Exit price is the open of the flat or flip fill, or the last close at end of sample. Holding sessions equal the number of book sessions from the entry session inclusive through the last session whose close the trip still held.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Fund close on signal date `t` | 16:00 ET on `t` (13:00 on an early close) | Target for the next session's open. Not used to size or fill on `t`. |
| Close 252 own bars before `t` | The close of that earlier session | The signal on `t` |
| Fill-session open | 09:30 ET on the fill session | The fill and the mark-to-open equity, both at the open, before the close exists |
| Fill-session close | 16:00 ET on the fill session | Open-to-close P&L after the fill. Not an input to the target or the size. |
| SPY bar present on a session | When that daily bar is stored; the month's last session is known once the session has occurred | Which day is the signal date. The SPY close is not an input to the target. |
| Quintile edges and the placebo draws | After the whole sample | Reported checks only. Not inputs to any share quantity. |

No full-sample z-score, quantile, or volatility target enters the primary. The grid does not choose the lookback.

## Samples

- Warm-up: the first 252 own daily bars. No eligible signal exists before a fund has 252 prior own closes.
- **In-sample:** the first fill session of the primary (the book session after the first eligible month-end) through the last book session strictly before 2024-07-01.
- **Out-of-sample:** book sessions from 2024-07-01 through 2026-10-01, inclusive.
- The split date is the one this repo already uses, extended through the last daily bar in the store rather than the last one-minute bar. The OOS window is not unseen. See Prior exposure. Evaluation-window metrics use only the sessions inside the window. Flat days inside the window count as 0. A round trip is in-sample or out-of-sample by its entry fill date, not by its exit date. Daily returns stay in the window that contains the session.

## Benchmarks

- Equal-weight long-only of TLT and IEF. Same fill dates as the primary (fills of eligible month-ends, including a month when the primary's target is flat). Target weight `+1/2` each fund on every such fill, same next-open accounting, cost 0. It does not trade before the primary's first fill. Not a hurdle for the verdict.
- SPY close-to-close on the same evaluation sessions: today's stored daily close divided by the previous book session's stored daily close, minus 1. A missing SPY bar would earn 0. SPY is not the hurdle. The late-2024 extended-hours defect in stored SPY daily bars is inherited and not repaired.

## Secondary candidates

None. TLT and IEF are the two legs of one book, not a cross-market pair. There is no second primary.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at 1 bp per side: the protocol defaults. Sharpe is the mean daily simple return divided by its sample standard deviation (divisor n−1), times √252, with a zero risk-free rate. CAGR uses a 252-session year. Total return, volatility, max drawdown, and the t-stat are computed on the window's daily returns compounded from 1 at the first session of that window. Profit factor is the sum of winning round trips' net dollars divided by the absolute sum of losing round trips' net dollars. A trip with net dollars of 0 is neither. Win rate is the share of trips with net dollars greater than 0. Average net trade, in bp, is the mean over trips of net dollars divided by mark-to-open equity at that trip's entry, times 10,000. Exposure is the share of window sessions with a non-zero share count at the close. Undefined Sharpe (fewer than 2 sessions or zero standard deviation) or undefined profit factor does not pass a line that needs the number.
- The same windows for the long-only benchmark and for SPY close-to-close.
- April 2025: compounded strategy, benchmark, and SPY returns on the book sessions whose dates fall in that calendar month. This is the pre-registered V-shaped-reversal check. It is not an acceptance line.
- Breakdowns: calendar year; long versus short trips; exit reason (`flat`, `flip`, `end_of_sample`); quintile of the month's SPY close-to-close (defined under predictions); quintile of the session's SPY close-to-close. Quintile assignment ranks the full evaluation window, sorts ascending, and splits into five contiguous groups as equal as possible, with any extra members assigned to the worst groups first. The worst group is quintile 1. The middle group is quintile 3. Quintiles are not a filter.
- Costs: 0, 0.5, 1, 2, and 3 bp per side, which is 0, 0.5×, 1×, 2×, and 3× the base. The share path is rebuilt at each cost, because size depends on equity.
- Fill delay: the same signals filled at the open of the book session after the next one (signal at `t`, fill at `t+2` book sessions). Upper bound, labelled as such and not a candidate: fill at the signal session's close. On that close the old shares first earn the prior close to this close, equity is marked, shares are reset to `weight * equity / close`, cost is charged on the absolute notional at the close, and the new shares earn nothing further that day.
- Direction placebo: on the zero-cost book's round trips, keep each trip's dates and gross dollar P&L and multiply the whole trip by an independent ±1. 2,000 draws, seed 20261051. The daily placebo return is the summed flipped dollars divided by the unflipped zero-cost equity at the previous close. Compare that draw's full-sample gross Sharpe with the actual zero-cost full-sample Sharpe. `p = (1 + count of draws with Sharpe ≥ actual) / 2001`.
- Timing placebo: 500 draws, seed 20261053. Independently permute each fund's signal across eligible rebalance dates, preserving its count of positive, negative, and zero signals. Ineligible dates stay at weight 0. Rebuild the zero-cost book. Gross Sharpe on the primary's evaluation dates. Same p formula with 501.
- Block bootstrap: circular 20-session blocks of the full-sample net daily returns, 2,000 draws, seed 20261052. Report the 2.5th and 97.5th percentiles of the Sharpe.
- Plateau grid on IS: `LOOKBACK ∈ {126, 189, 252, 315, 378}`. Weights, cost, and fill stay fixed. Five cells. Each cell's in-sample window starts at that cell's own first fill. Out-of-sample is the same 2024-07-01 through 2026-10-01 dates, shown for selection bias only. Nothing is selected from the grid. These cells are not a 1/3/12 blend and they are not extra signal legs.
- Cross-market: not run. Line 5 is not applicable. No third Treasury ETF is in the store. The futures rates series were already used in `micro-futures-trend` and are not a line-5 instrument. This was recorded before the lock.
- Verification: the self-test below, before the store is opened, and `verify.py` on every rebalance, which is more than 40. Seed 20261054 is recorded for that script.

### Self-test, before the store

Hand-built sessions, lookback 1 so the arithmetic is small. The engine's lookback is a parameter; the locked primary is still 252. The script aborts if any case fails.

- Both long, 1 bp. Two funds. January month-end up 20 percent, fill at the next open, still held at a sample that ends on the fill day. The fill day's own month-end signal must not trade, because no next session exists.
- Both short, 1 bp. The same shape with a down signal.
- One flat, 1 bp. One fund's lookback return is exactly 0, so its weight is 0. The other is long at 1/2, not rescaled to 1.
- A flip, 0 cost and a separate 1 bp one-fund flip. Old trip exits at the flip open. New trip starts there. Costs of the two legs are split as specified.
- A missing bar. A hole does not book a gap on the next session. A missing signal-date bar sets that fund's next target to 0 and does not carry the old signal.
- End of sample. Covered by the both-long case: no phantom close fill.

## Acceptance

The primary is a **paper-trading candidate** only if every scored line holds at 1 bp per side:

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. IS Sharpe > 0, and at least 60% of the IS grid cells have Sharpe > 0 (at least 3 of 5).
4. Full-sample total return > 0 at 2× base cost (2 bp per side).
5. Not applicable. Not scored. No third Treasury ETF is in the store, and the futures rates series were already used in `micro-futures-trend`. Recorded before the lock.
6. At least 24 OOS round trips, counted by entry fill on or after 2024-07-01. A round trip is one fund from entry until flat or flip, and a trip still open at the last close counts as one trip with reason `end_of_sample`.

Changes from the protocol defaults, with reasons fixed before the lock: line 5 is not scored, for the reason in that line. Line 6 is lowered from 100 trades to 24 round trips because two funds rebalanced monthly over 27 month-ends cannot produce 100 independent round trips when a trend persists. No other line is lowered.

Status, applied mechanically:

- If line 6 fails, the status is **Inconclusive**, even if another scored line also fails.
- If line 6 passes and any of lines 1, 2, 3, or 4 fails, the status is **Rejected**.
- If lines 1, 2, 3, 4, and 6 all pass, the status is **Paper-trading candidate**.

A failed scored line fails the strategy. A secondary does not exist and cannot be promoted.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone; TLT alone or IEF alone; a regime filter; a different cost, fill, or sample split; a short-Treasury leg; a blend of the 21-session, 63-session, and 252-session lookbacks; a volatility target; a coupon adjustment fit after the results; ZN or ZB futures as the primary or as the line-5 instrument.

## References

- Moskowitz, T. J., Ooi, Y. H., and Pedersen, L. H. (2012). Time series momentum. *Journal of Financial Economics*, 104(2), 228–250.
- McLean, R. D., and Pontiff, J. (2016). Does academic research destroy stock return predictability? *Journal of Finance*, 71(1), 5–32.
