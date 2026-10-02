# Commodity ETF cross-sectional momentum: pre-registered rules

Written 2026-10-02, before any return, P&L, forward return, or hit rate was computed on these series. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `research/commodity-etf-momentum/research/counts.py` (output `counts.json`), read through `agent-data/mdq.py`. No strategy return, forward return, hit rate, or P&L.
  - GLD, SLV, USO, UNG, DBA, and DBB each have 3,959 daily bars, first session 2011-01-04, last session 2026-10-01. No duplicate sessions, no non-positive OHLC, no zero-volume bars. In that window each name's sessions match SPY's sessions exactly (0 missing, 0 extra).
  - Coverage records 4,108 rows per name: 4,103 complete, 4 missing, 1 partial, 3,959 with bars. The zero-bar `complete` rows are holidays. The partial row is 2021-12-31, one real daily bar, note `expected: real session the terminal's calendar calls a holiday; the bar is good`. One missing row is 2025-01-09, note `expected: market closed`; `mdq` already treats that day as a holiday, so `nyse_sessions` omits it. The other three missing rows are 2012-10-29, 2012-10-30, and 2018-12-05, each with note `no bars: re-run ingest for this session`. Those three dates are inside `nyse_sessions` (3,963 sessions from 2011-01-01 through 2026-10-01 include all three). They are not sessions in this study.
  - The 253rd own bar of each name is 2012-01-04. The first SPY month-end on or after that date is 2012-01-31. There are 178 SPY month-ends from 2012-01-31 through 2026-10-01, and 190 SPY month-ends from 2011-01-31 through 2026-10-01. These are counts of dates, not outcomes. 2024-07-01 is a SPY session. The last SPY session before it is 2024-06-28.
  - Corporate actions: USO split ratio 0.125 on 2020-04-29; UNG split ratio 0.25 on 2018-01-05 and 0.25 on 2024-01-24. GLD, SLV, DBA, and DBB have none. No dividends are stored on any of the six.
  - Split continuity, ex-date close divided by the previous session's close. This is a jump check, not a result. USO 2020-04-29: raw ratio 8.451 versus 8.0 for an unadjusted 1-for-8 reverse split; adjusted ratio 1.056. UNG 2018-01-05: raw 3.894 versus 4.0; adjusted 0.974. UNG 2024-01-24: raw 4.187 versus 4.0; adjusted 1.047. The adjusted series does not contain the reverse-split jump. `backtest.py` re-checks the inequality (raw ratio > 3 and adjusted ratio between 0.5 and 1.5) and aborts if it fails. It does not target the residual ratio.
- **Earlier studies on the same instruments, periods, or mechanism.** No study in this repo used GLD, SLV, USO, UNG, DBA, or DBB. `micro-futures-trend` (Rejected, 2026-09-26) is the related study. It traded time-series momentum (sign of 1-, 3-, and 12-month returns, volatility-scaled, monthly) on CME micro futures, open-to-close only, with equities and rates in the same book. Its commodity sleeve was MGC (gold), MHG (copper), and MCL (crude). It did not trade silver, natural gas, agriculture, or these ETFs. Its evaluation was 2022-10-03 → 2026-09-25, and its OOS window was 2024-10-01 → 2026-09-25.
- **What I already know about the test windows.** This study's OOS window, 2024-07-01 → 2026-10-01, overlaps that futures OOS window and is **not unseen data**. From `micro-futures-trend`'s rules and report, before any return was computed here: gold rallied through 2024–2025; crude drifted lower over 2023–2025; managed-futures trend was strong in 2022, lost money in 2023, was roughly flat to modestly positive in 2024, and drew down in the first half of 2025 around the April tariff shock. From that study's results: the whole futures book had no gross edge (full Sharpe −0.33, OOS −0.19, OOS return −7.8%). The commodity sleeve was negative in sample (Sharpe −0.64) and positive out of sample (Sharpe +0.88), full net P&L +$1,638, full Sharpe +0.19. Inside the sleeve, gold was −$3,613 (Sharpe −0.12 full, +0.73 OOS), copper +$729 (Sharpe +0.12 full, +0.64 OOS), and crude +$4,521 (Sharpe +0.35 full, +0.14 OOS). That is a different rule on different instruments. It is still prior exposure to gold, copper, and crude over 2022–2026. Separately, as public product history and not as a measured return from this store: USO and UNG embed a collateralized futures roll and have spent long stretches trending down. They stay in the universe. The rule is allowed to be short them. Equity studies in this repo used an OOS window starting 2024-07-01 on QQQ, SPY, and IGV. From those reports, QQQ buy-and-hold returned about +55% in that window and the window contains the April 2025 tariff crash and rebound. Those are not commodity-ETF results. No return on these six series has been computed.
- **Where the parameters came from.** The ranking window, the one-month hold, and the long-winner short-loser construction are the commodity-futures momentum schedule in Miffre and Rallis (2007), which builds on Erb and Harvey (2006). Both are used as the reason for a 12-month rank held to the next month, not as a license to copy a quintile breakpoint onto six names. Book size 2, the decision not to skip a month, the 2024-07-01 split, and the two cost levels were fixed in the study request before any return on these series. Reasons are in the parameter table. They are not to be changed after the run.

## Hypothesis

Among GLD, SLV, USO, UNG, DBA, and DBB, the two with the highest prior 12-month price return outperform the two with the lowest over the next month, after the costs below.

**Mechanism.** Commodity futures momentum comes from slow-moving hedgers and from underreaction (Miffre and Rallis 2007; the 12-month rank held for one month is the schedule Erb and Harvey 2006 found profitable in commodity futures, and the schedule that was the most profitable cell in Miffre and Rallis). The other side of the trade is a hedger, or a rebalancer, who leans against the move and keeps doing so because the hedge or the mandate is the point of the trade, not the momentum P&L. The same paper finds that the profitable momentum books buy backwardated contracts and sell contangoed ones. This study does not observe the futures curve and does not add that filter.

**Known counter-forces.** The roll embedded in a collateralized commodity ETF can dominate the spot move, particularly in USO and UNG, so a long book of those names can lose while the futures curve is in contango. A dollar-neutral rank can still lose when the winners reverse together, which is what a sharp risk-off month does. Six names is a narrow cross-section: one name can be the whole result. Published momentum has multi-year droughts. None of these is a reason to drop a name after the run. The predictions below are how the mechanism is scored.

## Predictions beyond P&L

If the mechanism is right, then, on the primary book:

1. Full-sample gross price P&L, on the $1 start, is positive on the long trades and positive on the short trades. Gross means price P&L before costs. A result that is only a long-side drift, or only a short of the known weak products, does not satisfy this.
2. Leave-one-out: full-sample gross price P&L stays positive when each one name is removed from the universe and the same rule is run on the other five (top 2 and bottom 2 of the remaining five, weights still ±1/2). If any of the six leave-one-out sums is not positive, the breadth prediction is not consistent. This check is not a new primary and cannot be promoted.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- Instruments: GLD, SLV, USO, UNG, DBA, DBB. All six are always in the primary universe. Read with `agent-data/mdq.py`, daily bars, split-adjusted (the default). Dividends are not stored and are not invented. SPY daily bars date the month-ends. SPY is not traded.
- Prices are adjusted-share prices. Share counts live in that same adjusted space. The engine does not multiply shares by a split ratio on an ex-date. Doing so on an already adjusted series would invent a jump. The self-test feeds a smooth series across a labelled split date and requires the book's price P&L to be the smooth move only.
- Calendar: book sessions are `mdq.nyse_sessions` from 2011-01-04 through 2026-10-01, minus 2012-10-29, 2012-10-30, and 2018-12-05. Those three are in the calendar and have no bars. They are skipped, not entered as zero-return days. 2025-01-09 is already outside `nyse_sessions`. 2021-12-31 is a book session because it has a real daily bar. Early closes have a normal daily bar (open and close of the shortened session) and need no separate rule. A daily bar's `ts` is 09:30 ET; its close is known only at the session close (16:00 ET, or 13:00 ET on an early close).
- Missing bars: a name with no bar on a book session earns 0 that session. Its mark stays at the most recent close. The missing session is not inserted into that name's bar list, so it is not a bar in the formation window and it is not a forward-filled signal. In this store, after the three dates above are removed, every book session has a bar for all six names and for SPY. The missing-bar rule is still part of the engine.
- Checks the script must pass before it writes results: each of the six has 3,959 daily bars, first 2011-01-04, last 2026-10-01; no duplicate sessions; no non-positive OHLC; corporate actions equal the three splits above and nothing else; on each of those three ex-dates the raw close ratio versus the previous session is greater than 3 and the adjusted close ratio is between 0.5 and 1.5; the book-session list has 3,959 dates and does not contain the three skipped closures.

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| Universe | GLD, SLV, USO, UNG, DBA, DBB, all of them, every date | Frozen before the sample. Products with a long public downtrend stay in |
| Formation `K` | 252 own daily bars | 12-month commodity ranking window (Erb and Harvey 2006; Miffre and Rallis 2007). No skip |
| Book size | 2 | With N=6 a published quintile is 1.2 names. 2 is the smallest book that is not a single-name bet. Not to be cut to 1 after the run |
| Hold | Until the next monthly fill | The one-month holding period of that same schedule |
| Weights | +1/2, +1/2, 0, 0, −1/2, −1/2 | Gross long 1, gross short 1, dollar neutral at the sizing instant |
| GLD, SLV cost | 1 bp per side | SPY-class default. These two are liquid |
| USO, UNG, DBA, DBB cost | 5 bp per side | Round prior. Those four are thinner than SPY and the store has no quotes. Not a measured spread, and not to be revised after P&L |
| Starting equity | 1.0 | Unit book. Fractional shares. No borrow fee, no cash interest. The store has no borrow quotes; charging zero borrow is a limitation, stated here, not a fitted cost |

1. **Signal dates.** The last book session of each calendar month that has a SPY daily bar. Group book sessions by year and month. The last session in the group is the signal date. A signal uses that session's close and is not acted on until a later open.
2. **Formation.** For each name, let the name's own daily bars with session on or before the signal date be `b[0..n)`, in session order. If `n < K+1`, the name is not rankable. Otherwise the formation return is `b[n-1].close / b[n-1-K].close − 1`. The last bar is the signal-date bar when that bar exists, and the most recent earlier bar when it does not. No session is inserted to stand in for a hole. There is no one-month skip: the window includes the most recent month. The equity 12-1 skip exists to step over short-term stock reversal. That is a different rule, and it is not used here.
3. **Rank.** If fewer than six names are rankable, the rebalance is skipped: shares and any still-unfilled targets stay as they are. Otherwise sort by formation return descending, and break exact float ties by symbol ascending (DBA, DBB, GLD, SLV, UNG, USO). The alphabetically earlier symbol ranks higher on a tie. The first 2 are winners, the last 2 are losers, the middle 2 are flat. Target weights are +1/2, +1/2, 0, 0, −1/2, −1/2 in that sorted order.
4. **Pending targets.** The signal replaces any unfilled target for that name. Only the latest target is eligible. It fills at that name's next own bar with session strictly after the signal date, at that bar's open. A name that has no such bar is not filled. Shares are constant between fills. There is no stop, no target, and no volatility scaling.
5. **Accounting on a book session.**
   - A name with a bar and a previous close marks from that close to today's open. Old shares earn the gap: `old_shares × (open − last_close)`. A name with no bar earns 0 and keeps its last close. A name with no previous close, or with zero shares, has gap 0.
   - Mark-to-open equity is cash plus old shares marked at today's open where a bar exists and at the last close where it does not. Every fill on this session is sized off this one equity number, before today's costs and before any of today's trades.
   - A name that fills today trades from old shares to `target_weight × equity_open / open`. Cash decreases by `delta_shares × open + cost`. Cost is `abs(delta_shares) × open × rate`. The rate is the name's bp per side times the cost multiple, divided by 10,000. The primary multiple is 1.
   - New shares then earn the open-to-close: `new_shares × (close − open)`. The mark becomes today's close. A name that did not have a bar does not update its mark.
   - The same arithmetic on a day with no fill is old shares earning close-to-close, because old and new shares are equal.
6. **Terminal exit.** The last book session is 2026-10-01. After the open's fills and the open-to-close, every name still held is flattened at that session's close, or at the last close if the name has no bar. Cost is charged on the absolute notional. There is no further price P&L, because the exit price is the mark. Exit reason `end`. A signal whose next open does not exist is not filled. 2026-10-01 is itself a month-end. Its signal is computed and then discarded. The flatten is not a same-bar fill of that signal. It is the end of the sample, and it is the only close fill in the primary.
7. **What is not charged.** No borrow, no commission beyond the bp rate, no cash interest. A flip pays one cost on the absolute share change, which equals the notional closed plus the notional opened. That cost is split between the two trades in proportion to those two notionals. Opening from flat, closing to flat, and a same-sign resize put the whole cost on that one trade. The terminal flatten's cost goes to the trade it closes.

## Trade identity

A round trip is one name, one sign, from the fill that opens a nonzero position of that sign until the fill that sets the shares to 0 or to the opposite sign.

- A same-sign change in share count is a resize. It stays inside the open trade. Entry date, entry price, and entry shares stay at the opening fill. Those entry shares are the denominator of the trade's return.
- The gap into an exit open belongs to the trade that is closing. The open-to-close after a fill belongs to the trade that holds the new shares.
- Exit reasons are `flip`, `flat`, and `end`.
- Trade gross P&L is the price P&L attributed to the trade. Trade net P&L is gross minus the costs attributed to it. Return in bp is `net_pnl / (abs(entry_shares) × entry_price) × 10,000`.
- A trade is in-sample if its entry fill is before 2024-07-01, and out-of-sample if its entry fill is on or after 2024-07-01. Profit factor, win rate, and the round-trip count use that split. The whole trade is in one sample even when its daily P&L crosses 2024-07-01. Daily Sharpe does not use this split. It uses the session date. Both facts are reported. Neither is chosen after seeing which one looks better.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Signal-date close | Session close of the signal date (16:00 ET, or 13:00 ET on an early close) | Rank after that close |
| Own-bar closes in the formation window | Those sessions' closes, all on or before the signal date | Rank after the signal close |
| Mark-to-open equity | The fill session's open | Sizing of fills at that open, before the trade |
| Fill price | That name's next open after the signal | The fill |
| Terminal exit price | The last session's close, after open-to-close has been earned | Flatten after that close, because no later open exists |
| Costs, book size, K, universe | Constants, fixed in this file | Everywhere |

No statistic is estimated on the full sample. Costs, weights, and K are constants. The formation window uses only bars on or before the signal date. A fill never uses a price from before the signal close as a same-bar execution, except the labelled upper bound below, which is not the primary.

## Samples

- **Warm-up.** The 252 own bars a name needs before it can be ranked. No P&L is recorded in the warm-up.
- **First signal.** The first signal date on which all six names are rankable at K=252. From the count above, that date is expected to be 2012-01-31. The rule is authoritative if the count is wrong.
- **In-sample.** The first fill session through 2024-06-28, the last book session before 2024-07-01.
- **Out-of-sample.** 2024-07-01 through 2026-10-01.
- **Why this split.** It is the repo's existing OOS start, chosen so this study shares that boundary. It was fixed before any return on these series. The overlap with `micro-futures-trend` and with the equity studies is disclosed above. The OOS window is not unseen.
- **Which days enter the Sharpe.** Evaluation sessions only: the first fill through 2026-10-01. A day inside that window with no position and no trade has return 0 and stays in the mean and the standard deviation. Warm-up days do not.

## Benchmarks

Equal-weight long-only of the same six names. Target weight +1/6 each. Same signal dates, starting at the primary's first signal date, same next-open fills, same gap and open-to-close accounting, cost multiple 0. No terminal cost. Its daily returns are aligned to the primary's evaluation sessions. It is uncosted by construction. It is not a second primary.

## Secondary candidates

None. There is no second primary.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, IS, and OOS at base cost, for the primary and the benchmark: total return; CAGR on a 252-session year; annualized volatility (sample SD × √252); Sharpe (mean ÷ sample SD × √252, sample SD with delta degrees of freedom 1, zero risk-free rate); max drawdown of equity compounded from 1 over that window's daily returns only; t-stat of the mean daily return; trades; win rate; profit factor (sum of positive net trade P&L ÷ absolute sum of negative net trade P&L); average net trade in bp; exposure. Exposure is the fraction of window sessions on which any shares are nonzero after the open's fills and before a terminal flatten. A trade with net P&L of 0 is neither a win nor a loss. If there are no losing trades, the profit factor is reported as null and line 1 fails.
- Gross Sharpe, full, IS, and OOS: daily price P&L divided by the previous close's net equity, same Sharpe formula. Costs stay in the equity denominator because the shares are the shares the costed book held. They are not subtracted from the numerator.
- Breakdowns: calendar year (compounded return, Sharpe, max drawdown, benchmark return); long versus short; exit reason (`flip`, `flat`, `end`); per name (gross P&L, net P&L, trades); calendar-month strategy return by quintile of the benchmark's calendar-month return. A month is any calendar month that contains an evaluation session. Monthly return compounds that month's daily net returns. Months are sorted by benchmark monthly return ascending, ties by month ascending. Month i in that list, from 0, is in quintile `(i × 5) // n + 1`. Quintile 1 is the worst benchmark months.
- Costs: multiples 0, 0.5, 1, 2, and 3 of each name's own bp rate. Each multiple is a full resimulation, so equity and share counts change. Report full-sample Sharpe, OOS Sharpe, and full-sample total return.
- Fill delay: the fill is the second own bar strictly after the signal, instead of the first. Same sizing at that later open. Report full and OOS Sharpe and full return.
- Same-bar close fill, labelled upper bound, not a verdict input: on the signal session, after old shares have earned that session's close-to-close, rebalance at the close off mark-to-close equity before cost. New shares earn nothing more that day. A name with no bar on the signal date cannot fill at a close and keeps the next-open rule. In this store that hole does not occur.
- Direction placebo: each completed trade keeps its timing and its daily gross price P&L, and that whole path is multiplied by an independent ±1. 2,000 draws, seed 20261021. Daily gross return uses the actual primary book's previous net equity as the denominator. Compare the draw's gross Sharpe with the actual gross Sharpe on the full evaluation window. p = (1 + number of draws with Sharpe ≥ actual) / 2001.
- Timing placebo: 500 draws, seed 20261023. At each signal date, permute the six formation returns across the six names and re-rank with the same tie-break. Resimulate at base cost. Score is full-sample gross Sharpe. p = (1 + number of draws with Sharpe ≥ actual) / 501. This is a reported check, not an acceptance line.
- Block bootstrap: circular blocks of 20 evaluation sessions, 2,000 draws, seed 20261022, of the full-sample net daily returns. If the length is not a multiple of 20, draw enough blocks to cover it and truncate to the original length. Report the 2.5 and 97.5 percentiles of the draw Sharpes.
- Plateau grid, IS only for the acceptance line: K ∈ {63, 126, 189, 252, 315}, book size fixed at 2. Five cells. Each cell's first signal is the first month-end on which all six names are rankable at that K, and its IS window is that cell's first fill through 2024-06-28. OOS is shown for selection bias only. Nothing is selected from the grid. The K=252 cell's IS Sharpe must equal the primary IS Sharpe.
- Cross-market: not run. See acceptance line 5.
- Verification: a self-test on synthetic sessions before the store is opened, covering a clean rank, an exact tie, a missing bar (skipped session earns 0, formation uses the most recent real close, a fill waits for the next open and is sized off that later equity), a smooth path across a labelled reverse-split date, a flip, and the last-session exit. `verify.py` shares no signal code with `backtest.py`. It recomputes the book and must match every trade on symbol, side, entry date, entry price, exit date, exit price, and exit reason. Prices match to 1e-6 absolute. Seed 20261024 is recorded for a draw of 40 rebalance dates; the pass condition is the full trade list, which contains any such draw.

## Acceptance

The primary is a **paper-trading candidate** only if every line below holds at base cost. Line 5 is not a line of this study.

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. IS Sharpe > 0, and at least 60% of the IS grid cells have Sharpe > 0. Five cells, so at least 3.
4. Full-sample total return > 0 at 2× base cost.
5. Not applicable. The futures series were already used by `micro-futures-trend` and are not these ETFs, and no second commodity-ETF set is in the store. This line is not a pass and not a fail. It is not in the verdict.
6. At least 24 OOS round trips. A round trip is the trade defined above. It is OOS when the entry fill is on or after 2024-07-01. The terminal flatten completes the trades it closes, so those count. Reason for lowering the protocol's 100: a monthly rebalance over the OOS month-ends cannot be asked for 100 independent round trips without shortening the holding period that was pre-registered. The other lines are not lowered.

**Verdict map, fixed here.** If line 6 is not met, the status is **Inconclusive**, and the other lines are still reported. If line 6 is met and any of lines 1–4 fails, the status is **Rejected**. If lines 1, 2, 3, 4, and 6 all pass, the status is **Paper-trading candidate**.

A failed line fails the strategy. A grid cell, one side, or one name does not replace the primary.

## Not done in this study

The report will not promote any of these in place of the primary:

- dropping USO or UNG, or any other name, because of its P&L;
- keeping only the short side, or only the long side;
- switching to a top-1 book;
- adding a one-month skip;
- volatility scaling, a stop, or a different weight;
- using the futures study, or its commodity sleeve, as a second primary or as a cross-market pass;
- moving the sample split;
- a grid cell other than K=252;
- a different cost, a different fill, or the same-bar close upper bound.
