# Intraday channel trend: pre-registered rules

Written 2026-09-26, before any of these rules was run on the store. Results may not edit this file. Anything computed after the first run, if it is kept, goes in the report under "post hoc."

This is a session-local Donchian breakout with a chandelier trail. It is the primary and only candidate. The other checks below explain the result. They are not substitutes if the primary fails.

## Data

- QQQ, SPY, and IGV regular-hours bars from `data/market-data.sqlite`, read with `agent-data/mdq.py`. Prices are split-adjusted. Dividends are not adjusted. The strategy is flat overnight, so it neither earns the dividend nor takes the overnight gap.
- Decisions use 15-minute bars built by `mdq.resample`: buckets start at 09:30 ET, never cross a session, and a bucket with at least one 1-minute print is emitted. A 15-minute bar's timestamp is the bucket open. Its open is the first print inside the bucket, and its close is the last print inside the bucket.
- A missing bucket is left missing. Indicators use the real 15-minute bars in time order. Nothing is forward-filled.
- The book calendar is every New York session from the first through the last on which QQQ has at least one 1-minute bar. A name with no bars that session contributes a zero return. 2021-12-31 and 2025-01-09 have no QQQ minute bars, so they are not book sessions.
- Early closes are the dates in `mdq.EARLY_CLOSES`. On those days the entry cutoff and the session end follow the 13:00 close. QQQ's stored early-close sessions are 211 one-minute bars; that identity is a data check the script requires before it writes results.

## Why fifteen minutes and a two-hour channel

The primary is slow on purpose. A breakout of the prior two hours is a move that has already left the range the stock traded for the last eight 15-minute bars. The fill is the next 15-minute open, and the position is marked only at later closes. That is the trend this study is built to follow. The grid varies the channel length. It does not replace the primary.

## Primary rule

Parameters are fixed: channel length N = 8 bars (two hours), average-true-range length A = N, chandelier multiple K = 2.5, cost = 1 basis point of notional per side.

On each session, independently for each symbol:

1. **True range.** On the session's first real 15-minute bar, TR = high − low. After that, TR = max(high − low, |high − prior close|, |low − prior close|), using the prior real bar in the session. ATR at bar i is the arithmetic mean of the N true ranges ending at i, and it is defined only when that many true ranges exist. A zero ATR blocks a new entry and blocks the stop.
2. **Channel**, evaluated at the close of bar i, for i ≥ N. The upper edge is the highest high of the previous N real bars, excluding bar i. The lower edge is the lowest low of those same bars.
3. **Signal at that close.**
   - Long if close > upper edge.
   - Short if close < lower edge.
   - No breakout otherwise.
4. **Position.** At most one position. A long entry, a short entry, a stop, and a flip are the only changes.
   - Flat and a breakout: enter, if the entry clock allows it.
   - In a position and the opposite breakout: flip, if the entry clock allows the new side.
   - The chandelier is checked first. If it is hit, the position is exited and the same close does not open the other side.
5. **Chandelier**, at the close, using the ATR of that bar. The extreme is the highest high since entry for a long, and the lowest low since entry for a short, including the current bar once its high and low are known. It is not a one-way ratchet: the formula is recomputed every bar, so a larger ATR can move the stop away from price.
   - Long exits if close < extreme − K × ATR.
   - Short exits if close > extreme + K × ATR.
   - The extreme starts at the entry price. On a next-open fill, the fill bar's own high and low are included at that bar's close, because they printed after the fill. The stop can therefore fire on the entry bar.
6. **Fills.** The order is sent for the next real bar whose open time is at least one bar-length after the signal bar's open (the next bucket when the grid is intact). Entries and flips fill at that bar's open, and only when that open is strictly before 15:30 ET, or strictly before 12:30 ET on an early close. Stops fill on that next bar even when the clock is past the cutoff. A stop or a flip that has no later bar left in the session waits for the forced flatten. While an order is waiting, later signals do not replace it. With the primary one-bar delay and an intact grid, the order fills on the next bar, so this waiting rule does not come into play.
7. **Session end.** A position still open is sold or covered at the last real bar's close. If that final close also satisfies the stop inequality, the recorded reason is `stop`. Otherwise it is `eod`. A flip signal on the final bar flattens and does not open a new side. No entry is opened on the final bar.
8. **Sizing.** Each name's notional equals that name's equity at the session open and stays fixed through the session's fills. The name's day return is the sum of its trade returns. A trade return is side × (exit / entry − 1) minus two times the one-side cost, with side +1 for a long and −1 for a short. A flip is two trades: the closing side and the opening side, each carrying its own two-sided cost, which is the four fills of an open, a reverse, and a later exit. Equity compounds across sessions. There is no leverage and no borrow fee. Cash earns nothing.
9. **Book.** The book day return is the equal-weight mean of the three name day returns. A name that is absent or takes no trade contributes zero. Weights are not rescaled when a name is absent.

## Samples

- **In sample:** book sessions from the first QQQ session through 2024-06-28.
- **Out of sample:** book sessions from 2024-07-01 through the last QQQ session in the store.
- There is no cross-session indicator warmup. The first session is tradable. The first close-to-close benchmark day needs a prior close, so that benchmark starts one session later.

The date cut is a calendar split of a September 2021–September 2026 store. It was chosen before the run.

## What is reported

All of the following are part of the locked report. None of them is allowed to replace the primary.

- For the book and for each name, on the in-sample window, the out-of-sample window, and the full window, at 1 bp per side: Sharpe (sample standard deviation, √252, flat sessions kept), total return, CAGR on a 252-session year, annualized volatility, maximum drawdown, t-statistic of the mean daily return, trades, win rate, profit factor, average net trade in basis points, average winner and loser, time in the market, and trades per session.
- Benchmarks on the same sessions, uncosted: equal-weight close-to-close of the last one-minute print, and equal-weight open-to-close of the first one-minute print to the last. A name missing either print is left out of that day's benchmark average and the remaining names are averaged. Correlation of the book with each benchmark, and the correlation of the three names' strategy returns with each other.
- Cost repeats at 0, 0.5, 1, 2, and 3 bp per side. Costs do not change the path.
- Fill repeats: the primary next-bar open; a slower fill at the first real bar at least two bar-lengths after the signal; and an upper bound that trades at the signal bar's close. The upper bound checks the stop on later bars only, so a position is not opened and closed on the same print. Its entry clock is the signal bar's close time, and that time must be strictly before the same cutoff.
- By calendar year, by side, by exit reason (`stop`, `flip`, `eod`), and by the clock hour of the entry.
- Book days split into quintiles of the equal-weight open-to-close move, with the mean strategy return in each quintile.
- Placebo: each completed trade's gross price return is multiplied by an independent ±1, costs stay, and the book is rebuilt. 2,000 draws, NumPy seed 20260926. p = (1 + the number of draws whose Sharpe is at least the actual full-sample Sharpe) / 2001.
- Circular block bootstrap of the full-sample book Sharpe, block length 20 sessions, 2,000 draws, seed 20260927. Report the 2.5th and 97.5th percentiles.
- Grid, same cost and fill as the primary, A = N, N ∈ {4, 6, 8, 12}, K ∈ {1.5, 2.0, 2.5, 3.0, 4.0}. The in-sample grid is the plateau check. The out-of-sample grid is shown and is not used to pick a cell.
- Tape-quality sensitivity: zero out any name-session whose stored one-minute count is below 90% of `rth_minutes` (351 on a full day, 190 on an early close) and rebuild the book. This uses the completed session's bar count, so it is a data check, not a rule that could have been run at 11:45.
- Mechanism: the same entries, flips, clock, and cost, with the chandelier turned off, so a position ends only on an opposite breakout or at the close. This asks whether the trail changed the result. It is not a candidate.
- The ten largest winning trades and the ten largest losing trades, the worst drawdown's dates, and the monthly book returns.

## Acceptance

The primary is proposed for paper trading only if every one of these is true at the locked parameters, next-bar fills, and 1 bp per side:

1. Out-of-sample book Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Placebo p ≤ 0.05 on the full sample.
3. In-sample book Sharpe > 0, and at least 60% of the 20 in-sample grid cells have Sharpe > 0.
4. Full-sample book total return > 0 at 2 bp per side.
5. At least two of QQQ, SPY, and IGV have out-of-sample Sharpe > 0.

A failed item fails the strategy. The report does not promote a grid cell, the long side, the short side, the hold-to-close variant, or a single symbol in its place.
