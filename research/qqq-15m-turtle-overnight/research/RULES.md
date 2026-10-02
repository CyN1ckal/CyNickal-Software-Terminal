# QQQ 15-minute Turtle breakout, held overnight: pre-registered rules

Written 2026-09-26, before any return, P&L, forward return, or hit rate was computed from the store. Results may not edit this file. Anything computed after the first store run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** All counts, no outcome measured.
  - `python agent-data/mdq.py instruments`, `actions`, and `coverage --tf 1m` for QQQ, SPY, and IGV. Each has 1-minute bars on 1,254 sessions, 2021-09-27 → 2026-09-25. 2021-12-31 has no bars (the known terminal-calendar miss). SPY 2026-03-03 has 389 minutes. IGV has 647 `partial` sessions (untraded minutes). QQQ and SPY have **no corporate actions stored, including no dividends**. IGV has one split, 5-for-1 on 2024-03-07, and no dividends stored.
  - `research/qqq-15m-turtle-overnight/research/counts.py`. Each symbol has 32,494 fifteen-minute bars: 1,244 sessions of 26 bars and 10 early-close sessions of 15 bars (the 13:00 bucket holds the one 13:00 minute). 55-bar breakout *onsets* (a close outside the prior 55 bars' high–low range that was not outside on the previous bar, ignoring position): QQQ in-sample 589 long / 457 short, out-of-sample 484 long / 304 short; SPY 572 / 467 and 505 / 291; IGV 541 / 423 and 400 / 300. Up-breakouts outnumbering down-breakouts out of sample is a fact about price direction that I saw. No price after any onset was read.
- **Earlier studies on the same instruments, periods, or mechanism.**
  - `intraday-channel-trend`: a session-local 15-minute Donchian breakout (8 bars) with a 2.5-ATR chandelier, flat every night, equal-weight QQQ/SPY/IGV. **Rejected.** Out-of-sample book Sharpe −0.56, return −12.5%. The average out-of-sample trade made +0.02 bp before costs. Every grid cell lost out of sample. The three names' strategy returns correlated 0.72–0.85. This is the closest prior: the same bar size and the same family of rule. This study differs in three ways fixed below: the channel spans sessions, positions are held overnight, and the exits are the Turtle channel and 2N stop rather than a chandelier. A mechanism very close to this one has already failed on this data, so my prior that this rule works is low.
  - `qqq-intraday-trend`: noise-boundary intraday momentum, flat every night. **Paper-trading candidate** on QQQ: out-of-sample Sharpe 0.82. The same rules lost out of sample on SPY (−0.36) and IGV (−0.38). Profit came from a few large-move days; removing April 2025 cut out-of-sample Sharpe to 0.45. Its Sharpe was about zero from May 2025 on.
  - `qqq-intraday-reversion` (summarised in `igv-small-account-fade`; its folder is not in this layout): fading 4σ 5-minute shocks. Rejected; the average path after a shock kept going.
  - `igv-small-account-fade`: minute fades in IGV. Rejected.
  - `micro-futures-trend`: monthly-rebalanced multi-month trend following on micro futures including MES and MNQ. Rejected; no gross edge.
- **What I already know about the test windows.** The out-of-sample window 2024-07-01 → 2026-09-25 is **not unseen data**. From the reports above I know: QQQ buy-and-hold returned +55.4% on it (Sharpe 1.01, max drawdown −22.9%); it contains the April 2025 tariff crash and the 2025-04-09 +12% rebound; intraday trend following made money on QQQ there but not on SPY or IGV; 15-minute session-local breakouts had no gross edge there on any of the three; QQQ's 20 worst days in 2021–2026 averaged −4.4% close to close. The in-sample window contains 2022, when QQQ fell about a third.
- **Where the parameters came from.** The user asked for a 15-minute-bar trend strategy that holds overnight. The rule is Turtle System 2 as published (Faith 2007; "The Original Turtle Trading Rules", 2003): a 55-bar entry channel, a 20-bar exit channel, and a stop 2N from entry, where N is the 20-bar Wilder-smoothed true range. The bar counts were published for daily bars. No published default exists for 15-minute bars, so the counts are used unchanged on 15-minute bars. That makes the entry channel about 2.1 sessions and the exit channel about 0.8 of a session, so positions naturally carry through the close. **The choice of QQQ as the primary is informed by prior exposure:** QQQ is where intraday trend following survived in `qqq-intraday-trend`. To offset that, the cross-market line below requires SPY specifically to be positive out of sample, not any one of several names.

## Hypothesis

A QQQ close outside the range of the previous 55 fifteen-minute bars (about two sessions) is followed by continuation in the same direction over the next hours to days, including across the overnight gap, large enough that a Turtle-style breakout position held through the close earns more than 1 bp per side.

**Mechanism.** Three documented sources of short-horizon continuation in index products, each with a party on the other side who keeps paying:

- **Price-insensitive hedging flows.** Dealers who are short gamma, and leveraged and inverse ETFs (TQQQ/SQQQ) that rebalance near the close, must buy after rallies and sell after declines. The flows grow with the size of the move and land late in the session (Cheng & Madhavan 2009; Baltussen, Da, Lammers & Martens 2021). A breakout of a two-session range is a move large enough to trigger them. The hedgers pay because they must hedge, not because they forecast.
- **Slow incorporation of information** on days with real news (Gao, Han, Li & Zhou 2018), and time-series momentum from under-reaction at longer horizons (Moskowitz, Ooi & Pedersen 2012). Liquidity providers who fade the move are on the other side.
- **Momentum returns accrue overnight.** Lou, Polk & Skouras (2019) find that the return to momentum portfolios is earned overnight, which they attribute to the clientele that trades at the open. If that carries to index trend, the overnight gap is where a trend position is paid, and a rule that is flat every night (`intraday-channel-trend`) gives that up.

**Known counter-forces.**

- **Overnight drift.** Most of the equity premium in index ETFs has come overnight (Cliff, Cooper & Gulen 2008). A short held overnight pays that drift. Boyarchenko, Larsen & Whelan (2023) find overnight returns are especially high after large intraday declines, which is exactly when a trend rule is short.
- **Short-horizon index reversal.** The closest prior (`intraday-channel-trend`) found no gross edge in 15-minute breakouts out of sample.
- **Leveraged-ETF rebalancing pushes the close and then partly reverts** the next morning, which works against a position carried into the open.
- **Whipsaw and costs.** A channel of about two sessions will break often in range-bound weeks.

The report shows the overnight and intraday parts of the gross P&L separately, by side, so these can be seen.

## Predictions beyond P&L

If the mechanism is right, then:

1. **The overnight hold is paid.** Summed over every overnight the primary holds a position, the gross overnight contribution (side × (next session's first 15-minute open − last close) ÷ entry price) is positive. And the forced-flat variant defined under Reported checks, which is identical except that it is flat at every session's last close, has a lower full-sample gross Sharpe than the primary.
2. **Profit concentrates on large-move days.** Split the evaluation sessions into quintiles of |QQQ close-to-close return|. The primary's mean net daily return is positive in the top quintile and is higher there than in each of the other four.
3. **Trend convexity.** Split sessions into quintiles of *signed* QQQ close-to-close return. The primary's mean net daily return is positive in both the lowest and the highest quintile.

Each one is scored *consistent*, *not consistent*, or *not testable*. A passing P&L with failed predictions has not confirmed the mechanism.

## Data

- **Instruments.** QQQ is the primary. SPY and IGV are cross-market checks under identical rules. Read with `agent-data/mdq.py`, split-adjusted (only IGV has a split; the adjustment divides price by 5 before 2024-03-07 and does not change returns).
- **Dividends are not in the store and are not adjusted.** A long held into an ex-dividend open loses the dividend in the price; a short gains it, though in reality it would pay it. The study cannot correct this from the store. It reports an approximate bias estimate (Reported checks). The primary numbers are price-only.
- **Bars.** 15-minute bars from `MarketData.bars(sym, "15m")`, which resamples stored 1-minute regular-hours bars into buckets anchored at 09:30 ET that never cross a session; a bucket with at least one print is emitted. A bar's `ts` is its bucket open; its open is the first print in the bucket, its close the last. Bars from all sessions are concatenated in time order into **one continuous series**. Indicators run across session boundaries, so the first bar of a session sees the prior session's bars, and its true range includes the overnight gap.
- **Calendar.** Daily returns are on `mdq.nyse_sessions` from the first evaluation session through 2026-09-25. 2021-12-31 is an NYSE session with no bars: its daily return is 0, and the gap from 2021-12-30's close to 2022-01-03's open lands in 2022-01-03. 2025-01-09 is not a session. Early closes (`mdq.EARLY_CLOSES`) have 15 bars; nothing special is done on them.
- **Missing bars.** Not forward-filled. A missing bucket simply does not exist in the series. "Next bar" means the next existing bar.
- **Data checks** the script must pass before it writes results: for each symbol, every session that has bars has 26 fifteen-minute bars, or 15 on an `EARLY_CLOSES` date; no session other than 2021-12-31 in the NYSE calendar between the first and last session lacks bars; bars are strictly increasing in time.

## Primary rule

Parameters, all fixed:

- `BAR = 15m` (the user's idea).
- `N_IN = 55` bars (Turtle System 2 entry channel).
- `N_OUT = 20` bars (Turtle System 2 exit channel).
- `N_ATR = 20` bars (Turtle N).
- `STOP_MULT = 2` (Turtle 2N stop).
- `COST_BPS`: QQQ 1, SPY 1, IGV 2 per side. See Costs.
- `WARMUP = 20` sessions.

Let bars be indexed `i = 0, 1, …` over the continuous series. `H_i, L_i, C_i, O_i` are high, low, close, open.

1. **True range and N.** `TR_0 = H_0 − L_0`. For `i ≥ 1`, `TR_i = max(H_i − L_i, |H_i − C_{i−1}|, |L_i − C_{i−1}|)`, where `i − 1` may be the previous session's last bar. `N_19 = mean(TR_0 … TR_19)`. For `i ≥ 20`, `N_i = (19 × N_{i−1} + TR_i) / 20`. `N_i` is known at the close of bar `i`.
2. **Channels at bar `i`**, excluding bar `i`: `HI55_i = max(H_{i−55} … H_{i−1})`, `LO55_i = min(L_{i−55} … L_{i−1})`, `HI20_i = max(H_{i−20} … H_{i−1})`, `LO20_i = min(L_{i−20} … L_{i−1})`.
3. **Evaluation.** Signals are evaluated at the close of every bar `i` whose session is an evaluation session (see Samples). Warm-up bars are never evaluated, and the position is flat at the start of the first evaluation session. Let `p ∈ {−1, 0, +1}` be the position held at that close (after any fill at bar `i`'s open), `E` its entry fill price, and `S` its stop.
   - **Flat (`p = 0`).** If `C_i > HI55_i`: order to go long. Else if `C_i < LO55_i`: order to go short. Else nothing.
   - **Long (`p = +1`).** Exit if `C_i ≤ S` (reason `stop`) or else if `C_i < LO20_i` (reason `channel`). `stop` takes precedence when both hold. If exiting and also `C_i < LO55_i`: the order is a **reversal**: exit the long and go short at the same fill. A long exit never re-enters long on the same bar.
   - **Short (`p = −1`).** Mirror: exit if `C_i ≥ S` (`stop`) or else if `C_i > HI20_i` (`channel`); reverse to long if also `C_i > HI55_i`.
4. **Fills.** Every order fills at the **open of the next existing bar `i + 1`**, which may be the first bar of the next session. So a signal at the 15:45 bar's close fills at the next session's 09:30 open. There is **no forced flatten at the session end**: positions are held overnight, over weekends, and over holidays. For a new position, `E = O_{i+1}` and `S = E − STOP_MULT × N_i` for a long, `E + STOP_MULT × N_i` for a short, using `N_i` from the signal bar. `S` never moves. The fill bar's close `C_{i+1}` is the first exit check. If there is no bar `i + 1` (the last bar of the data), an entry order is cancelled, and an exit or reversal fills at `C_i` (the reversal's new side is not opened). A position still open at the close of the last bar of the data is closed at that close with reason `end`.
5. **Stops are close-confirmed**, not intrabar stop orders. High and low are not used for exits. A gap through the stop overnight is seen at the first bar's close and fills at the next bar's open.
6. **Sizing.** Equity starts at 1.0 and compounds. At an entry fill, the notional equals equity marked at the fill price, after any exit cost of the leg being closed. Shares `q = side × notional / E`, fractional, held unchanged until the exit. No leverage and no pyramiding (the Turtles added up to four units; this rule uses one).
7. **Costs.** `COST_BPS / 10,000 × |q| × fill price` is deducted at every fill, entry and exit. A reversal pays the exit leg and the entry leg. QQQ and SPY: 1 bp per side, the protocol default for this liquidity (QQQ's one-cent spread is about 0.15 bp at recent prices). IGV: 2 bp per side, the basis in `igv-small-account-fade` (displayed one-cent spread on a ~$83 median price, widened for a thin tape). No borrow fee on shorts (easy-to-borrow ETFs; general-collateral borrow of about 0.25–0.5% a year is not charged, and the report states the approximate size). No interest on cash, and no margin interest (never above 1× notional).

### Deviations from the published Turtle rules, fixed now

- Entries and exits are close-confirmed at the bar's close and filled at the next open. The Turtles used resting stop orders at the channel and stop prices.
- One unit at 1× equity, no pyramiding, no N-based unit sizing. With one instrument and one unit, N-based sizing would only change leverage.
- Bar counts published for days are applied to 15-minute bars.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| `C_i`, `H_i`, `L_i` | Close of bar `i` (its bucket end, e.g. 10:00 for the 09:45 bar) | Decision at that close |
| `HI55, LO55, HI20, LO20` at `i` | Close of bar `i − 1` (bar `i` excluded) | Decision at close of `i` |
| `N_i` | Close of bar `i`; built from `TR` of bars ≤ `i` | Stop distance for an order decided at close of `i` |
| Fill price | Open of bar `i + 1`, strictly after the decision | That fill |
| Overnight gap | Next session's first bar open | Only through the position already held, and through `TR` of that first bar |
| Warm-up | First 20 sessions | Indicator history only; never traded |
| Daily bars | Not used | Not used |
| Quintiles of the session's close-to-close move | That session's last print | Breakdowns only, never a filter |
| Dividend bias estimate | External approximate schedule | Reported sensitivity only |

No statistic is normalized over the full sample. The universe is three ETFs chosen by hand long before this study. That is not a point-in-time universe and the rules do not claim it is.

## Samples

- **Warm-up:** the first 20 QQQ sessions with bars (2021-09-27 onward). Channels need 55 bars and N needs 20; 20 sessions is 520 bars, enough for N's smoothing to settle.
- **In-sample:** the first evaluation session (the 21st session with bars) through 2024-06-28.
- **Out-of-sample:** 2024-07-01 through 2026-09-25.
- Daily returns belong to their session's sample. **Trades belong to the sample of their entry fill's session**, which decides trade counts and profit factor. A trade open across 2024-06-28 → 2024-07-01 contributes daily returns to both samples.
- The split matches the earlier studies so the windows can be compared. It is not fresh. The out-of-sample window is exposed, as stated under Prior exposure.

## Benchmarks

Uncosted, on the same evaluation sessions:

- QQQ buy-and-hold, price only: last 15-minute close ÷ previous session's last close − 1, 0 on a session with no bars (its move lands on the next session with bars). Price-only to match the strategy's price-only returns.
- The forced-flat variant (Reported checks) is a mechanism comparison, not a benchmark to beat.

## Reported checks

All of these appear in the report whatever they show.

- **Metrics** for full, IS, and OOS at base cost, for QQQ (and the same table for SPY and IGV): the protocol defaults. Sharpe = mean daily simple return ÷ sample SD × √252, zero rate, flat days 0. Total return, CAGR on a 252-session year, annual volatility, max drawdown of compounded equity, t-stat of the mean daily return, trades, win rate, profit factor (Σ winning net trade returns ÷ |Σ losing net trade returns|), average net trade in bp, exposure (share of evaluation bar closes with a position held) and share of overnights held, median and mean holding time in bars and in overnights.
- **Trade return** (for `trades.csv`, win rate, and profit factor): `gross = side × (exit / entry − 1)`, `net = gross − 2 × COST_BPS / 10,000`.
- **Breakdowns:** calendar year; long vs short; exit reason (`stop`, `channel`, `end`, with reversals flagged); entry fill hour (the 09:30 bucket separately, since it holds the overnight-decided entries); number of overnights held; quintiles of |QQQ close-to-close| and of signed QQQ close-to-close return (mean net daily return per quintile).
- **Overnight vs intraday decomposition** of gross P&L, by side. For each trade and each session it is open, the fixed-notional gross contribution is `side × (P_end − P_start) / E`, where `P_start` is the entry fill on the entry session and otherwise the last close of the previous session with bars (so the session's contribution includes the gap into it), and `P_end` is the exit fill on the exit session and otherwise the session's last close. A trade's contributions sum to its gross return. The overnight part of a session's contribution is `side × (the session's first bar open − the previous session's last close) / E`, for each overnight held; the intraday part is the rest. Report sums, counts, and means by side.
- **Costs:** 0, 0.5, 1, 2, and 3× the base cost per side (QQQ 0, 0.5, 1, 2, 3 bp). Costs do not change the path.
- **Fill delay:** each order fills at the open of the second existing bar after the signal bar (`i + 2`). While an order is pending no new decision is made. With no bar `i + 2`, entries are cancelled and exits fill at the last bar's close. **Upper bound** (labelled, not a result): fill at the signal bar's close `C_i`; the first exit check is then the next bar's close.
- **Direction placebo** on gross returns. Build the fixed-notional gross daily series `g_d = Σ over trades open on d of the contribution above`. Actual gross Sharpe is computed on `g` with actual sides. Each draw multiplies every trade's contributions by an independent ±1 and rebuilds `g`. 2,000 draws, NumPy `default_rng(20260926)`. `p = (1 + #draws with Sharpe ≥ actual) / 2001`, full sample.
- **Timing placebo:** not run. Trades span sessions and their lengths depend on the path, so "same count per session with the same exits" has no clean definition here. The direction placebo and the forced-flat variant address the question instead.
- **Block bootstrap:** circular blocks of 20 sessions of the full-sample net daily returns, 2,000 draws, `default_rng(20260927)`; 2.5th and 97.5th percentiles of Sharpe.
- **Plateau grid on IS:** `N_IN ∈ {27, 40, 55, 80, 110}` × `N_OUT ∈ {10, 20, 30}`, 15 cells, `N_ATR = 20` and `STOP_MULT = 2` fixed, 1 bp. OOS Sharpe per cell is shown for selection bias only. Nothing is selected.
- **Cross-market:** identical rules and parameters on SPY (1 bp) and IGV (2 bp).
- **Forced-flat variant** (for Prediction 1; not a candidate): identical, except a position open at the close of a session's last bar is closed at that close (reason `eod`, one exit cost), and a decision at a session's last bar close is dropped.
- **Dividend bias estimate** (approximate; not in any acceptance line): the store has no dividends. Assume QQQ goes ex on the Monday after the third Friday of March, June, September, and December (next session if a holiday) with a dividend of 0.14% of price; SPY on the third Friday of those months with 0.32%; IGV none. For each assumed ex-session, the position held across that session's open earns a correction of `side × yield` (a long was understated, a short overstated). Report the summed correction and the corrected total return, labelled approximate.
- **Verification:** a self-test on synthetic series before the store is opened, covering: long entry on a 55-bar break; short entry; channel exit; stop exit; stop precedence when both hold; reversal long → short; an overnight fill (signal on a session's last bar, filled at the next session's open); a gap through the stop seen at the next session's first close; the end-of-data close; an entry cancelled on the last bar; a missing bucket; an early close; the first-evaluation-bar start; and cost accounting on a reversal. `verify.py` is an independent, deliberately naive re-implementation that builds its own 15-minute bars from 1-minute bars and shares no signal code with `backtest.py`. It replays the full QQQ, SPY, and IGV series and must match every trade on side, entry time and price, exit time and price, and exit reason.

## Acceptance

The primary (QQQ) is a **paper-trading candidate** only if every line holds at 1 bp per side:

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full-sample gross Sharpe.
3. IS Sharpe > 0, and at least 60% of the 15 IS grid cells (9 or more) have Sharpe > 0. An undefined Sharpe counts as not > 0.
4. Full-sample total return > 0 at 2 bp per side.
5. **SPY** OOS Sharpe > 0 under identical rules at 1 bp per side.
6. At least 100 OOS trades. Below that, the verdict is **Inconclusive**.

Line 5 is stricter than the protocol default ("at least one related instrument"): it names SPY because the choice of QQQ as the primary was informed by earlier results, and SPY is the market the published trend literature is built on. IGV is reported but not in the acceptance table. The other lines are the protocol defaults. The pre-lock onset count suggests line 6 will not bind; it was not used to change any parameter.

A failed line fails the strategy.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone; long-only (on the grounds of overnight drift); SPY or IGV as the book; the forced-flat variant; a time-of-day, day-of-week, or volatility filter; dropping the 2N stop; intrabar stop orders; pyramiding or N-based sizing; the dividend-corrected return as the headline; a different cost, fill delay, bar size, or sample split; excluding April 2025 or 2022.
