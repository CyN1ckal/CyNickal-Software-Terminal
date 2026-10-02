# QQQ intraday trend following: pre-registered rules

Written 2026-09-26, before any of these rules was run on the store. Results may not edit this file. Anything added after the first run goes in the report under "post hoc" and is labelled that way.

## Data

- QQQ 1-minute regular-hours bars from `data/market-data.sqlite`, read with `agent-data/mdq.py`: split-adjusted, 09:30–15:59 ET, and 09:30–13:00 on early closes. QQQ has no splits in the store.
- Minute k of a session is the bar whose open is 09:30 + k minutes. `open_d` is the open of the first bar. `close_d` is the close of the last bar.
- The prior close is the last minute close of the previous NYSE session. A session whose previous NYSE session has no minute data (2022-01-03, because 2021-12-31 is missing) is not traded and counts as a zero-return day.
- Within a session, a missing minute takes the previous minute's close. A fill that falls on a missing minute uses the next bar that exists.

## Candidate P (primary): noise-boundary momentum

Published by Zarattini, Aziz & Barbon (2024), "Beat the Market: An Effective Intraday Momentum Strategy for S&P500 ETF (SPY)". Its defaults are used unchanged:

1. **Noise area.** `move(d, k) = |close(d, k) / open_d − 1|`. `σ(d, k)` is the mean of `move` over the previous L = 14 sessions that have minute k.
   `UB(d, k) = max(open_d, prevclose_d) · (1 + V·σ(d, k))` and `LB(d, k) = min(open_d, prevclose_d) · (1 − V·σ(d, k))`, with V = 1.
2. **VWAP.** Session-cumulative Σ(typical · volume) / Σ volume, where typical = (high + low + close) / 3, through minute k.
3. **Decisions** are made only at the half-hour marks, on the close of minutes 29, 59, …, 359 (information known at 10:00, 10:30, …, 15:30). The target is:
   - long if `close > max(UB, VWAP)`;
   - short if `close < min(LB, VWAP)`;
   - flat otherwise.

   So the VWAP/band level is a trailing stop, checked only at the marks.
4. **Fills.** A change of target fills at the **open of the next minute** (10:00, 10:30, …). Positions still open are closed at the session's last close. Nothing is held overnight. A mark with no later bar in the session (on an early close) is skipped.
5. **Sizing.**
   - P1 (signal test): notional = 1× equity at entry.
   - P2 (deployment): leverage = min(2, 2% / σ14), where σ14 is the sample standard deviation of the previous 14 daily close-to-close returns, set at the session open and held all day. The paper caps at 4×; 2× is chosen here for prudence, before any result is seen.
6. **Costs.** 1 bp of traded notional per side, which scales with leverage. No borrow cost and no interest on cash.

## Candidate S (secondary): market intraday momentum

From Gao, Han, Li & Zhou (2018, JFE). `r1 = close(minute 29) / prevclose − 1`. Buy (sell) at the open of the last 30 minutes (minute 360, or 180 on an early close) if r1 > 0 (< 0), and exit at the session's last close. Sizing is 1×, with 1 bp per side.

## Samples

- Warm-up: the first 15 sessions with data.
- **In-sample (IS):** the first evaluable session through 2024-06-28.
- **Out-of-sample (OOS):** 2024-07-01 through 2026-09-25.

Defaults are not tuned. The grid below is run on IS to check that the defaults sit on a plateau, not a spike. The whole grid is shown on OOS only as a check of selection bias.

## Reported checks

- Daily-return Sharpe (zero rate, √252, zero-return flat days kept), CAGR, max drawdown, volatility, trades, win rate, profit factor, average net trade in bps, exposure, and t-stat of mean daily return.
- Split by year and by long/short side, P&L by entry mark, and behaviour against QQQ open-to-close quintiles.
- Cost sensitivity at 0, 0.5, 1, 2, and 3 bp per side, and a one-minute extra fill delay.
- Placebo: for P1, the same entry and exit times with the trade direction randomized, 2,000 draws; p = the share of draws whose Sharpe ≥ actual.
- Block-bootstrap 95% CI of the full-sample Sharpe (20-day blocks, 2,000 draws).
- Cross-market: identical P1 rules on SPY and IGV 1-minute data.
- Grid on IS: L ∈ {7, 10, 14, 20, 30}, V ∈ {0.5, 0.75, 1.0, 1.25, 1.5}, decision spacing ∈ {15, 30, 60} minutes.
- Benchmarks: QQQ buy-and-hold (close to close), and QQQ long every day open to close.

## Acceptance (decided now)

P is proposed for paper trading only if **all** of these hold for P1 at 1 bp:

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Placebo p ≤ 0.05 on the full sample.
3. IS Sharpe > 0, and at least 60% of the IS grid has Sharpe > 0.
4. Positive total return at 2 bp per side on the full sample.

If P fails, the report says so. It does not swap in a grid winner.
