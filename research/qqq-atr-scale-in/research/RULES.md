# QQQ intraday ATR scale-in: pre-registered rules

Written 2026-09-26, before any return, P&L, forward return, or hit rate was computed from the store. Results may not edit this file. Anything computed after the first store run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** Counts only. No price after a signal bar, and no return.
  - `python agent-data/mdq.py` through `research/qqq-atr-scale-in/research/counts.py`, which writes `counts.json`.
  - QQQ, SPY, and IGV each have 1-minute bars on 1,254 sessions, 2021-09-27 → 2026-09-25. Coverage records one missing session on each, the known 2021-12-31 gap (the terminal calendar treats that Friday as a holiday; NYSE was open; there are no 1-minute bars). SPY has one further short session (389 minutes; prior studies identify it as 2026-03-03). IGV has 644 sessions needing attention, almost all untraded minutes. QQQ and SPY have no corporate actions stored. IGV has one split, 5-for-1 on 2024-03-07, and no dividends stored.
  - Daily bars, split-adjusted: QQQ 2021-09-16 → 2026-09-23 (1,260 bars); SPY 2021-09-16 → 2026-09-25 (1,262); IGV 2021-09-17 → 2026-09-23 (1,259). Intraday bars run through 2026-09-25 for all three, so the last two QQQ and IGV sessions have minutes and no daily bar. The ATR rule below carries the last prior close forward onto those sessions. That carry was checked before the lock; it is not a result.
  - Five-minute bars from `mdq.resample`: a full session has 78 bars and an early close has 43 (the 13:00 bucket is the one 13:00 minute). QQQ and SPY have no decision bar whose next 5-minute bucket is missing. IGV has 15 such bars, on 15 sessions that have 77 five-minute bars.
  - Wilder ATR(14) seed is fixed at the 2021-10-06 close for QQQ and SPY, and at the 2021-10-07 close for IGV. The first session that may use it is 2021-10-07 (QQQ, SPY) and 2021-10-08 (IGV).
  - Same-bar level counts at spacing 0.5, with the entry cutoff below, and with no later price read. A close beyond level 3 is also beyond level 1, so the levels are nested. A session can be both long and short.

    | | QQQ sessions beyond the level (IS / OOS) | of which OOS long / short at level 1 |
    |---|---|---|
    | Level 1 (0.5 ATR) | 702 (418 / 284) | 160 / 129, not exclusive |
    | Level 2 (1.0 ATR) | 150 (89 / 61) | |
    | Level 3 (1.5 ATR) | 31 (15 / 16) | |

    SPY level-1 sessions: 651 (382 / 269). IGV: 757 (445 / 312). OOS level-1 sessions are a lower bound on campaigns, because every session starts flat. 284 is above the 100-trade minimum, so the study is feasible. The third rung is rare. It was not moved after this count.

- **Earlier studies on the same instruments, periods, or mechanism.**
  - `qqq-intraday-trend`: noise-boundary momentum, flat every night. **Paper-trading candidate** on QQQ. Out-of-sample Sharpe 0.82. The same rules lost out of sample on SPY (−0.36) and IGV (−0.38). Profit came from a few large days; removing April 2025 cut the out-of-sample Sharpe to 0.45. From May 2025 the Sharpe was about −0.15.
  - `qqq-intraday-reversion` (its folder is not in this layout; the figures are those recorded in `igv-small-account-fade`): fade 4σ five-minute shocks. **Rejected.** Out-of-sample Sharpe −0.76 on QQQ. The average path after a shock kept going. The same rules lost on IGV (out-of-sample Sharpe −0.46).
  - `intraday-channel-trend`: 15-minute Donchian breakout, flat every night, equal-weight QQQ/SPY/IGV. **Rejected.** Out-of-sample book return −12.5%, Sharpe −0.56. No gross edge out of sample.
  - `igv-small-account-fade`: one-minute fades in IGV, no adds. **Rejected.** Out-of-sample return −2.96%, Sharpe −3.82. Zero-cost out-of-sample Sharpe −0.32.
  - `qqq-15m-turtle-overnight`: 55/20 Turtle on 15-minute bars, held overnight. **Rejected.** Out-of-sample Sharpe 0.09. The overnight gaps it held summed to a loss. SPY out-of-sample Sharpe −0.48.
  - `micro-futures-trend`: multi-month trend on micro futures, including the equity indices. **Rejected.** No gross edge. Different horizon.
  - This study is the first here that adds to a losing intraday position. The single-shot fades above are the closest mechanism, and they lost. That lowers the prior that this one works. It does not decide the test.

- **What I already know about the test windows.** The out-of-sample window 2024-07-01 → 2026-09-25 is **not unseen data**. From the reports above: QQQ buy-and-hold returned +55.4% on it (Sharpe 1.01, max drawdown −22.9%); the window contains the April 2025 tariff crash and the 2025-04-09 rebound; intraday momentum made money on QQQ there and lost on SPY and IGV; single-shot intraday fades lost on QQQ; session-local 15-minute breakouts had no gross edge. The in-sample window contains 2022, when QQQ fell about a third. QQQ's 20 worst days in 2021–2026 averaged about −4.4% close to close. I also saw, in the count above, that out-of-sample QQQ level-1 sessions are somewhat more often down than up (160 vs 129). The rule stays symmetric. A long-only book would harvest a drift I already know is there.

- **Where the parameters came from.** The user asked for an intraday strategy that scales into losing positions, in ATR units, and whose payoff is negatively skewed with mostly positive campaigns. No numeric parameter was supplied. Each number below is Wilder's published default or a reason written before the count. The count was used only to confirm that a 100-trade out-of-sample sample is reachable. Nothing was resized after it.

## Hypothesis

On QQQ, a 5-minute close at least half a prior-day Wilder ATR away from the session open is followed, more often than not, by a move back toward the average entry of a scale-in fade, and that bounce is large enough, after 1 bp per side, that a book which adds equal units at one and one-and-a-half ATR and caps the gain at a quarter ATR past the average has a positive expected return by the cash close.

**Mechanism.** The scale-in is an inventory, not a forecast. A market maker who buys as price falls, and sells as it rises, is paid when the dislocation was someone else's need to transact rather than information (Hendershott and Seasholes 2007; the same claim at a weekly horizon is in Lehmann 1990). On an index ETF the price-insensitive side is real: leveraged and inverse funds rebalance in the direction of the move, and option hedges do the same (Cheng and Madhavan 2009; Baltussen, Da, Lammers and Martens 2021). They keep paying because the mandate is to finish the trade. The anchor is the session open because that is the last price at which the overnight information was agreed, and the unit is Wilder's ATR because it is the published scale of a normal day's range (Wilder 1978). "Dampening" in this study means the average entry is pulled toward the market by later, worse fills. It does not mean a filter on falling ATR.

The payoff is built to be short-option shaped. The gain per share is capped. The loss is whatever is left at the cash close, and it is larger when more units are on. That is the negative-skew profile of selling liquidity (Ilmanen 2011, on carry and short volatility). The cap and the adds are part of the hypothesis, not an ornament.

**Known counter-forces.** The other side of Glosten and Milgrom (1985) is adverse selection: the flow that pushes price 1.5 ATR is more likely to be informed, and adding buys more of it. The event study in `qqq-intraday-reversion` found that extreme 5-minute shocks in this sample kept going. Intraday momentum (Gao, Han, Li and Zhou 2018), and the QQQ result in `qqq-intraday-trend`, say the same thing about large moves into the close. The open is also a stale anchor once the day's news is in. Costs scale with the number of legs, while the gain per share does not. The report splits results by the size of the open-to-close excursion and by the number of units, which is where these forces show up.

## Predictions beyond P&L

If the mechanism is right, then, on the full QQQ sample at 1 bp per side:

1. **The booked shape is negative skew with a high win rate.** The win rate of campaigns is above 55%, and the Pearson moment skewness of per-unit net campaign returns is negative. Skewness is g1 = m3 / m2^1.5, with the second and third central moments divided by N, not N−1. A campaign is a win when its net dollars are positive.
2. **Ordinary days pay and large excursions do not.** Sessions that have a 5-minute tape are ranked by |last 5-minute close − 09:30 open| / prior ATR and split into quintiles as specified under Reported checks. The mean net daily strategy return on the pooled sessions in quintiles 2 and 3 (1-indexed) is positive. The mean on quintile 5 is negative.
3. **The later adds are the tail, not the edge.** The mean per-unit net return of 1-unit campaigns is greater than the mean per-unit net return of 3-unit campaigns. If either group has fewer than 5 campaigns, this prediction is *not testable* rather than failed.

Each one is scored *consistent*, *not consistent*, or *not testable*. These scores do not enter the acceptance table. A book can pass the table with the wrong shape; the report then says the mechanism is unconfirmed. A book with the right shape and a failed table is rejected.

## Data

- **Instruments.** QQQ is the primary. SPY is the cross-market acceptance check. IGV is reported under the same rules and is not an acceptance line. Read with `agent-data/mdq.py`, split-adjusted. Only IGV has a split; adjustment divides prices by 5 before the 2024-03-07 ex-timestamp, so the true range does not treat the split as a crash. Dividends are not stored and are not adjusted. The book is flat overnight, so a dividend gap is not held.
- **Bars.** Decisions are on 5-minute bars built by `mdq.resample` from 1-minute bars, anchored at 09:30 New York, never spanning a session. The bar timestamp is the open. The close is the last 1-minute close inside the bucket. A partial bucket still emits. The daily ATR uses split-adjusted daily bars. A daily close is known at 16:00 New York even though its timestamp is 09:30.
- **Calendar.** Evaluation sessions are `mdq.nyse_sessions` from the symbol's first usable session through 2026-09-25. That calendar excludes 2025-01-09 and includes 2021-12-31. QQQ and SPY start 2021-10-07. IGV starts 2021-10-08. In-sample is the start through 2024-06-28. Out-of-sample is 2024-07-01 through 2026-09-25.
- **Missing bars.** An empty 5-minute bucket is left missing. A signal fills only when the bar exactly 300 seconds later exists. Nothing is forward-filled. A session with no usable tape contributes a strategy return of 0 and no trade. 2021-12-31 is that case.
- **Checks the script must pass before it writes results.** QQQ's first usable session is 2021-10-07. Every tradable session has a 09:30 bar. The ATR used on a session is positive and was fixed on a strictly earlier session. 2021-12-31 is on the QQQ calendar and has no 5-minute bars.

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| `BAR_S` | 300 | Five minutes, so three sequential decisions are at least five minutes apart and can still fit before the cutoff. |
| `ATR_N` | 14 | Wilder (1978), used unchanged. |
| `SPACING` | 0.5 | Half a prior day's ATR. A full ATR from the open is about one entire prior range, so the first rung is the smallest round fraction that is still a dislocation rather than a spread. |
| `TARGET` | 0.25 | Half the spacing. The booked gain per share is smaller than the gap between rungs, which is what makes the intended skew negative. |
| `MAX_UNITS` | 3 | Rungs at 0.5, 1.0, and 1.5 ATR. Equal units, not a doubled stake. |
| `COST_BPS` | 1 | Protocol default for QQQ-class liquidity. One cent on a price above about $100 is under 1 bp, so 1 bp is wider than the quoted spread of QQQ and SPY for a small order. |
| `CUTOFF_MIN` | 30 | No new unit fills in the last half-hour. That half-hour is its own trading session in Gao, Han, Li and Zhou (2018), and this book does not want to open inventory into it. |
| `DELAY` | 1 | A signal at a bar's close fills at the next bar's open. |

1. **ATR.** On daily bars sorted by session, the true range of a bar after the first is `max(high − low, |high − previous close|, |low − previous close|)`. The seed is the arithmetic mean of the first 14 true ranges, fixed at that 14th bar's close. Thereafter `ATR = (13 × previous ATR + true range) / 14`, fixed at that bar's close. On an intraday session, use the latest ATR whose daily session is strictly earlier. If that value is missing or not positive, the session is not tradable.

2. **Session open and levels.** The open `S` is the open of the 09:30 five-minute bar. With `A` the ATR from rule 1, long level `k` is `S − k × SPACING × A` and short level `k` is `S + k × SPACING × A`, for `k = 1, 2, 3`. Decisions use the close only. High and low are ignored.

3. **When a close may schedule a fill.** Bar `i` may schedule a fill only if a bar exists at `ts + DELAY × 300`, every intermediate step of 300 seconds exists, and the fill bar's New York open is strictly earlier than the session end minus 30 minutes. The session end is 16:00, or 13:00 on a date in `mdq.EARLY_CLOSES`. A regular fill at 15:25 is allowed. A fill at 15:30 is not. An early-close fill at 12:25 is allowed. A fill at 12:30 is not. The last bar of the session never schedules a fill, because no later bar exists. At most one order is pending. While an order is pending, no new decision is made.

4. **Position.** At most one campaign. It is long or short, with 1 to 3 units. Each session starts flat. A campaign ends flat before another can start. There is no flip: an exit does not open the other side on the same fill. The bar on which an exit filled may, at its close, schedule a new campaign for the following open.

5. **Decisions, in this order, at a close that is allowed to schedule.** Let `c` be the close, and let the average entry be the arithmetic mean of the campaign's fill prices (each unit has the same notional, so this is the notional-weighted average).
   - **In a long:** if `c ≥ average + TARGET × A`, schedule an exit of every unit, reason `target`. Else if the unit count `n` is below 3, `c ≤` long level `n+1`, and `c` is strictly below every existing fill, schedule one added unit. Otherwise hold.
   - **In a short:** if `c ≤ average − TARGET × A`, schedule an exit, reason `target`. Else if `n < 3`, `c ≥` short level `n+1`, and `c` is strictly above every existing fill, schedule one added unit. Otherwise hold.
   - **Flat:** if `c ≤` long level 1, schedule one long unit. Else if `c ≥` short level 1, schedule one short unit. A close beyond level 3 still schedules one unit, not three. At most one unit is filled on a bar.
   - Target is checked before an add. The two cannot both be true for one close, because an add requires a close beyond the worst fill and the target is on the favorable side of the average. If a bar ever satisfies both, the target wins.

6. **Exits.** The only planned exit is the target above, filled at the next bar's open under rule 3. If a campaign is open at the close of the session's last bar, it flattens at that close, reason `session`, and any order that has not filled is cancelled. On the primary delay of one bar, a target touched on the last bar does not get a next open, so the reason is `session` even if the close is through the target. There is no stop. A fill on the last bar's open, if that open was a legal fill, is flattened at that same bar's close.

7. **Fills.** The fill price is the scheduled bar's open, except the session flatten, which is the last bar's close. The fill is known only at that timestamp. A unit filled at a bar's open is part of the average when that bar's close is judged.

8. **Sizing.** Equity starts at 1. It compounds once per session, at the last mark, with no intraday compounding. The unit notional on a session is one third of equity at the previous session's mark, and every unit opened that session uses that same notional. Three units are 100% of start-of-day equity. There is no leverage and no doubling. Gross dollars on a unit are `side × (exit / entry − 1) × unit notional`, with side +1 long and −1 short. Each unit pays `COST_BPS` basis points of its entry notional on the entry and again on the exit. Net dollars are the sum across the campaign's units. The session's strategy return is the sum of its campaigns' net dollars divided by start-of-day equity. A session with no campaign, and a session that is not tradable, returns 0. A short is assumed locatable. Borrow and cash interest are zero because the book is flat overnight.

9. **Tradable session.** Prior ATR is positive, a 09:30 five-minute bar exists, and the last bar's open is at least 15:55 on a regular day or 12:55 on an early close. Otherwise the session stays on the calendar with return 0.

10. **Costs on the reported sweeps.** The same fills are repriced at 0, 0.5, 1, 2, and 3 bp per side. Costs scale with the unit notional and with the number of units. They are not charged on unused cash.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Wilder ATR | Close of the daily session that fixed it, 16:00 New York that day | Every decision on later sessions only |
| Session open `S` | 09:30:00 of the session, the first five-minute open | Closes after that open |
| Signal close | End of that five-minute bucket | Fill at the next bucket's open, or not at all |
| Average entry | The opens of units already filled | Later closes of the same campaign |
| Last-bar flatten | That bar's close, the last stored regular-hours print | The same close. There is no later bar |
| Daily close used only in the buy-and-hold benchmark | 16:00 of its session | The benchmark, not a signal |
| Quintile edges | Computed in the backtest from the whole evaluation sample's excursions | Reporting the pre-registered prediction. Not a trading input |

Rolling statistics use prior sessions only. No z-score, quantile, or volatility target is estimated on the evaluation sample and then used to trade. The universe is these three symbols on every date.

## Samples

- **Warm-up:** daily bars before the first usable session. QQQ and SPY: through 2021-10-06, the ATR seed. IGV: through 2021-10-07. Warm-up sessions are not in the return calendar.
- **In-sample:** the symbol's first usable session through 2024-06-28. QQQ and SPY begin 2021-10-07. IGV begins 2021-10-08.
- **Out-of-sample:** 2024-07-01 through 2026-09-25.
- The split is the one used by the earlier studies on this store, so the windows can be compared. It is not a fresh split. The out-of-sample window is exposed, as stated under Prior exposure. Trades belong to the sample of their entry session. No campaign crosses a session, so none crosses the split.

## Benchmarks

Both are uncosted, and both are reported on the same QQQ evaluation sessions.

- **Buy and hold.** Daily close to daily close, split-adjusted. Where a session has no daily bar (QQQ 2026-09-24 and 2026-09-25, and the same pattern on IGV), the mark is that session's last five-minute close. The previous mark is the previous session's daily close when it exists, otherwise that session's last five-minute close. The script records how many marks were five-minute closes.
- **Open to close.** Last five-minute close divided by the 09:30 five-minute open, minus one, on sessions that have a tape. A session without a tape is 0.

The headline benchmark in the summary table is buy and hold. Open to close is there so an intraday drift is visible. Neither is a trading rule.

## Reported checks

All of these appear in the report whatever they show.

- **Metrics** for full, in-sample, and out-of-sample at 1 bp per side, for QQQ, and the same trio for SPY and IGV: the protocol defaults. Sharpe is the mean daily simple return divided by its sample standard deviation, times √252, with a zero risk-free rate. Days with no position are 0. CAGR uses a 252-session year. Profit factor is the sum of winning campaigns' net dollars divided by the absolute sum of losing campaigns' net dollars. Win rate is the share of campaigns with positive net dollars. Average net trade, in bp, is the mean over campaigns of net dollars divided by (units × unit notional), times 10,000. Exposure is the share of five-minute bars, on tradable sessions, during which a unit was on: the fill bar counts, the flatten bar counts, and the bars in between count. Holding time is the number of five-minute bars from the first fill through the exit, inclusive.
- **A campaign** is one row of `trades.csv`. The entry price is the arithmetic mean of the leg prices. The entry time is the first fill. Leg prices are stored beside the average so a missing add cannot hide.
- **Breakdowns:** calendar year (the year's own compounded return, Sharpe, and max drawdown, with the drawdown peak reset on the first session of the year); long vs short; exit reason (`target` or `session`); New York hour of the first fill; the excursion quintiles below; campaigns grouped by unit count.
- **Quintiles.** On tradable QQQ sessions, rank `|last close − open| / ATR` with a stable sort. Quintile of the session in position `i` of that order (0-based) is `min(i × 5 // N, 4)`. Ties keep their original order and can fall in different quintiles. Sessions without a tape are not ranked.
- **Costs:** 0, 0.5, 1, 2, and 3 bp per side on QQQ, same fills.
- **Fill delay:** the primary, plus one extra bar (`DELAY = 2`: the fill is 600 seconds after the signal close, and both intermediate timestamps must exist and the fill must clear the cutoff). The labelled upper bound is a same-bar close fill (`DELAY = 0`): the order fills at the signal close, the state updates, and that bar is not judged again. On the last bar, `DELAY = 0` still opens nothing; a target that is true exits at that close with reason `target`, and a position that is not at its target exits with reason `session`.
- **Direction placebo** on gross returns. Fixed notional, not the compounded equity path. A campaign's session contribution is the sum over its legs of `side × (exit / entry − 1) / 3`. Sessions with no campaign contribute 0. The actual gross Sharpe uses the real sides. Each draw multiplies every campaign's contribution by an independent ±1, one sign for the whole campaign. 2,000 draws, NumPy `default_rng(20260926)`. `p = (1 + count of draws whose Sharpe ≥ actual) / 2001`, on the full sample.
- **Timing placebo.** 500 draws, `default_rng(20260927)`, QQQ only, gross fixed-notional Sharpe as above. For each session, in entry order, each primary campaign keeps its side and its unit count as a cap. Legal fill bars are those after the 09:30 bar whose open is strictly before the cutoff. The draw picks uniformly among legal fill bars that sit strictly after the previous placebo campaign's exit and that start a run of `n` time-adjacent legal bars. It then applies the same add rule, the same target, and the same session flatten, so a campaign may finish with fewer than `n` units if price never reaches the next rung. If no legal start remains, that campaign is dropped on that draw. `p = (1 + count of draws whose gross Sharpe ≥ actual gross Sharpe) / 501`. This is not an acceptance line.
- **Block bootstrap:** circular blocks of 20 sessions of the full-sample net daily returns, 2,000 draws, `default_rng(20260928)`. Report the 2.5th and 97.5th percentiles of Sharpe, and the share of draws at or below 0.
- **Plateau grid on the in-sample window:** `SPACING ∈ {0.25, 0.375, 0.50, 0.625, 0.75}` × `TARGET ∈ {0.125, 0.25, 0.375}`, 15 cells, everything else fixed, 1 bp, QQQ. The primary cell is (0.50, 0.25). Out-of-sample Sharpe for each cell, and the out-of-sample result of whichever cell ranks first in sample, are shown for selection bias only. Nothing is selected. A cell with undefined Sharpe counts as not greater than 0. The in-sample rank of the primary is reported.
- **Cross-market:** identical rules on SPY and on IGV, each with its own ATR, its own equity starting at 1, and 1 bp per side. IGV's spread is wider than 1 bp on many prints; the 1 bp figure is the identical-rules number, not a claim about IGV's cost. Pairwise correlation of the three daily strategy-return series, on the dates they share, is reported.
- **Verification.** `backtest.py` runs the self-test below before it opens the store, and aborts if any case fails. `verify.py` is a second implementation and replays every QQQ evaluation session. It must match every campaign on side, unit count, leg prices, entry time, average entry price, exit time, exit price, and exit reason.

Self-test paths, on hand-built bars, with the expected trades computed by hand: a one-unit long that hits the target; a three-unit long that never hits it and flattens at the last close; a short of the same shape; a gap fill already through the next level that does not add unless the close is worse than every fill; a first close beyond level 3 that buys one unit and then adds once per later bar; no fill in the last 30 minutes; a signal whose next bucket is missing; an early-close cutoff at 12:30; an exit and a later re-entry; a last-bar signal that does not open; a day with no signal; costs on three units; `DELAY = 0` filling at the signal close and not re-deciding on that bar.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 1 bp per side. If line 6 fails, the status is **Inconclusive**, even if other lines also fail. If line 6 holds and any other line fails, the status is **Rejected**.

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full-sample gross Sharpe.
3. In-sample Sharpe > 0, and at least 60% of the 15 in-sample grid cells (9 or more; 8/15 = 53% and 7/15 = 47%, so the count that satisfies 60% is 9) have Sharpe > 0.
4. Full-sample total return > 0 at 2 bp per side.
5. SPY out-of-sample Sharpe > 0 under identical rules at 1 bp per side. IGV is not this line. One basis point understates IGV's cost, and "any one of two names" would let that understatement decide the test.
6. At least 100 out-of-sample QQQ campaigns. Below that, the verdict is **Inconclusive**.

Line 5 is tighter than the protocol's "at least one related instrument," for the reason in the line. Every other threshold is the protocol default.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone, including the side that had more level-1 sessions in the pre-lock count; IGV, or any single symbol other than the primary; a time-of-day or regime filter; a different cost, fill, or sample split; a stop loss; a doubled or martingale stake; dropping the third rung because the count showed it is rare; a filter that trades only when ATR is falling; an anchor other than the session open, including VWAP; a long-only book, which would take the equity drift already measured on this out-of-sample window; holding a position overnight.

## References

- Baltussen, Guido, Zhi Da, Sten Lammers, and Martin Martens (2021). "Hedging Demand and Market Intraday Momentum." *Journal of Financial Economics*.
- Cheng, Minder, and Ananth Madhavan (2009). "The Dynamics of Leveraged and Inverse Exchange-Traded Funds." *Journal of Investment Management*.
- Gao, Lei, Yufeng Han, Sophia Zhengzi Li, and Guofu Zhou (2018). "Market Intraday Momentum." *Journal of Financial Economics*.
- Glosten, Lawrence R., and Paul R. Milgrom (1985). "Bid, Ask and Transaction Prices in a Specialist Market with Heterogeneously Informed Traders." *Journal of Financial Economics*.
- Hendershott, Terrence, and Mark S. Seasholes (2007). "Market Maker Inventories and Stock Prices." *American Economic Review, Papers and Proceedings*.
- Ilmanen, Antti (2011). *Expected Returns*. Wiley.
- Lehmann, Bruce N. (1990). "Fads, Martingales, and Market Efficiency." *Quarterly Journal of Economics*.
- Wilder, J. Welles Jr. (1978). *New Concepts in Technical Trading Systems*. Trend Research.
