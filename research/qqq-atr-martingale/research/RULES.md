# QQQ ATR martingale to breakeven: pre-registered rules

Written 2026-09-26, before any return, P&L, forward return, or hit rate was computed from the store for this study. Results may not edit this file. Anything computed after the first store run goes in the report, labelled **(post hoc)**. It never replaces the primary.

This is the sister of `qqq-atr-scale-in`. The entry is the same dislocation. The inventory is not. That study bought equal units, capped them at three, took a quarter-ATR profit, and flattened every night. This one doubles the stake at each further rung, holds overnight, and does not close while the round trip is a loss.

## Prior exposure

- **Looks at the store before this file.** No new query. The coverage, corporate actions, ATR seeds, and same-bar level counts are the ones locked in `qqq-atr-scale-in` (`research/qqq-atr-scale-in/research/counts.json` and that study's `RULES.md`). They are restated here so this file stands alone. They contain no forward return.
  - QQQ, SPY, and IGV each have 1-minute bars on 1,254 sessions, 2021-09-27 → 2026-09-25. 2021-12-31 has no 1-minute bars. 2025-01-09 is a closure. SPY 2026-03-03 has 389 minutes. IGV has hundreds of sessions with untraded minutes. QQQ and SPY have no corporate actions stored. IGV has a 5-for-1 split on 2024-03-07 and no dividends stored.
  - Daily bars: QQQ 2021-09-16 → 2026-09-23; SPY 2021-09-16 → 2026-09-25; IGV 2021-09-17 → 2026-09-23. Intraday bars run through 2026-09-25, so the last two QQQ and IGV sessions have minutes and no daily bar. ATR carries the last prior close forward.
  - Five-minute bars: 78 on a full session, 43 on a 13:00 close. QQQ and SPY have no gap between a decision bar and the next 5-minute bucket. IGV has 15 such gaps.
  - Wilder ATR(14) is first usable on 2021-10-07 for QQQ and SPY, and on 2021-10-08 for IGV.
  - At spacing 0.5, QQQ sessions with a legal level-1 close: 702 (418 in sample, 284 out of sample). Level 2: 150. Level 3: 31. Out-of-sample level-1 sessions were 160 with a down close and 129 with an up close, and a session can be both. SPY level-1 sessions: 651. IGV: 757.
  - Those level-1 counts are not a count of campaigns for this study. This study holds one campaign at a time and does not flatten at the close, so it cannot open a new campaign on every level-1 day. How long a campaign lasts is an outcome and was not measured.

- **Earlier studies on the same instruments, periods, or mechanism.**
  - `qqq-atr-scale-in`: the sister. Equal units, three-unit cap, quarter-ATR target, flat every night. **Rejected.** Out of sample −8.81%, Sharpe −0.57, profit factor 0.78, 343 campaigns. Full sample −25.51%, Sharpe −1.00. Win rate 55.2%, skewness −2.23. On an account that starts at 1, one-unit campaigns made +0.253, two-unit campaigns lost −0.221, and 32 three-unit campaigns lost −0.287. Target exits made +0.508. Session flattens lost −0.763. Zero-cost out-of-sample Sharpe −0.38. Direction placebo p = 0.95. The same rules lost on SPY (out-of-sample Sharpe −0.53) and IGV (−1.09).
  - `qqq-intraday-trend`: noise-boundary momentum, flat every night. **Paper-trading candidate** on QQQ, out-of-sample Sharpe 0.82. The same rules lost on SPY and IGV. Large moves continued.
  - `qqq-intraday-reversion`: fade 4σ five-minute shocks. **Rejected.** Out-of-sample Sharpe −0.76 on QQQ. The path after a shock kept going.
  - `intraday-channel-trend`: 15-minute breakout, flat every night. **Rejected.** Out-of-sample book Sharpe −0.56.
  - `igv-small-account-fade`: one-minute fades, no adds. **Rejected.** Out-of-sample Sharpe −3.82.
  - `qqq-15m-turtle-overnight`: held overnight. **Rejected.** Out-of-sample Sharpe 0.09. The overnight gaps it held were a loss.
  - `micro-futures-trend`: **Rejected.** No gross edge.
  - `qqq-strategy-portfolio`: in-sample max-Sharpe mix of QQQ and five earlier studies. **Rejected.** Out-of-sample Sharpe 0.91 against QQQ's 1.01, return +37.6%. It does not include the scale-in.

- **What I already know about the test windows.** The out-of-sample window 2024-07-01 → 2026-09-25 is **not unseen**. QQQ buy-and-hold rose about 55% on it (Sharpe about 1.0, drawdown about −23% to −24% depending on the study). The window contains the April 2025 crash and rebound. Intraday momentum made money on QQQ there. Intraday fades lost. The in-sample window contains 2022, when QQQ fell about a third, and then a large recovery. A short inventory opened into that recovery, and refused a losing exit, is a position I already know can stay underwater for a long time. The rule stays two-sided. A long-only martingale would be using the drift those studies already measured.

- **Where the parameters came from.** The user asked for the sister of the scale-in that martingales until breakeven, books no realized loss, and can show a large mark-to-market drawdown. No numeric parameter was supplied. The entry spacing, the ATR length, the bar size, the cost, and the base unit are the sister study's, so the first entry is the same bet. The doubling and the refusal to realize a loss are the classic martingale, not a fit. The cap of 16 adds is a numerical bound written below, chosen before any run of this rule.

## Hypothesis

A QQQ campaign opened when price is half a prior-day ATR through the session open, doubled at each further half-ATR step, and held until the round trip covers its costs, has a positive mark-to-market Sharpe on initial capital after 1 bp per side.

**Mechanism.** The entry is the same inventory claim as the sister study (Hendershott and Seasholes 2007; Cheng and Madhavan 2009). The staking is not an inventory claim. It is the doubling martingale: after each adverse step the next stake is a constant multiple of the last, and the position is closed only when a sale at the market covers every fill and the round trip (Dubins and Savage 1965). In a fair game that policy reaches a small target with probability 1 and has a path whose drawdown is the real risk. Costs and any drift are a house edge. They make the expected mark-to-market result negative even while almost every *closed* campaign is a scratch. The study exists to measure that split on this tape, not to argue that doubling creates an edge the sister study's first unit did not have.

**Known counter-forces.** The sister study's adds were the loss, and its direction was worse than a coin flip. A martingale buys more of that. Overnight gaps are a second counter-force: the turtle study's overnight holds lost, and this rule holds the inventory across the gap on purpose. A broker would liquidate a book whose notional is many times capital; this backtest does not. The liquidation would turn the drawdown into the realized loss the exit rule refuses to book. The report states the notional and the mark, and does not call the unliquidated path a tradable account.

## Predictions beyond P&L

Scored on the full QQQ sample at 1 bp per side. They do not enter the acceptance table. A book can pass the table with these predictions failed; the report then says the martingale shape was not what the path produced. A book that matches the shape and fails the table is rejected.

1. **No realized loss.** Every campaign that closes has a realized net of at least 0. The count of closed campaigns with a negative net is 0. If nothing closes, the prediction is *not testable*.
2. **The drawdown is large.** The maximum drawdown of mark-to-market equity is at most −50% (a decline of at least half the running peak).
3. **The hole is larger than the scratches.** The largest peak-to-trough decline in equity units (max over t of peak_t − E_t) is greater than the sum of realized nets on closed campaigns.

## Data

- **Instruments.** QQQ is the primary. SPY is the cross-market acceptance check. IGV is reported under the same rules and is not an acceptance line. Read with `agent-data/mdq.py`, split-adjusted. Only IGV has a split. Dividends are not stored. Borrow and cash interest are 0. A short held for months would pay borrow in a real account; charging 0 favors the short book. That choice is fixed here, not after the run.
- **Bars.** 5-minute bars from `mdq.resample`, anchored at 09:30 New York. The timestamp is the open. Decisions use the close. Fills use the open, except a same-bar upper bound and the end-of-sample mark. The daily ATR uses split-adjusted daily bars. A daily close is known at 16:00 even though its timestamp is 09:30.
- **Calendar.** `mdq.nyse_sessions` from the symbol's first usable session through 2026-09-25. QQQ and SPY start 2021-10-07. IGV starts 2021-10-08. The calendar includes 2021-12-31 and excludes 2025-01-09. In-sample is the start through 2024-06-28. Out-of-sample is 2024-07-01 through 2026-09-25.
- **Missing bars.** Nothing is forward-filled. A session with no bars leaves the position unchanged and contributes 0. A new campaign requires an exact 300-second step to its fill bar. An add or a breakeven exit may fill on the next existing bar, including the next session's open.
- **Checks the script must pass before it writes results.** QQQ's first session is 2021-10-07. 2021-12-31 is on the QQQ calendar and has no 5-minute bars. The ATR used to start a campaign was fixed on a strictly earlier session and is positive.

## Primary rule

| Name | Value | Source |
|---|---|---|
| `BAR_S` | 300 | Same bar as the sister study. |
| `ATR_N` | 14 | Wilder (1978), unchanged. |
| `SPACING` | 0.5 | The sister study's rung. Half a prior day's ATR. |
| `MULTIPLIER` | 2 | The doubling martingale. After k units are on, the next unit's notional is the base times 2^k. |
| `BASE` | 1/3 | The sister study's unit, as a fraction of *initial* equity, fixed for the whole sample. The first fill is the same size as that study's first fill. Later fills are not resized to current equity. |
| `MAX_ADDS` | 16 | Sixteen units in total. The last unit is `BASE × 2^15`. A further double is past any book a broker would finance and is unnecessary for the mark-to-market claim, and the bound keeps the arithmetic inside ordinary floating point. Hitting the cap does not close the campaign. |
| `COST_BPS` | 1 | Protocol default for QQQ-class liquidity. |
| `CUTOFF_MIN` | 30 | A new campaign may not fill in the last half-hour. An open campaign may still add or exit there. Same clock reason as the sister study (Gao, Han, Li and Zhou 2018). |
| `DELAY` | 1 | A signal at a bar's close is acted on at the open of the bar one step later in the tape. |

1. **ATR.** True range and Wilder smoothing are the sister study's formulas. On an intraday session the usable ATR is the latest value whose daily session is strictly earlier. A new campaign records that ATR and does not update it. Later sessions' ATRs do not move an open grid.

2. **Anchor.** A new campaign records `S`, the open of that session's 09:30 five-minute bar. The anchor does not change. Long rung `k` is `S − k × SPACING × A`. Short rung `k` is `S + k × SPACING × A`, for `k = 1, 2, …` while a unit is still allowed. Decisions use the close only.

3. **When a close may schedule an order.** At most one order is pending. While it is pending, no new decision is made. The fill bar is the bar `DELAY` steps later in that symbol's existing 5-minute tape. If that bar does not exist, the order is dropped and the campaign is not closed.
   - A **new campaign** is dropped unless the fill bar is in the same session, its timestamp is exactly `DELAY × 300` seconds after the signal bar, and its New York open is strictly earlier than the session end minus 30 minutes. The session end is 16:00, or 13:00 on `mdq.EARLY_CLOSES`. A new campaign also requires the signal session to have a 09:30 bar and a last bar opening at or after 15:55 (12:55 on an early close).
   - An **add** or a **breakeven exit** may fill on a later session's open. The last half-hour does not block them.

4. **Position.** One campaign, or none. It is long or short. A new campaign starts only when flat. There is no flip on the same fill.

5. **Notional.** The first unit is `BASE`. With `n` units already on, the next unit is `BASE × MULTIPLIER^n`. Notionals are fractions of initial equity. They do not compound and they do not shrink after a drawdown.

6. **Mark of an open campaign.** At a price `m`, with side `s` (+1 long, −1 short), leg prices `P_j` and notionals `N_j`,

   `gross = Σ s × (m / P_j − 1) × N_j`

   `net = gross − 2 × c × Σ N_j`

   where `c = COST_BPS / 10000`. The factor of 2 is the entry cost and the exit cost. `net` is the P&L if the campaign is sold at `m`.

7. **Decisions at a close that is allowed to schedule,** in this order.
   - **Campaign open and `net(close) ≥ 0`.** Schedule an exit.
   - **Campaign open, `net(close) < 0`, fewer than `MAX_ADDS` units, and the close is at or beyond the next rung and strictly worse than every fill.** Schedule one add. One add per bar. A close beyond several rungs still schedules one unit.
   - **Flat, and the close is at or beyond rung 1 of today's anchor using today's ATR.** Schedule one new unit on that side. A close beyond rung 3 still schedules one unit.

8. **Fills, at the fill bar's open,** except the delay-0 upper bound, which fills at the signal close.
   - **Pending exit.** Fill only if `net(open) ≥ 0`. Otherwise cancel the exit and keep the campaign. An exit that fills has a non-negative net at the fill price. The reason is `breakeven`.
   - **Pending add.** If `net(open)` of the units already on is ≥ 0, cancel the add and exit at that open instead (reason `breakeven`). Otherwise fill the add at the open.
   - **Pending entry.** Fill at the open. The anchor and the ATR are the signal session's, already fixed.
   - A realized net between −1e−9 and 0 initial-equity units is stored as 0. A realized net below −1e−9 aborts the run. That tolerance is dust, not a loss the rule accepts.

9. **End of the sample.** An open campaign is not closed. It is marked at the last bar's close. The row is reason `open`. Its mark-to-market net is in equity. It is not a realized trade. There is no forced flatten at a loss, at the close, or at the cap.

10. **Equity and the daily result.** Initial equity `E_0 = 1`. While a campaign is open, its contribution is `gross(mark) − c × Σ N_j` (entry cost is sunk; exit cost is not charged until the exit fills). When it exits, the contribution becomes the realized `net`. Equity is 1 plus all realized nets plus the open contribution. It is marked at each session's last available close. A session with no bars has a mark equal to the previous mark.

    The daily result used for Sharpe, volatility, and the daily profit factor is `r_t = E_t − E_{t−1}`. It is P&L per unit of initial capital, not P&L divided by current equity. A martingale can drive mark-to-market equity through zero, and a ratio to that equity is not a return. Total return is `E_T − 1`, which includes the open campaign. Maximum drawdown is the minimum of `E_t / peak_t − 1`, with `peak` the running maximum of equity and with the path starting at 1. It may be worse than −100%. CAGR is `E_T^(252/n) − 1` when `E_T > 0`, and is undefined otherwise.

11. **Costs on the sweeps.** The path is resimulated at 0, 0.5, 1, 2, and 3 bp per side. A higher cost moves the breakeven price. It is not a rescaling of one path.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| ATR recorded on a campaign | Prior daily close, 16:00 that day | The entry decision and every later rung of that campaign |
| Today's ATR and today's open | 09:30 open; ATR from the prior close | A new entry only, and only on a later close the same day |
| Signal close | End of that 5-minute bar | Fill at a later bar's open, or not at all |
| `net` at the fill open | That open | Whether an exit fills, or whether a pending add is turned into an exit |
| End-of-sample mark | Last bar's close | Equity. Not a fill |
| Buy-and-hold daily close | 16:00 of its session | The benchmark, not a signal |

No full-sample quantile or volatility target enters the orders. The grid is fixed at the entry.

## Samples

- **Warm-up.** Daily bars before the first usable session. QQQ and SPY through 2021-10-06. IGV through 2021-10-07. Warm-up is not in the return calendar.
- **In-sample.** The first usable session through 2024-06-28.
- **Out-of-sample.** 2024-07-01 through 2026-09-25.
- The split matches the earlier studies on this store. It is not fresh, and the out-of-sample window is exposed.
- A campaign that crosses 1 July 2024 keeps its fills. Its mark-to-market change on each day belongs to that day's window. Trade counts for a closed campaign belong to the window of its entry session. The open campaign at the sample end belongs to its entry window for the count, and its mark is in equity on the last day.

## Benchmarks

Both uncosted, on the same QQQ sessions. The headline benchmark is buy and hold.

- **Buy and hold.** Daily close to daily close. Where a session has no daily bar, the mark is the last 5-minute close. The first session's previous mark is the last daily close before the evaluation calendar. The script records how many marks came from a 5-minute close.
- **Open to close.** Last 5-minute close over the 09:30 open, minus one, on sessions with a tape. A session without a tape is 0.

The sister study's published result is prior exposure, not a second benchmark recomputed here.

## Reported checks

- **Metrics** for full, in-sample, and out-of-sample, at 1 bp, for QQQ, SPY, and IGV. Sharpe is the mean of `r_t` over its sample standard deviation, times √252, with a zero rate. Profit factor is the sum of positive `r_t` over the absolute sum of negative `r_t`. A window's total return is the sum of its `r_t` (the change in equity across the window, in units of initial capital). CAGR for a window is `(E_end / E_start)^(252/n) − 1` when both equity marks are positive, and is undefined otherwise. The full-sample maximum drawdown uses the running peak of equity from the start at 1. The summary's in-sample and out-of-sample maximum drawdowns are computed on a path that restarts at 1 and adds only that window's `r_t`, so neither window inherits the other's peak. Both the restarted figure and the full-path reading during the window are stored. Drawdown is not an acceptance line. Prediction 2 uses the full-sample path.
- **Realized account, stored separately and not used for the acceptance lines.** Number of closed campaigns, number with negative net, sum of realized nets, number still open, open mark-to-market net, maximum gross notional, whether the 16-unit cap bound, minimum equity, and the peak-to-trough decline in equity units.
- **Milestones.** The first session on which gross notional exceeds 2, 5, 10, and 20 times initial equity, and the equity on that session. This is what a financing limit would have turned into a forced sale. It is not itself a strategy.
- **Breakdowns.** Calendar year (sum of `r_t`, Sharpe, and the worst full-sample drawdown reading during the year). Side at the session close. Closed campaigns by unit count. Sum of overnight gross gaps (prior close to this open, on legs already held) and the rest of the daily P&L.
- **Costs.** 0, 0.5, 1, 2, and 3 bp, each a full resimulation.
- **Fill delay.** `DELAY = 2`, and `DELAY = 0` as the labelled upper bound. Delay 0 fills at the signal close and does not judge that bar again. On the last bar, delay 0 may exit if `net(close) ≥ 0` and may add; it may not open a new campaign if that bar's open is inside the cutoff window.
- **Direction placebo.** Gross P&L only, constant capital, costs excluded. Each campaign's gross pieces are multiplied by one ±1 for the whole campaign. 2,000 draws, NumPy `default_rng(20260926)`. `p = (1 + count of gross Sharpes ≥ actual) / 2001` on the full sample. A single long-lived campaign makes this a weak test. It is still the test.
- **Timing placebo.** 500 draws, `default_rng(20260927)`, QQQ only, not an acceptance line. The primary's campaign starts are counted per session, with their sides. Each draw walks the tape. When a session has a primary start and the placebo book is flat, it enters at the open of a bar drawn uniformly from that session's bars whose open is strictly before the cutoff, with the primary's side and the base notional. Adds, the cap, and the breakeven exit then follow the primary rule. A session whose primary started a campaign while the placebo is already in a campaign consumes no draw and starts nothing. `p = (1 + count of gross Sharpes ≥ actual) / 501`.
- **Block bootstrap.** Circular 20-session blocks of full-sample `r_t`, 2,000 draws, `default_rng(20260928)`. The 2.5th and 97.5th percentiles of Sharpe, and the share of draws at or below 0.
- **Plateau, in sample only.** `SPACING ∈ {0.35, 0.50, 0.65, 0.80, 1.00}` × `MULTIPLIER ∈ {1.5, 2.0, 3.0}`, 15 cells. The primary is (0.50, 2). Out-of-sample Sharpe is shown and is not used. An undefined Sharpe counts as not greater than 0. Nine cells are 60%. The in-sample rank of the primary is reported. A tie in in-sample Sharpe is broken by the listed spacing order, then the listed multiplier order.
- **Cross-market.** SPY and IGV, each with its own ATR, its own equity starting at 1, and the same rules. Pairwise correlation of `r_t` on shared dates.
- **Verification.** Self-test before the store is opened: a one-unit scratch that clears 2 bp and a return only to the entry price that does not; a doubled second unit; an exit cancelled because the next open is no longer a scratch; an add that is cancelled and turned into an exit when the open already scratches; a path that never recovers and is still open, with a negative mark and no realized trade; the 16-unit cap with no forced sale; a new entry blocked in the last half-hour and blocked across a missing bucket; an add that fills on the next session's open; the short side; dust not booked as a loss. `verify.py` replays every QQQ campaign and matches side, unit count, leg prices, leg notionals, entry time, exit time, exit price, and exit reason, and checks that every closed net is ≥ 0.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 1 bp per side, judged on mark-to-market equity, not on realized scratches. If line 6 fails, the status is **Inconclusive** even if other lines fail. If line 6 holds and any other line fails, the status is **Rejected**.

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample daily profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full-sample gross Sharpe.
3. In-sample Sharpe > 0, and at least 9 of the 15 in-sample grid cells have Sharpe > 0.
4. Full-sample total return `E_T − 1` > 0 at 2 bp per side.
5. SPY out-of-sample Sharpe > 0 at 1 bp. IGV is not this line.
6. At least 100 out-of-sample sessions in which a campaign was held at the mark or a campaign was closed. Below that, **Inconclusive**.

Line 6 is the protocol's 100-trade minimum, restated in sessions rather than closed campaigns. A hold-until-breakeven book can have one campaign open for hundreds of sessions and only a handful of closes. The daily mark is the observation. Closed-trade profit factor is not a line, because the exit rule makes it unable to fail.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone; a liquidation at 2×, 5×, or any other financing limit; a rule that flattens at the close or at the 16-unit cap and books the loss; the realized-scratch total as if the open mark were zero; IGV; a time-of-day filter; a different cost or sample split; the equal-unit three-cap sister study, which already has its own verdict.

## References

- Cheng, Minder, and Ananth Madhavan (2009). "The Dynamics of Leveraged and Inverse Exchange-Traded Funds." *Journal of Investment Management*.
- Dubins, Lester E., and Leonard J. Savage (1965). *How to Gamble If You Must*. McGraw-Hill.
- Gao, Lei, Yufeng Han, Sophia Zhengzi Li, and Guofu Zhou (2018). "Market Intraday Momentum." *Journal of Financial Economics*.
- Hendershott, Terrence, and Mark S. Seasholes (2007). "Market Maker Inventories and Stock Prices." *American Economic Review, Papers and Proceedings*.
- Wilder, J. Welles Jr. (1978). *New Concepts in Technical Trading Systems*. Trend Research.
