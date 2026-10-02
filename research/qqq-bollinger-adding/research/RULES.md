# QQQ intraday Bollinger reversion with adding: pre-registered rules

Written 2026-09-26, before any return, P&L, forward return, or hit rate was computed from the store. Results may not edit this file. Anything computed after the first store run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** Counts only, through `research/qqq-bollinger-adding/research/counts.py`, which writes `counts.json`. Every count compares a 5-minute close with the band built from that close and the 19 closes before it in the same session. No later price was read.
  - QQQ, SPY, and IGV each have 5-minute bars on 1,254 sessions, 2021-09-27 → 2026-09-25: 1,244 sessions of 78 bars and 10 early closes of 43 bars. IGV has 15 sessions of 77 bars and 15 decision bars whose next bucket is missing. QQQ and SPY have none. 2021-12-31 is an NYSE session with no bars (the known gap). QQQ and SPY have no corporate actions stored. IGV has a 5-for-1 split on 2024-03-07.
  - With a 20-bar session-local window, the first band exists at the 11:10 close on every session.
  - Legal closes (next bucket exists and fills before the cutoff below) with |z| above 2: QQQ 3,579 in sample and 2,528 out of sample, on 678 and 541 sessions. |z| above 3: 166 and 131 closes. |z| above 4: 3 and 3. The largest |z| a close can have inside its own 20-close window is 19/√20 = 4.25. Sessions with a 2σ break below the lower band: 481 / 402; above the upper band: 500 / 366 (in sample / out of sample; not exclusive). SPY and IGV are similar (`counts.json`).
  - The median band half-width (2σ as a fraction of the middle band) at those QQQ breaks is 28.8 bp in sample and 23.0 bp out of sample.
  - 541 out-of-sample sessions with a break is a lower bound on campaigns and is above the 100-campaign minimum, so the study is feasible. No parameter was moved after this count.
- **Backtest ledgers already in the store.** `python agent-data/mdq.py ledgers` lists three ledgers named "Bollinger band reversion QQQ 1m" (ids 2, 3, 4), created 2026-09-26 14:24–14:26 UTC with the terminal's `bollinger_revert` strategy, parameters length 20, deviations 2, direction long. Their fills run from 2024-01-02 or 2026-01-02 through 2026-09-25, inside this study's out-of-sample window. I read only the listing (name, fill count, first and last fill, parameters). I did not open their fills, round trips, book, or P&L, and I do not know their result. Whoever ran them may have seen it in the terminal, so the idea itself may carry knowledge of how a 1-minute, long-only, hold-overnight version did on QQQ in 2024–2026. This study differs from those runs on bar size, direction, session handling, and adding.
- **Earlier studies on the same instruments, periods, or mechanism.**
  - `qqq-atr-scale-in`: fade a half-ATR move from the session open on 5-minute QQQ bars, add equal units at 1.0 and 1.5 ATR up to three, take profit a quarter ATR past the average, flatten at the close. **Rejected.** Out-of-sample Sharpe −0.57, zero-cost out-of-sample Sharpe −0.38, direction placebo p = 0.95. Campaigns that added lost more than one-unit campaigns made. SPY out-of-sample Sharpe −0.53. This is the closest earlier study: same bar size, same unit size, same cap, same cutoff, same close flatten. It differs in the anchor (a moving 20-bar mean instead of the open), the unit of distance (the band's SD instead of prior-day ATR), and the exit (the moving middle band instead of a fixed target).
  - `qqq-atr-martingale`: the same entry, doubling units, held overnight until the round trip nets at least zero. **Paper-trading candidate** on QQQ (out-of-sample Sharpe 1.02), with a blow-up in one grid neighbor and a path through zero on IGV.
  - `qqq-intraday-reversion` (folder not in this layout; figures from other reports): fading 4σ 5-minute shocks on QQQ. **Rejected**, out-of-sample Sharpe −0.76. The average path after a shock kept going.
  - `igv-small-account-fade`: one-minute fades in IGV. **Rejected**, out-of-sample Sharpe −3.82, zero-cost −0.32.
  - `qqq-intraday-trend`: noise-boundary momentum on QQQ, flat overnight. **Paper-trading candidate**, out-of-sample Sharpe 0.82, lost on SPY and IGV, flat since May 2025.
  - `intraday-channel-trend`: 15-minute Donchian breakout on QQQ, SPY, IGV. **Rejected**, no gross edge out of sample.
  - Taken together: intraday fades on this store have lost, including one with adds on the same bars, and intraday momentum on QQQ has made money. That lowers the prior that this study passes. It does not decide the test.
- **What I already know about the test windows.** The out-of-sample window 2024-07-01 → 2026-09-25 is **not unseen data**. From the reports above: QQQ buy-and-hold returned +55.4% on it (Sharpe about 1.0, max drawdown about −23%); it contains the April 2025 tariff crash and the 2025-04-09 rebound; intraday momentum made money on QQQ and lost on SPY and IGV; fades anchored at the open, and fades of 4σ shocks, lost on QQQ. The in-sample window contains 2022, when QQQ fell about a third. From the count above, out-of-sample QQQ has more sessions with a lower-band break (402) than an upper-band break (366). The rule stays symmetric.
- **Where the parameters came from.** The user asked for "QQQ intra-day, Bollinger band reversion, with adding," with no numbers. Length 20 and width 2 population SD on closes are Bollinger's published defaults (Bollinger 2001), and the terminal's `CBollinger` study and `bollinger_revert` strategy use the same defaults and the same population SD. The entry on a close outside the band and the exit on a close back at the middle band are the terminal strategy's rule. The adding rule, the unit size, the cap, and the cutoff are reasoned below, and the last three are the same as `qqq-atr-scale-in` so the two studies can be compared.

## Hypothesis

On QQQ 5-minute bars, a close outside a 20-bar, 2-SD Bollinger band built from the same session is followed often enough by a return to the band's middle that a book which fades it, adds up to two more equal units at each further one-SD step past its worst fill, exits at the middle band, and is flat by the close, has a positive expected return after 1 bp per side.

**Mechanism.** A close two standard deviations away from the recent mean is, on this view, a price concession to someone who needed to trade now. Liquidity providers take the other side and are paid when the imbalance clears (Grossman and Miller 1988; Campbell, Grossman and Wang 1993). The return to supplying liquidity is larger when the move is larger and when volatility is high (Nagel 2012). On an index ETF the impatient side includes leveraged and inverse ETF rebalancing and hedging flows (Cheng and Madhavan 2009). They keep paying because their mandates require them to finish the trade, not to time it. Under the inventory models, the concession grows with the size of the imbalance, so a unit added further from the mean is bought at a larger concession. That is the case for adding. The middle band is the estimate of where price stood before the pressure, so it is the exit.

**Known counter-forces.** Bollinger himself says a tag of the band is not a signal by itself: in a trend, price walks the band (Bollinger 2001). Intraday momentum is documented in index ETFs (Gao, Han, Li and Zhou 2018), and hedging demand from option dealers who are short gamma pushes price in the direction of a large move (Baltussen, Da, Lammers and Martens 2021). The informed-trader side of Glosten and Milgrom (1985) says the flow that pushes price furthest is the most likely to be informed, which makes the adds the worst trades, not the best. On this store, `qqq-atr-scale-in` found exactly that. The middle band also moves toward price during a trend, so a long can exit "at the middle" below its own average entry. The report splits results by the size of the day's move, by unit count, and by leg, which is where these forces show up.

## Predictions beyond P&L

If the mechanism is right, then, on the full QQQ sample at 1 bp per side:

1. **Range days pay and trend days do not.** Tradable QQQ sessions are ranked by |last 5-minute close / 09:30 five-minute open − 1| and split into quintiles as defined under Reported checks. The mean net daily strategy return on the pooled sessions of quintiles 1 and 2 (the two quietest) is positive, and the mean on quintile 5 is negative.
2. **Adding is paid.** Per leg, the gross return from the leg's fill to its campaign's exit, signed by side, is larger on average for added legs (the second and third units) than for first legs. If fewer than 30 added legs exist, this prediction is *not testable* rather than failed.
3. **The payoff is many small wins and a few large losses.** The campaign win rate (net dollars > 0) is above 55%, and the moment skewness of per-unit net campaign returns is negative. Skewness is g1 = m3 / m2^1.5, with central moments divided by N.

Each one is scored *consistent*, *not consistent*, or *not testable*. These scores do not enter the acceptance table. A book that passes the table with failed predictions has not confirmed the mechanism, and the report will say so.

## Data

- **Instruments.** QQQ is the primary. SPY is the cross-market acceptance check. IGV is reported under the same rules and is not an acceptance line. Read with `agent-data/mdq.py`, split-adjusted (only IGV has a split, 5-for-1 on 2024-03-07). Dividends are not stored and are not adjusted. The book is flat overnight, so no dividend gap is held.
- **Bars.** Decisions use 5-minute bars built by `mdq.resample(md.bars(sym, "1m"), 300)`: anchored at 09:30 New York, never spanning a session, timestamp = open, close = last 1-minute close in the bucket, partial buckets emitted. For the buy-and-hold benchmark only, split-adjusted daily bars.
- **Calendar.** The evaluation calendar is `mdq.nyse_sessions("2021-09-27", "2026-09-25")`. It includes 2021-12-31 (no bars; a zero-return, no-trade session) and excludes 2025-01-09. Early closes are the dates in `mdq.EARLY_CLOSES`, where the session ends at 13:00.
- **Missing bars.** Nothing is forward-filled. The band uses the session's existing bars by count. An order fills only on the bar exactly `DELAY × 300` seconds after the signal bar, and every 300-second step in between must exist. Otherwise no order is scheduled from that close.
- **Checks the script must pass before it writes results.** QQQ has 5-minute bars on 1,254 sessions and 2021-12-31 has none. Every QQQ session with bars has a 09:30 bar and is tradable. The calendar has 1,255 sessions from 2021-09-27 through 2026-09-25. Full sessions have 78 bars and early closes 43.

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| `BAR_S` | 300 | Five-minute bars, the bar size of `qqq-atr-scale-in`, so the two are comparable. At 1 bp per side a round trip is 2 bp. The pre-lock count put the median distance from a 2σ break to the middle band at 23–29 bp on 5-minute bars, so the cost is under a tenth of the target distance. |
| `N` | 20 | Bollinger (2001) default; the terminal's `CBollinger` default. |
| `K` | 2.0 | Bollinger (2001) default; the terminal's default. |
| SD | population (÷ N) | Bollinger (2001); the terminal's `CBollinger`. |
| `STEP` | 1.0 | An add needs a close at least one band SD beyond the campaign's worst fill. One SD is the band's own unit, and half the distance from the band to the middle, so an add is a clearly further move and not the next tick of the same break. |
| `MAX_UNITS` | 3 | Two adds after the first unit. Equal units, as in `qqq-atr-scale-in`. |
| `COST_BPS` | 1 | Protocol default for QQQ-class liquidity. One cent on a price above $300 is under 1 bp. |
| `CUTOFF_MIN` | 30 | No entry or add fills in the last 30 minutes (Gao et al. 2018 treat the last half-hour as its own momentum period); same as `qqq-atr-scale-in`. |
| `DELAY` | 1 | A signal at a bar's close fills at the next bar's open. |

1. **Bands.** For each session, index its 5-minute bars `i = 0, 1, …` in time order. For `i ≥ N − 1`, the middle band `M_i` is the arithmetic mean of the closes of bars `i − N + 1 … i`, `S_i` is their population standard deviation `sqrt(Σ(c − M_i)² / N)`, the lower band is `L_i = M_i − K·S_i`, and the upper band is `U_i = M_i + K·S_i`. The window never reaches into another session. Bars `0 … N − 2` have no band and make no decision. On a regular QQQ session the first decision is at the 11:10 close.

2. **When a close may schedule a fill.** Bar `i` may schedule an order only if it has a band, it is not the session's last bar, and the fill bar at `ts_i + DELAY × 300` exists with every intermediate 300-second step. An **entry or add** additionally needs the fill bar's New York open to be strictly earlier than the session end minus 30 minutes. The session end is 16:00, or 13:00 on an early close. So a regular-day entry can fill at 15:25 and not at 15:30, and an early-close entry can fill at 12:25 and not at 12:30. **Exits** have no cutoff. At most one order is pending. While an order is pending, no new decision is made.

3. **Position.** At most one campaign at a time, long or short, with 1 to 3 units. Each session starts flat. A campaign is flat before another can start. An exit never opens the other side on the same fill. The bar whose open filled an exit may, at its own close, schedule a new entry.

4. **Decisions at a close `c = close_i` that may schedule, in this order.**
   - **Long with `n` units:** if `c ≥ M_i`, schedule an exit of every unit, reason `middle`. Else if `n < 3`, `c < L_i`, and `c ≤ min(fills) − STEP·S_i`, schedule one added unit. Otherwise hold.
   - **Short with `n` units:** if `c ≤ M_i`, schedule an exit, reason `middle`. Else if `n < 3`, `c > U_i`, and `c ≥ max(fills) + STEP·S_i`, schedule one added unit. Otherwise hold.
   - **Flat:** if `c < L_i`, schedule one long unit. Else if `c > U_i`, schedule one short unit.
   - The inequalities are exactly as written: the entry is strict (`<`, `>`), the exit is inclusive (`≥`, `≤`), the add's distance from the worst fill is inclusive. Entries and adds that fail the cutoff are not scheduled. At most one unit fills per bar. An exit and an add cannot both hold on one close, because an add needs `c < L_i ≤ M_i`. If `S_i = 0` no entry or add is possible, because `c < M_i − 0` is impossible when every close in the window equals `M_i`.

5. **Exits.** The only planned exit is the middle band above, filled at the next bar's open. There is no stop and no time exit. If a campaign is open at the close of the session's last bar, every unit exits at that close, reason `session`, and any pending order is cancelled. With `DELAY = 1`, a middle-band close on the last bar does not get a next open, so its reason is `session`. The middle band moves, so an exit at the middle can be at a loss against the campaign's average entry. That is part of the rule.

6. **Fills.** The fill price is the scheduled bar's open, except the session flatten, which is the last bar's close. A unit filled at a bar's open is part of the campaign when that bar's close is judged.

7. **Sizing.** Equity starts at 1 and compounds once per session, at the session's last mark. The unit notional on a session is one third of equity at the previous session's mark, and every unit opened that session uses it. Three units are 100% of start-of-day equity. No leverage. Fractional shares are allowed. A leg's gross dollars are `side × (exit / entry − 1) × unit notional`, with side +1 long, −1 short. Each leg pays `COST_BPS` basis points of the unit notional on entry and again on exit. A campaign's net dollars are the sum over its legs. The session's return is the sum of its campaigns' net dollars divided by start-of-day equity. A session with no campaign, and 2021-12-31, return 0. Shorts are assumed locatable. Borrow and cash interest are zero because the book is flat overnight.

8. **Tradable session.** The session has bars, its first bar opens at 09:30, and its last bar opens at 15:55 (12:55 on an early close). Otherwise it stays on the calendar with return 0.

9. **Costs on the sweep.** The same fills repriced at 0, 0.5, 1, 2, and 3 bp per side.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| `M_i`, `S_i`, `L_i`, `U_i` | Close of bar `i` (uses closes of bars `i − 19 … i` of the same session) | The decision at the close of bar `i`, which fills no earlier than bar `i + 1`'s open |
| Signal close `c` | Close of bar `i` | Same decision |
| Worst fill | The opens of units already filled | Later closes of the same campaign |
| Session flatten | The last bar's close, the last stored regular-hours print | The same close. No later bar exists |
| Daily close (buy-and-hold benchmark only) | 16:00 of its session | The benchmark, not a signal |
| Quintile edges of the day's move | The whole evaluation sample | Scoring prediction 1 only. Not a trading input |

No statistic is estimated on the evaluation sample and then used to trade. The band uses only the current session's bars up to the decision bar. The universe is these three ETFs on every date.

## Samples

- **Warm-up:** none. The band is session-local, and the first 19 bars of every session are its warm-up.
- **In-sample:** 2021-09-27 → 2024-06-28 (693 calendar sessions, 692 with bars).
- **Out-of-sample:** 2024-07-01 → 2026-09-25 (562 sessions).
- This is the split used by the earlier studies on this store, so the windows can be compared. It is not fresh. The out-of-sample window is exposed, as stated under Prior exposure. A campaign belongs to the sample of its session. No campaign crosses a session, so none crosses the split.

## Benchmarks

Both are uncosted and are reported on the same QQQ calendar.

- **Buy and hold.** Daily close to daily close, split-adjusted. Where a session has no daily bar but has 5-minute bars, the mark is its last 5-minute close. 2021-12-31 uses its daily bar if one exists; otherwise it returns 0 and the next session is measured from the last available mark. The first session's return is its mark over its 09:30 five-minute open, minus one. The script records how many marks came from a 5-minute close.
- **Open to close.** The last 5-minute close divided by the 09:30 five-minute open, minus one. A session without bars is 0.

The summary table's benchmark is buy and hold. Neither benchmark is a trading rule.

## Reported checks

All of these appear in the report, whatever they show.

- **Metrics** for full, in-sample, and out-of-sample at 1 bp per side for QQQ, and the same trio for SPY and IGV: the protocol defaults. Sharpe = mean daily simple return ÷ its sample SD × √252, zero risk-free rate, flat sessions count as 0. CAGR on a 252-session year. Max drawdown of compounded equity, with the window's equity restarted at 1. t-stat of the mean daily return. Profit factor = Σ winning campaigns' net dollars ÷ |Σ losing campaigns' net dollars|. Win rate = share of campaigns with net dollars > 0. Average net trade (bp) = mean over campaigns of net dollars ÷ (units × unit notional) × 10,000. Exposure = share of 5-minute bars on tradable sessions from each campaign's first fill bar through its exit bar, inclusive. Holding time = bars from first fill bar through exit bar, inclusive.
- **A campaign** is one row of `trades.csv`: side, units, each leg's fill time and price, the average entry price (arithmetic mean of leg prices), entry time (first fill), exit time, exit price, reason, gross and net dollars, gross and net per-unit bp.
- **Breakdowns:** calendar year (the year's compounded return, Sharpe, and max drawdown, restarted at 1); long vs short; exit reason (`middle`, `session`); New York hour of the first fill; the day-move quintiles; unit count; first vs added legs.
- **Quintiles.** Rank tradable QQQ sessions by |last 5-minute close / 09:30 open − 1| with a stable sort ascending. The session at 0-based position `p` of `T` sessions is in quintile `min(5p // T, 4) + 1`.
- **Costs:** 0, 0.5, 1, 2, 3 bp per side, QQQ, same fills.
- **Fill delay:** the primary, plus `DELAY = 2` (fill at the open 600 s after the signal close; both steps must exist; the entry cutoff applies to that fill bar). Upper bound, labelled: `DELAY = 0`, fill at the signal bar's close; the state updates and that bar is not judged again; an entry or add needs that close time (`ts_i + 300`) strictly earlier than the session end minus 30 minutes; on the last bar a middle-band close exits with reason `middle` at that close and anything else exits with reason `session`.
- **Direction placebo** on gross returns, fixed notional. A campaign's contribution to its session is `Σ_legs side × (exit / entry − 1) / 3`. Sessions without campaigns are 0. Each draw multiplies each campaign's contribution by an independent ±1. 2,000 draws, `numpy.random.default_rng(20260926)`. `p = (1 + #draws with Sharpe ≥ actual gross Sharpe) / 2001`, full sample.
- **Timing placebo,** QQQ only, reported, not an acceptance line. 500 draws, `default_rng(20260927)`, gross fixed-notional Sharpe as above. In each session the primary's campaigns are placed in order. Each placebo campaign keeps its side, its leg offsets (bars from the first fill), and its holding offset (bars from first fill to exit bar). Legal start bars are indices `≥ N` whose open is strictly before the entry cutoff and strictly after the previous placebo campaign's exit bar in that session; the start is drawn uniformly among them, and if none exist the campaign is dropped from that draw. A leg whose offset lands on a bar at or after the cutoff, or at or after the exit bar, is dropped. If the primary campaign's reason was `session`, the placebo exits at the session's last close; otherwise at the open of the bar at the holding offset, or the last close if that bar does not exist. `p = (1 + #draws ≥ actual) / 501`.
- **Block bootstrap:** circular 20-session blocks of full-sample net daily returns, 2,000 draws, `default_rng(20260928)`. The 2.5th and 97.5th percentiles of Sharpe and the share of draws ≤ 0.
- **Plateau grid on the in-sample window:** `N ∈ {10, 20, 30}` × `K ∈ {1.5, 2.0, 2.5}` × `STEP ∈ {0.5, 1.0, 1.5}`, 27 cells, everything else fixed, 1 bp, QQQ. The primary is (20, 2.0, 1.0). The out-of-sample Sharpe of every cell, and of the cell ranked first in sample, is shown for selection bias only. Nothing is selected. A cell with undefined Sharpe counts as not > 0. The primary's in-sample rank is reported.
- **Diagnostics, reported only, never promoted:** D1, the primary with `MAX_UNITS = 1` (no adding), on QQQ, so the effect of adding is visible. D2, the primary with the band computed over the last 20 five-minute bars regardless of session (the terminal chart's continuous convention; the first decision can be the 09:35 close), on QQQ.
- **Cross-market:** identical rules on SPY and IGV, each with its own equity starting at 1, 1 bp per side. IGV's quoted spread is often wider than 1 bp; its 1 bp figure is the identical-rules number, not a cost claim. The pairwise correlation of the three daily strategy-return series is reported.
- **Verification.** `backtest.py` runs a self-test on hand-built sessions before it opens the store and aborts on any failure. Paths: a one-unit long that exits at the middle; a one-unit short that exits at the middle; a long that adds twice and flattens at the session close; an add refused because the close is below the band but not a full STEP below the worst fill; an add refused at the cap; an entry refused by the 15:30 cutoff while an exit after 15:30 is allowed; an early-close cutoff at 12:30; a close whose next bucket is missing; no decision before bar `N − 1`; a middle-band close on the last bar that exits as `session`; an exit followed by a re-entry on the exit bar's close; `DELAY = 2` and `DELAY = 0`; costs on three units. `verify.py` is a separate implementation that shares no signal code and replays every QQQ session at the primary settings. It must match every campaign on side, units, leg times and prices, exit time, exit price, and reason.
- **Seeds:** 20260926 (direction placebo), 20260927 (timing placebo), 20260928 (bootstrap).

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 1 bp per side. If line 6 fails, the status is **Inconclusive**. If line 6 holds and any other line fails, the status is **Rejected**.

1. QQQ out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full-sample gross Sharpe.
3. In-sample Sharpe > 0, and at least 17 of the 27 in-sample grid cells (60% of 27 is 16.2, rounded up) have Sharpe > 0.
4. Full-sample total return > 0 at 2 bp per side.
5. SPY out-of-sample Sharpe > 0 under identical rules. IGV is not this line: 1 bp understates IGV's cost, and "either of two names" would let that understatement decide the test.
6. At least 100 out-of-sample QQQ campaigns.

Line 5 is tighter than the protocol's "at least one related instrument," for the reason given. Every other threshold is the protocol default.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone, including the long-only direction of the ledgers already in the store; the no-adding book D1; the continuous-window book D2; 1-minute bars; IGV or SPY alone; a time-of-day, volatility, or trend filter (including a band-width or ADX filter); a stop loss; a doubled stake; a different exit band; a different cost, fill, or sample split; holding overnight.

## References

- Baltussen, G., Z. Da, S. Lammers, and M. Martens (2021). "Hedging Demand and Market Intraday Momentum." *Journal of Financial Economics* 142(1).
- Bollinger, J. (2001). *Bollinger on Bollinger Bands*. McGraw-Hill.
- Campbell, J. Y., S. J. Grossman, and J. Wang (1993). "Trading Volume and Serial Correlation in Stock Returns." *Quarterly Journal of Economics* 108(4).
- Cheng, M., and A. Madhavan (2009). "The Dynamics of Leveraged and Inverse Exchange-Traded Funds." *Journal of Investment Management* 7(4).
- Gao, L., Y. Han, S. Z. Li, and G. Zhou (2018). "Market Intraday Momentum." *Journal of Financial Economics* 129(2).
- Glosten, L. R., and P. R. Milgrom (1985). "Bid, Ask and Transaction Prices in a Specialist Market with Heterogeneously Informed Traders." *Journal of Financial Economics* 14(1).
- Grossman, S. J., and M. H. Miller (1988). "Liquidity and Market Structure." *Journal of Finance* 43(3).
- Nagel, S. (2012). "Evaporating Liquidity." *Review of Financial Studies* 25(7).
