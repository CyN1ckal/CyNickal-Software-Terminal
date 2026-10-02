# SPY RSI(2) dip-buy: pre-registered rules

Written 2026-09-26, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

The request was a strategy for stock indices whose payoff is negatively skewed with a high expected win rate. The study tests the best-known published rule of that shape, Connors' 2-period RSI dip-buy, on SPY. Its shape is built in: buy a short-term oversold close, sell on the first close back above a 5-day average, and never stop out. Many small wins and a few large losses are what the rule is designed to produce. Whether that shape is *paid* is the question.

## Prior exposure

- **Looks at the store before this file.** All by `counts.py`, output in `counts.json`. No price after a decision minute was read, no exit was simulated, and no return, P&L, or hit rate was computed.
  - SPY, QQQ, and IGV each have regular-hours 1-minute bars on 1,254 sessions, 2021-09-27 → 2026-09-25. 2021-12-31 has no 1-minute bars. 2025-01-09 is a closure. The 15:49 decision minute (12:49 on an early close) exists on every session with bars, for all three symbols. QQQ and SPY have no corporate actions. IGV has a 5-for-1 split on 2024-03-07.
  - **The stored daily bars are not regular-hours bars from late 2024 on.** The stored 1d close differs from the last regular-hours minute's close by more than 20 bp on 111 SPY sessions and 168 QQQ sessions, from 2024-11-20 (SPY) and 2024-05-31 (QQQ), and by more than 5 bp on 172 SPY sessions in 2025 alone. On SPY 2025-04-02 the 1d close is 544.83 and the last regular-hours minute closed at 564.52: the 1d bar carries the after-hours print after that day's 16:00 tariff announcement. Opens show the same pattern (pre-market prints). This study therefore **does not use the stored 1d bars at all.** Every open and close is built from regular-hours 1-minute bars. This is a data defect in the store, not in mdq, and it is reported to the user separately.
  - Proxy RSI(2) below 10 at 15:50, on closes built from minutes, fired on SPY on 83 in-sample and 48 out-of-sample sessions; below 5 on 34 and 19; below 10 and above the 200-session average on 26 and 39. QQQ: 71 / 54 below 10. IGV: 81 / 69. These are fire days, not trades. Consecutive fires while a trade is open are one trade. The first session with 20 prior minute-built closes is 2021-10-25. The 200-session average is first available on 2022-07-14.
- **Earlier studies on the same instruments, periods, or mechanism.**
  - `qqq-atr-scale-in` (**Rejected**): intraday fade of a half-ATR move with capped equal adds, flat at the close. Win rate 55%, trade skew −2.23, no gross edge, direction placebo p = 0.95; lost on SPY and IGV too.
  - `qqq-bollinger-adding` (**Rejected**): intraday Bollinger fade with adds. Win rate 67%, skew −2.57, zero-cost OOS Sharpe −0.08; trend days wiped out range days; lost on SPY (OOS Sharpe −1.37).
  - `qqq-atr-martingale` (**Paper-trading candidate**): the same intraday fade, doubled and held overnight until breakeven. OOS Sharpe 1.02 on QQQ, 0.33 on SPY. Its overnight gross gap on held legs summed to +0.75 of starting capital. 20 best days carry the sign.
  - `qqq-intraday-reversion` (**Rejected**): fading 4σ five-minute shocks lost; moves kept going.
  - `igv-small-account-fade` (**Rejected**): one-minute fades on IGV lost gross and net.
  - `qqq-intraday-trend` (**Paper-trading candidate** on QQQ; lost OOS on SPY and IGV): intraday momentum. `qqq-15m-turtle-overnight` (**Rejected**): held overnight gaps lost. `intraday-channel-trend`, `micro-futures-trend`, `qqq-strategy-portfolio` (**Rejected**).
  - `low-liq-high-vol-mean-reversion` (**Rejected**): one-week reversal on small caps; not index, different universe.
  - No earlier study tested a multi-day, close-to-close dip-buy on an index ETF. The mechanism (short-term reversal / liquidity provision) is the one the intraday fades tested at a shorter horizon, and it failed there.
- **What I already know about the test windows.** The OOS window 2024-07-01 → 2026-09-25 is **not unseen**. From the earlier reports: QQQ buy-and-hold rose about 55% in it and SPY had an OOS Sharpe of about 1.2 (`low-liq` report, SPY benchmark), with drawdowns of roughly 20–24%. It contains the April 2025 tariff crash and rebound. The in-sample window contains the 2022 bear market (QQQ about −33%, SPY about −25% drawdown) and the 2023–24 recovery. A long-only book in either window collects drift that I already know was positive overall. The timing placebo below exists to separate dip timing from that drift. I also know, from the data check above, that SPY fell sharply after hours on 2025-04-02, which is common knowledge about the tariff announcement.
- **Where the parameters came from.** Connors and Alvarez (2008) and Connors' published RSI(2) rule: 2-period Wilder RSI, buy in the 0–10 zone, exit on a close above the 5-day simple average, no stop, a 200-day trend filter. RSI length 2 and exit SMA 5 are used unchanged. The entry threshold is 10, the upper edge of Connors' buying zone; Connors reports that below 5 did better on his data, and 5 is in the grid. I chose 10 before any outcome because the sample is five years and the rule is low-frequency: 10 roughly doubles the fire count (83 / 48 vs 34 / 19). The trend filter is dropped from the primary and kept as secondary S1, for the reason under *Primary rule*. The user supplied no parameters.

## Hypothesis

Buying SPY at the close after a short-term oversold reading (RSI(2) < 10) and selling at the first close back above the 5-day average earns a positive net return with a high win rate and a negatively skewed trade distribution, and it does so because of its timing, not only because it is long a rising market.

**Mechanism.** Short-term index declines are partly driven by liquidity demand: forced or impatient sellers (margin calls, volatility-targeting and risk-parity de-leveraging, leveraged-ETF rebalancing at the close) push prices below value, and whoever buys from them is paid a reversal premium for bearing inventory risk (Campbell, Grossman & Wang 1993; Nagel 2012; Cheng & Madhavan 2009). The premium is compensation for crash exposure: the buyer is effectively short a put, winning small and often when liquidity returns and losing large when the decline is information and keeps going. That is the source of both the high win rate and the negative skew (Nagel 2012 shows short-term reversal returns behave like a short volatility position and rise when volatility is high). Connors & Alvarez (2008) document the rule on US index ETFs.

**Known counter-forces.**
- The same reversal premium, tested intraday on QQQ, SPY, and IGV in this store, had no gross edge (`qqq-atr-scale-in`, `qqq-bollinger-adding`, `qqq-intraday-reversion`). Moves that were fading candidates kept going.
- Published anomalies decay after publication (McLean & Pontiff 2016). The RSI(2) rule has been public since 2008.
- Momentum and trend-following flows (time-series momentum, CTA de-risking) extend declines over days, which is the losing tail.
- Dividends are not in the store; an ex-dividend drop can fire a signal on a price move that is not a loss to a holder. SPY's quarterly dividend is about 0.3%.

## Predictions beyond P&L

Scored on the full SPY sample at base cost. They are not acceptance lines (the payoff shape the user asked for is line 7 of the acceptance table).

1. **The tail is fat.** The average losing trade's net return is at least twice the size of the average winning trade's.
2. **Deeper dips pay more.** Trades whose entry RSI was below 5 have a higher average gross return than trades with entry RSI in [5, 10).
3. **The premium rises with volatility** (Nagel 2012). Trades entered with trailing 20-session realized volatility (sample SD of the 20 close-to-close simple returns ending at the previous session with bars) above the median of all trades' entry volatility have a higher average gross return than those below. (A breakdown, not a signal; the median is taken across trades.)
4. **Losses are concentrated.** The worst 10% of trades by net return (rounded up to a whole trade) account for at least 50% of the sum of all losing trades' net returns.
5. **The reversal is fast.** The average gross return of the first held day (entry close → next close) is positive and larger than the average gross return of the later held days.

Each is scored as *consistent*, *not consistent*, or *not testable*.

## Data

- **Instruments.** SPY is the primary. QQQ is the cross-market acceptance check. IGV (a sector ETF, not a broad index) is reported under the same rules and is not an acceptance line. Read with `agent-data/mdq.py`, split-adjusted (only IGV has a split). Dividends are not stored; both the strategy and the benchmark exclude them, which understates both by roughly the dividend yield times exposure.
- **Bars.** Regular-hours **1-minute** bars only (`md.bars(sym, "1m", ...)`). The stored 1d bars are not used (see *Prior exposure*). For each session `d` with bars:
  - `C_d` = close of the session's last 1-minute bar (the regular-hours close; the closing-auction print is not in the store).
  - `O_d` = open of the session's first 1-minute bar.
  - `P_d` = the **decision price**: close of the 1-minute bar whose New York open is 15:49 (12:49 on `mdq.EARLY_CLOSES`), known at 15:50 (12:50). If that minute has no bar, the close of the latest bar opening before it. If there is none, the session has no decision.
- **Calendar.** `mdq.nyse_sessions(2021-10-25, 2026-09-25)` is the return calendar (it includes 2021-12-31 and excludes 2025-01-09). Indicators use only the sequence of sessions **with** 1-minute bars; 2021-12-31 is absent from that sequence, so the change from 2021-12-30 to 2022-01-03 is one step. On 2021-12-31 there is no decision and the return is 0; a held position's move from the 2021-12-30 close is booked on 2022-01-03.
- **Missing bars.** Nothing is forward-filled.
- **Checks the script must pass before it writes results.** The first evaluable SPY session is 2021-10-25. 2021-12-31 is on the calendar and has no bars. Every session with bars has a decision price. The self-test passes. No fill uses a price from a session earlier than the decision.

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| `RSI_N` | 2 | Connors & Alvarez (2008), unchanged |
| `ENTRY_RSI` | 10 | Upper edge of Connors' 0–10 buying zone; chosen a priori for sample size (see *Prior exposure*) |
| `EXIT_SMA` | 5 | Connors, unchanged |
| `TREND_SMA` | none (primary); 200 (S1) | Connors uses 200; dropped from the primary, see below |
| `WARMUP` | 20 sessions with bars | Enough for Wilder RSI(2) to forget its seed (weight 2⁻¹⁸) |
| `COST_BPS` | 1 per side | Protocol default for SPY/QQQ |
| `SIZE` | 1× equity, long only | One position, no leverage |

**Why no trend filter in the primary.** Connors' 200-day filter consumes 200 of the ~1,250 sessions with minute bars, so it cannot trade before 2022-07-14 and removes most of the 2022 bear market. That regime is where a dip-buyer's negative tail lives. A study of whether the negative-skew premium pays for its tail must keep the tail in the sample. The filter also cuts in-sample fires from 83 to 26. The published filtered rule is secondary S1.

**Why long only.** The mechanism is buying from liquidity-demanding sellers in a decline, a put-like premium. Connors' index rule is long-only. A short side would be a separate hypothesis (selling to buyers after rallies) with no put-premium analogue.

1. **RSI.** On the sequence of sessions with bars, with closes `C_1, C_2, …` (index from 2021-09-27): changes `Δ_i = C_i − C_{i−1}`. Wilder averages are seeded at index 2 as `AG = mean(max(Δ_1,0), max(Δ_2,0))`, `AL = mean(max(−Δ_1,0), max(−Δ_2,0))`, then `AG ← (AG·(N−1) + max(Δ,0))/N`, same for `AL`, with `N = 2`. The **proxy RSI** on session `i` updates the averages as of close `i−1` with the proxy change `P_i − C_{i−1}` (without storing that update), and is `100 − 100/(1 + AG'/AL')`. If `AL' = 0` it is 100 (50 if `AG'` is also 0).
2. **Exit average.** `SMA5_proxy_i = (C_{i−4} + C_{i−3} + C_{i−2} + C_{i−1} + P_i) / 5`.
3. **Decision, once per session at 15:50 (12:50 on an early close),** only on sessions from 2021-10-25 on that have a decision price:
   - **In a position:** if `P_i > SMA5_proxy_i`, sell at `C_i`. Otherwise hold. No other exit.
   - **Flat** (including not having exited this session — a session with an exit cannot also enter): if `RSI_proxy_i < ENTRY_RSI`, buy at `C_i`.
4. **Position.** At most one. No adds, no pyramiding, no short side.
5. **Exits.** Only the SMA exit. No stop, no time stop, no profit target. At the end of the sample (2026-09-25) an open position is sold at that session's `C` with reason `end`; it counts as a trade in the window of its entry.
6. **Fills.** Market-on-close style: the order is decided on the 15:49 minute's close and filled at the session's regular-hours close `C_i`. The fill is never on a price known before the decision.
7. **Sizing.** 100% of equity long while in a position; equity compounds. Flat days earn 0.
8. **Returns and costs.** `c = COST_BPS/10000`. Daily return on the return calendar:
   - entry session: `r = −c`;
   - a held session that is neither entry nor exit: `r = C_i / C_prev − 1`, where `C_prev` is the close of the previous session with bars;
   - exit session: `r = (C_i / C_prev)·(1 − c) − 1`;
   - a calendar session with no bars: `r = 0`.
   - Trade gross return `= C_exit / C_entry − 1`; trade net return `= (1 − c)² · C_exit / C_entry − 1`. A win is net > 0.

## Secondary S1: Connors' published rule with the trend filter

Identical to the primary, except an entry also requires `P_i > SMA200_proxy_i = (C_{i−199} + … + C_{i−1} + P_i)/200`. No entry before that average exists (2022-07-14 on SPY). Exits are unchanged (the filter does not force an exit). S1 is never promoted if the primary fails.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| `P_i` (15:49 minute close) | 15:50 of session i | Decision at 15:50 of session i |
| `C_{i−1}, …, C_{i−199}` | 16:00 of earlier sessions | Decision at session i |
| Wilder averages | Close of session i−1 | Proxy RSI at session i |
| Fill `C_i` | 16:00 of session i, after the decision | Fill only, never the decision |
| Delay variant: `C_i` then `O_{i+1}` | 16:00 i; open i+1 | Decision on C_i, fill at the next open |
| Trailing 20-session volatility for prediction 3 | Close of session i−1 | Breakdown only |
| Stored 1d bars | — | Not used |

No full-sample normalization, quantile, or volatility target enters a signal.

## Samples

- **Warm-up:** 2021-09-27 → 2021-10-22 (the first 20 sessions with bars). Not in the return calendar.
- **In-sample:** 2021-10-25 → 2024-06-28.
- **Out-of-sample:** 2024-07-01 → 2026-09-25.
- The split matches the earlier studies in this store, so results are comparable; the OOS window is exposed (see *Prior exposure*). Daily returns belong to their date's window. A trade belongs to the window of its entry session.

## Benchmarks

- **SPY buy and hold**, close to close on regular-hours minute closes (`C_i / C_prev − 1`), uncosted, same calendar, 0 on 2021-12-31. The headline benchmark.
- The **timing placebo** (below) is the exposure-matched benchmark: random long holds with the same durations.

## Reported checks

- **Metrics** for full / IS / OOS at 1 bp: the protocol defaults (total return, CAGR on a 252-session year, annualized volatility, Sharpe = mean/sample SD × √252 of daily returns with undefined when SD is 0, max drawdown of compounded equity, t-stat of the mean daily return, trades, win rate, trade profit factor = Σ winning net trade returns ÷ |Σ losing net trade returns|, average net trade in bp, exposure = share of calendar sessions with a position held at the close), plus average winner and loser, trade skewness (Fisher–Pearson `g1` of net trade returns), median and mean holding sessions, and the skewness of daily returns on sessions in a position.
- **Breakdowns:** calendar year; exit reason; entry RSI bucket (< 5, 5–10); entry volatility half; quintile of SPY's same-session close-to-close return, by daily strategy return; days held.
- **Costs:** 0, 0.5, 1, 2, 3 bp per side (a full rerun at each).
- **Fill delay:** D1 = decisions at each session's close on actual closes (RSI updated with `C_i`, `SMA5 = mean(C_{i−4..i})`), filled at the open `O_j` of the next session `j` with bars. At the close of session `i`: if a position is held, apply the exit test; otherwise apply the entry test (a position exited at the open of `i` is flat at its close and may signal a new entry). Returns: entry session `j`: `r = (1 − c)·C_j/O_j − 1`; held sessions: `C/C_prev − 1`; exit session `j`: `r = (O_j/C_prev)·(1 − c) − 1`; no bars: 0. An order pending after the last session is not filled; an open position is sold at the last close with reason `end`. **Upper bound** U = decisions on actual closes `C_i`, filled at `C_i`, with the primary's return formulas (Connors' published backtest convention; look-ahead, labelled).
- **Direction placebo** on gross daily returns: each trade's gross daily returns multiplied by one independent ±1. 2,000 draws, `numpy.random.default_rng(20260926)`. `p = (1 + #draws ≥ actual)/2001` on the full-sample gross Sharpe. For a long-only book in a rising market this is a weak test; the timing placebo is the one that matters.
- **Timing placebo** on gross daily returns: each draw keeps the primary's trades' occupancy lengths (entry session through exit session, in calendar sessions), shuffles their order, and places them at uniformly random non-overlapping positions on the full return calendar (uniform composition of the free sessions into `n + 1` gaps), long. A placed trade earns 0 on its entry session and `C_i/C_prev − 1` on each later session through its exit (0 on sessions without bars). 2,000 draws, `default_rng(20260927)`. `p = (1 + #draws ≥ actual)/2001` on the full-sample gross Sharpe.
- **Block bootstrap:** circular 20-session blocks of full-sample net daily returns, 2,000 draws, `default_rng(20260928)`; 95% interval of Sharpe and share ≤ 0.
- **Plateau grid on IS:** `ENTRY_RSI ∈ {5, 10, 15, 20, 25}` × `EXIT_SMA ∈ {3, 5, 10}`, 15 cells, at 1 bp. OOS is shown for selection bias only. An undefined Sharpe counts as not > 0.
- **Cross-market:** identical rules on QQQ (acceptance) and IGV (reported). IGV cost 2 bp per side, as in earlier studies (thinner ETF); QQQ 1 bp.
- **S1** metrics, placebos, 2× cost, and QQQ, for its own acceptance lines.
- **Verification:** a self-test on synthetic sessions before the store is opened, covering: proxy extraction at 15:49, at 12:49 on an early close, and the fallback to an earlier minute; Wilder RSI with `AL = 0`; entry; hold; SMA exit; no re-entry on an exit session; a session without bars while holding; the end-of-sample close; the S1 filter; the D1 next-open fill; return and cost arithmetic. `verify.py` independently rebuilds closes from minutes and replays **every** SPY and QQQ trade, matching entry date and price, exit date and price, and exit reason.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 1 bp per side. If line 6 fails, the verdict is **Inconclusive**.

1. OOS Sharpe ≥ 0.5 and OOS trade profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 **and** timing placebo p ≤ 0.05, both on the full-sample gross Sharpe.
3. IS Sharpe > 0, and at least 9 of the 15 IS grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 2 bp per side.
5. QQQ OOS Sharpe > 0 under identical rules.
6. Minimum sample: at least 25 OOS trades. Below that, **Inconclusive**.
7. **The payoff the user asked for:** full-sample net win rate ≥ 60% and full-sample net trade skewness < 0.

Changes from the protocol defaults, with reasons: line 2 adds the timing placebo, because a long-only book in a rising market beats a direction flip on drift alone; line 6 is lowered from 100 to 25 OOS trades, because the rule fires on about 48 OOS sessions and holds several days, so 100 OOS trades is not reachable in 2.2 years — inference rests on the 562 daily returns, and with 25 trades the win-rate standard error is about 9 points, which the report will state; line 7 is added because the user's brief is a payoff shape, and a profitable rule without that shape does not answer it.

**S1 acceptance** (reported, never promoted over a failed primary): lines 1, 2, 4, 5, and 7 on S1 at 1 bp, and at least 20 OOS trades or S1 is Inconclusive. No grid line for S1.

A failed line fails the strategy.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell (including RSI < 5); S1 when the primary fails; QQQ or IGV alone; a stop-loss, time stop, or profit target added after the run; a volatility or regime filter found in the breakdowns; a short side; the upper-bound close fill; a different cost, sample split, or benchmark; dividend adjustments added after the run.

## Seeds

20260926 (direction placebo), 20260927 (timing placebo), 20260928 (bootstrap).

## References

- Campbell, J. Y., S. J. Grossman, and J. Wang (1993). Trading Volume and Serial Correlation in Stock Returns. *Quarterly Journal of Economics* 108(4).
- Cheng, M., and A. Madhavan (2009). The Dynamics of Leveraged and Inverse Exchange-Traded Funds. *Journal of Investment Management*.
- Connors, L., and C. Alvarez (2008). *Short Term Trading Strategies That Work*. TradingMarkets Publishing.
- McLean, R. D., and J. Pontiff (2016). Does Academic Research Destroy Stock Return Predictability? *Journal of Finance* 71(1).
- Nagel, S. (2012). Evaporating Liquidity. *Review of Financial Studies* 25(7).
- Wilder, J. W. (1978). *New Concepts in Technical Trading Systems*. Trend Research.
