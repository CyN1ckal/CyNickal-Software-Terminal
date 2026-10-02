# QQQ daily-ATR-band dip buy, flat at the close: pre-registered rules

Written 2026-09-30, before any return, P&L, forward return, or hit rate was computed from the store. Results may not edit this file. Anything computed after the first store run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** Counts and coverage only, by `research/qqq-atr-band-dip-eod/research/counts.py`, which writes `counts.json`. No price after a signal bar was read; no outcome statistic was computed.
  - QQQ, SPY, IGV each have 1-minute bars on 1,254 sessions, 2021-09-27 → 2026-09-25. One QQQ session in the evaluation window has no tape: 2021-12-31 (the known calendar artifact; NYSE was open, no 1m rows stored). 2025-01-09 is not on the calendar. Ten evaluation sessions are early closes with 211 bars; all others (1,236) have 390. QQQ and SPY have no corporate actions stored; IGV has a 5-for-1 split on 2024-03-07 and no dividends.
  - Daily bars, split-adjusted: QQQ through 2026-09-23, so the last two sessions carry the 2026-09-23-fixed ATR forward. Wilder ATR(14) seed is fixed at the 2021-10-06 daily close; the first session that may use it is 2021-10-07.
  - **Signal counts, no outcome.** Sessions whose 1-minute low touches `open − K × prior ATR`, at most one per session, K as a multiplier of a full prior-day ATR:

    | K | 0.5 | 0.75 | **1.0** | 1.25 | 1.5 |
    |---|---:|---:|---:|---:|---:|
    | IS (2021-10-07 → 2024-06-28) | 239 | 130 | 68 | 35 | 14 |
    | OOS (2024-07-01 → 2026-09-25) | 184 | 97 | 56 | 29 | 17 |

    K = 1 is the user's stated band, chosen before this count. The count was used only to set the minimum-sample line (acceptance 6), not to change K. At K = 1 the signal fires on about 10% of sessions.

- **Earlier studies on the same instruments, periods, or mechanism.**
  - `qqq-atr-scale-in`: fade a 5-minute **close** 0.5 ATR away from the session open, up to 3 units, target 0.25 ATR, flat by the close. **Rejected**; OOS Sharpe −0.57, OOS PF 0.78. Its symmetric fade of a move of a fraction of an ATR off the open lost money on this store.
  - `qqq-atr-martingale`: same 0.5-ATR-off-open first entry, doubling adds, held overnight to breakeven. **Paper-trading candidate**; OOS Sharpe 1.02. Its edge came from averaging down over multiple days, not from a one-shot dip-to-close rebound.
  - `qqq-intraday-reversion` (no folder in this layout; figures recorded in other studies): fade 4σ 5-minute shocks. **Rejected**; OOS Sharpe −0.76. The average path after an extreme shock kept going.
  - `qqq-intraday-trend`: time-of-day noise-band momentum, flat nightly. **Paper-trading candidate**; OOS Sharpe 0.82, concentrated in a few large-move days. Momentum in the direction of a day's move worked on QQQ OOS. That is the opposite sign to this hypothesis on the same window.
  - `spy-rsi2-dip-buy`: buy SPY closes after 2-day RSI dips, hold days. **Paper-trading candidate**. Different horizon and trigger; same "dip buying in a drifting market" family.
  - `intraday-channel-trend`, `qqq-15m-turtle-overnight`, `index-opening-pop-fade`, `qqq-bollinger-adding`: rejected. The last of these fades 5-minute Bollinger excursions intraday — the closest mechanism to this study after the two ATR studies above — and lost.
- **What I already know about the test windows.** The OOS window 2024-07-01 → 2026-09-25 is **not unseen data**; six earlier studies evaluated on it. From their reports: QQQ buy-and-hold returned +55.4% OOS (Sharpe 1.01, max DD −22.9%); it contains the April 2025 tariff crash and the 2025-04-09 rebound; intraday momentum made money on QQQ there; single-shot intraday fades lost on QQQ; a 0.5-ATR-off-open fade lost, while the multi-day martingale version of that entry won. The IS window contains 2022, when QQQ fell about a third. I know none of the touch sessions' outcomes — no band-touch return was ever computed here before the lock.
- **Where the parameters came from.** The user's idea: "buy at the daily ATR band and rebound it to end of day." Asked to resolve the ambiguity, the user chose the band **anchored at the session open minus 1 prior-day ATR**, a **touch** trigger, **no stop, no target, flat at the close**, and **no time-of-day or gap filter**. That choice was made before any look at prices (the count table above came after the choice and changed nothing in it). No parameter was taken from chart inspection. ATR(14) and Wilder smoothing are published defaults (Wilder 1978), used unchanged. Cost 1 bp/side is the protocol default for QQQ-class liquidity.

## Hypothesis

On QQQ, on the roughly one session in ten whose trade prints at or below the session open minus one prior-day Wilder ATR(14), the price is on average higher at the 16:00 close than at that band, enough to cover 1 bp per side — i.e., a one-shot, all-day dip buy at a full daily-ATR dislocation from the open reverts by the close.

**Mechanism.** The buyer at a full ATR below the open is providing immediacy to price-insensitive selling: leveraged and inverse fund rebalancing sells into declines, vol-control and option-hedging flows sell delta into weakness, and dealers carrying short option gamma are forced sellers before the close (Cheng and Madhavan 2009; Baltussen, Da, Lammers and Martens 2021; the inventory story in Hendershott and Seasholes 2007). Such flow is not information, and it must finish today; the reversion is the fee the liquidity provider is paid for warehousing it until the counterparty is done (Hendershott, Jones and Menkveld 2016 on the modern provider side). The band is one full ATR because that is the published scale of an entire normal day's range (Wilder 1978) — a dislocation of one range unit from the open is unlikely to be pure noise — and the horizon is the close because the forced flow is intraday. Related published result: intraday reversal after large first-half moves (Barclay-Hirsch-Levinovitz 2019, CME) argues reversion over an overnight horizon, not within the day — cited here because it bounds what this study should expect.

**Known counter-forces.** Adverse selection (Glosten and Milgrom 1985): a full-ATR slide can be information, and informed selling does not revert by the close. Market intraday momentum (Gao, Han, Li and Zhou 2018) and this store's own results — `qqq-intraday-trend` positive, `qqq-intraday-reversion` and `qqq-atr-scale-in` negative on QQQ OOS — say that within-day moves on this window tended to persist. A close-anchored exit is also the moment the closing auction's flow can push a falling tape down hardest. The report must show the distribution's left tail: trend-down days are exactly where this rule is long.

## Predictions beyond P&L

If the mechanism is right, then, on the full QQQ sample at 1 bp per side:

1. **Early touches rebound; late ones do not.** Forced flow front-loads (hedgers rebalance near the open and re-hedge before the close). Mean net trade return for touches filled by 11:30 New York exceeds the mean for touches filled after 14:00. Both means come from the same trades.csv.
2. **The edge scales with the dislocation unit.** Forced-flow size and the noise floor both scale with realized volatility, so per-trade net return rises with the band's width relative to price: the mean net trade return in the top quintile of `ATR / session open` exceeds the mean in the bottom quintile.
3. **It is index flow, not a QQQ artifact.** The identical rule on SPY has a positive mean net trade return on the full sample (its OOS Sharpe > 0 is acceptance line 5; this prediction asks about the mechanism's cross-instrument sign, not its Sharpe).

Each one is scored *consistent*, *not consistent*, or *not testable*. These scores do not enter the acceptance table. A book can pass the table with the wrong shape; the report then says the mechanism is unconfirmed. A book with the right shape and a failed table is rejected.

## Data

- **Instruments.** QQQ is the primary. SPY is the cross-market acceptance check. IGV is reported under identical rules and is not an acceptance line. Read with `agent-data/mdq.py`, split-adjusted. Only IGV has a split (5-for-1 on 2024-03-07), so its prior true ranges are not read as crashes. Dividends are not stored and not adjusted; the book is flat overnight, so no dividend is held.
- **Bars.** Signals and fills use **1-minute bars** directly (no resampling). A bar's timestamp is its open; the 09:30 bar covers 09:30:00–09:30:59. The daily ATR uses split-adjusted daily bars. A daily close is known at 16:00 of its session even though its timestamp is 09:30; only the prior session's ATR is ever used.
- **Calendar.** Evaluation sessions are `mdq.nyse_sessions` from 2021-10-07 through 2026-09-25 (1,247). That calendar excludes 2025-01-09 and includes 2021-12-31. Early closes are `mdq.EARLY_CLOSES` (10 in the window): the session ends 13:00 and there are 211 bars.
- **Missing bars.** A minute with no trade has no row. Nothing is forward-filled. A session with no tape (2021-12-31) contributes return 0 and no trade. If the touch minute happens to be a minute with no row, no touch occurred in it by definition; the check runs on stored rows only.
- **Checks the script must pass before it writes results.** QQQ's first evaluation session is 2021-10-07 and has a 09:30 first bar. Every session with a tape has 390 bars, or 211 on an early-close date. The ATR used on a session was fixed on a strictly earlier session and is positive. 2021-12-31 is on the calendar and has no bars. The last session with a daily bar is 2026-09-23; 2026-09-24 and 2026-09-25 carry that ATR forward (counted in results.json).

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| `K` | 1.0 | The user's stated band, chosen before any look. |
| `ATR_N` | 14 | Wilder (1978), used unchanged. |
| `SIZE` | 1.0× | The user asked for a simple dip buy. All of start-of-day equity, no leverage. |
| `COST_BPS` | 1 | Protocol default for QQQ-class liquidity; wider than QQQ's quoted spread for a small order. |
| Exits | session close only | The user's stated idea: "rebound it to end of day." No stop, no target. |
| Filters | none | The user's stated choice. |

1. **ATR.** On QQQ split-adjusted daily bars sorted by session: true range of a bar after the first is `max(high − low, |high − prev close|, |low − prev close|)`; the seed is the arithmetic mean of the first 14 true ranges, fixed at the 14th bar's close (2021-10-06); thereafter `ATR = (13 × ATR_prev + TR) / 14`, fixed at that bar's close. On session *d*, use `A` = the latest ATR fixed on a session strictly earlier than *d*. If missing or ≤ 0, session *d* is not tradable (return 0).
2. **Session open.** `S` = the open of the first 1-minute bar of *d*, which must be timestamped 09:30 New York. If no tape or no 09:30 bar, *d* is not tradable.
3. **Order.** At the first bar's open (09:30:00) place **one** day limit-buy order to buy at the band `B = S − K × A`, for 100% of start-of-day equity (fractional shares allowed). The order is live from 09:30:00 and is cancelled at the session's end unfilled.
4. **Fill.** The fill is at the first 1-minute bar *j* with `low_j ≤ B`. Fill price = `min(B, open_j)` (a bar opening below the band fills at its better open; otherwise at the band). Exactly one entry per session; the order is removed once filled. A touch on bar 1 (09:30) is a legal fill; `open_1 = S > B` always, so bar-1 fills are at `B`.
5. **Exit.** Flatten the entire position at the close of the session's last 1-minute bar (15:59 close on a regular session, 12:59 close on an early close), reason `session`. That is the only exit. There is no stop, no target, and no overnight hold: any position at the session's end is closed.
6. **Sizing and returns.** Equity starts at 1 and compounds once per session at the last mark; no intraday compounding. Entry notional = start-of-day equity; shares = equity / fill price. Gross session return = `close_exit / fill_price − 1` on trades; 0 on sessions with no trade or no tradable session. Costs: `COST_BPS` of notional on entry and again on exit, so net = gross − 2 × cost on a traded session. Borrow and cash interest are zero (flat overnight, long only).
7. **Trades.** One `trades.csv` row per traded session: side `long`, entry time (bar *j*'s open timestamp) and fill price, exit time and price, gross and net session return, reason `session`. Trades belong to the sample of their entry session; no trade crosses the IS/OOS split.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Wilder ATR `A` | 16:00 of a strictly earlier session | Order placement at 09:30:00 of session *d* |
| Session open `S`, band `B` | 09:30:00 of *d* (first bar's open) | Same instant; the order is live from then |
| Touch test `low_j ≤ B` | during bar *j* | fill at bar *j*, price `min(B, open_j)`; nothing about bar *j* after that timestamp is used to fill it |
| Exit price | last bar's close (16:00 / 13:00) | the exit itself |
| Daily close of *d* | 16:00 of *d* | never a signal input; feeds only session *d*+1's ATR and the buy-and-hold benchmark |
| Quintile edges (report) | computed post-run, whole sample | reporting only, never trading |

No rolling statistic uses the session itself; no z-score, quantile or vol target is estimated on the evaluation sample; the universe is these fixed symbols on every date.

## Samples

- **Warm-up:** QQQ daily bars 2021-09-16 → 2021-10-06 (ATR seed and any smoothing before the first session). Not in the return calendar.
- **In-sample:** 2021-10-07 → 2024-06-28 (685 sessions, 68 touch sessions).
- **Out-of-sample:** 2024-07-01 → 2026-09-25 (562 sessions, 56 touch sessions).
- The split is the one every study on this store uses, so results are comparable. It is not a fresh split; the OOS window is exposed (see Prior exposure). The dates were fixed before any look (the count table came after, and only sized the sample line).

## Benchmarks

Both uncosted, on the same evaluation sessions.

- **Buy and hold.** Daily close to daily close, split-adjusted. Where a session has no daily bar (2026-09-24, 2026-09-25), the mark is that session's last 1-minute close; the script records how many marks were minute closes.
- **Open to close.** Last 1-minute close / 09:30 bar open − 1 on sessions with a tape; 0 otherwise.

## Secondary candidates

None registered. The single idea, as stated by the user, is the primary. (Parameter alternatives live in the plateau grid and are never selected from.)

## Reported checks

All of these appear in the report whatever they show.

- **Metrics** full / IS / OOS at 1 bp per side, QQQ, and the same trio for SPY and IGV: protocol defaults. Sharpe = mean daily simple return / sample SD × √252, zero risk-free, no-position days = 0. Total return, CAGR on a 252-session year, annualized vol, max drawdown of compounded equity, t-stat of the mean daily return, trades, win rate (share of trades with positive net dollars), profit factor (Σ winning net dollars ÷ |Σ losing net dollars|), average net trade in bp, exposure (share of 1-minute bars, tradable sessions, holding the position). Holding time: minutes from fill to exit.
- **Breakdowns:** calendar year (compounded return, Sharpe, max DD with peak reset at year start); entry hour (half-hour buckets of the fill timestamp); quintile of `ATR / S` (ranked on tradable sessions with a trade by a stable sort, `min(i × 5 // N, 4)`); quintile of the session's |open→close| move (all tradable sessions with a tape); day's total return outcome by touch vs non-touch sessions (mean close/S − 1 both groups — descriptive, not a trade).
- **Costs:** 0, 0.5, 1, 2, 3 bp per side, same fills.
- **Fill delay:** the primary (resting limit at the band) plus a conservative variant: detection at bar *j*'s low, fill at bar *j+1*'s open (if *j+1* exists; else the fill is *j*'s close). No same-bar-close upper bound is meaningful for a limit-at-band entry; the primary is already an advance order.
- **Direction placebo** on gross returns, fixed notional: each traded session contributes `±(exit/fill − 1)`; the actual uses +1; each of 2,000 draws flips every trade's sign independently by a fair coin (`default_rng(20260930)`); p = (1 + draws with gross Sharpe ≥ actual) / 2001, full sample. (With one-sided trades the placebo tests the timing/level, not a side; stated plainly.)
- **Timing placebo:** 500 draws, `default_rng(20260931)`, QQQ gross fixed-notional Sharpe. For each traded session the draw picks a uniformly random 1-minute bar (not the first) as entry, fills at that bar's open, same EOD exit. Non-traded sessions contribute 0 on every draw. p = (1 + draws ≥ actual) / 501. Not an acceptance line.
- **Block bootstrap:** circular, 20-session blocks of the full-sample net daily returns, 2,000 draws, `default_rng(20260932)`; report the 2.5th/97.5th percentiles of Sharpe and the share of draws ≤ 0.
- **Plateau grid on IS:** `K ∈ {0.50, 0.75, 1.00, 1.25, 1.50}`, everything else fixed, 1 bp, QQQ. 5 cells; the primary cell is K = 1.00. Report IS Sharpe and trades per cell, OOS Sharpe per cell for selection bias only, the primary's IS rank, and how many cells have Sharpe > 0. Nothing is selected. A cell with undefined Sharpe counts as not > 0.
- **Cross-market:** identical rules on SPY and IGV, each own ATR, own equity from 1, 1 bp/side (IGV's true spread exceeds 1 bp on many prints; the number is identical-rules, not a cost claim). Pairwise correlation of the three net daily return series.
- **Verification.** `backtest.py` runs the synthetic self-test below before opening the store and aborts on any failure. `verify.py` is an independent re-implementation (naive minute loop, no shared code) replaying every QQQ evaluation session; every trade must match on side, entry time, entry price, exit time, exit price, reason.
- **Seeds:** direction 20260930, timing 20260931, bootstrap 20260932, verify sample 20260933. NumPy `default_rng` only.

**Self-test paths** (hand-built 1-minute sessions, expected trades hand-computed): a touch filling mid-session at the band; a bar whose open is below the band filling at the open; no touch (no trade); touch on bar 1; touch on the last bar (fills at the band, flattens at the same close, gross = close/B − 1); a gap-down open below the band on bar 1 (fills at open_1 < B); the j+1-open variant when bar *j* is the last bar; early close flatten at the 12:59 close; ATR missing or ≤ 0 → no trade, return 0; no tape → return 0; costs: net = gross − 2 bp on a touch day; only the first touch fills when several bars are below the band; carry-forward ATR when the last daily session is two days back.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 1 bp per side. If line 6 fails, the status is **Inconclusive**, even if other lines also fail. If line 6 holds and any other line fails, the status is **Rejected**.

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full-sample gross Sharpe.
3. In-sample Sharpe > 0, and at least 60% of the 5 IS grid cells (3 of 5) have Sharpe > 0.
4. Full-sample total return > 0 at 2 bp per side.
5. SPY out-of-sample Sharpe > 0 under identical rules at 1 bp per side. IGV is not this line (its 1 bp understates its cost).
6. At least **40** OOS QQQ trades. Below that, **Inconclusive**.

Line 6 is lowered from the protocol default of 100 before the lock, with a written reason: the signal is a full daily ATR below the open, which by construction fires on about one session in ten — the pre-lock count (56 OOS touches) fixes the reachable sample, and it was never adjusted after. Every other threshold is the protocol default.

A failed line fails the strategy.

## Not done in this study

The report will not promote in place of the primary: another grid K (including 0.5, which trades 4× as often); an entry-time, gap, or volatility filter; a stop or target; the prior-close-anchored band or the 5-minute-close trigger of `qqq-atr-scale-in`; a short-side or mirror "pop fade"; a different exit time than the close; holding overnight; adding units or a martingale stake; a different cost, fill, or sample split; dropping 2022 or April 2025; SPY or IGV as the primary; a long-only-with-the-drift variant presented as this rule.

## References

- Baltussen, Da, Lammers, Martens (2021). "Hedging Demand and Market Intraday Momentum." *J. Financial Economics*.
- Barclay, Hirsch, Levinovitz (2019). "The Intraday Reversal." *Journal of Future Markets* (CME working paper).
- Cheng & Madhavan (2009). "The Dynamics of Leveraged and Inverse Exchange-Traded Funds." *J. Investment Management*.
- Gao, Han, Li, Zhou (2018). "Market Intraday Momentum." *J. Financial Economics*.
- Glosten & Milgrom (1985). "Bid, Ask and Transaction Prices..." *J. Financial Economics*.
- Hendershott, Jones, Menkveld (2016). "Electronic Liquidity Provision." *J. Political Economy*.
- Hendershott & Seasholes (2007). "Market Maker Inventories and Stock Prices." *AER P&P*.
- Wilder (1978). *New Concepts in Technical Trading Systems*. Trend Research.
