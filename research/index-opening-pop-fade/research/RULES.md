# Index opening-pop fade: pre-registered rules

Written 2026-09-26, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

The request was "a strategy for selling into intra-day pops on stock indices." This study tests one specific, published form of that idea: short an index ETF when its first half hour of regular trading is an unusually large rise, and cover at the close.

## Prior exposure

- **Looks at the store before this file:** `counts.py` (kept in this folder, output in `counts.json`). For SPY, QQQ, and IGV, 2021-09-27 → 2026-09-25, it checked bar presence and counted signal fires. It read no price after 10:30 except the last bar's timestamp, and computed no return after a signal.
  - Every session with bars starts with a 09:30 bar. 2021-12-31 has no bars (known calendar defect). Normal sessions end with a 15:59 bar. The ten early closes in the window end with a **13:00** bar (211 bars), not 12:59.
  - SPY and QQQ have a 09:44/09:45, 09:59/10:00, and 10:29/10:30 bar on every session. IGV falls back to an earlier or later minute on 49–69 sessions. No splits in SPY or QQQ. IGV split 5:1 on 2024-03-07 (mdq adjusts).
  - Fires of the primary signal (09:30 open → 09:59 close ≥ 1.0 × trailing 60-session RMS), first evaluable session 2021-12-21: **SPY 96 IS / 82 OOS**, QQQ 99 / 90, IGV 109 / 82. The secondary (prior close → 09:59 close): SPY 90 / 75, QQQ 92 / 96. Grid counts are in `counts.json`; the thinnest cells (z = 2.0) have 9–19 fires per window.
- **Earlier studies on the same instruments, periods, or mechanism.** I read every report in `research/`. Those that bear on this one:
  - `qqq-intraday-trend` (Zarattini, Aziz & Barbon noise-boundary momentum): goes **long** at 10:00 or later when QQQ is above a time-of-day noise band and VWAP. Positive IS and OOS on QQQ (OOS Sharpe 0.82, long side PF 1.39 over the full sample); positive IS but negative OOS on SPY (−0.36) and IGV (−0.38). My primary signal overlaps its 10:00 long entries on QQQ, so I expect the QQQ cross-market leg of this study to be the opposite side of a trade that made money there. That study exits on a band or VWAP re-cross, not at the close, so the overlap is partial.
  - `qqq-atr-scale-in` and `qqq-bollinger-adding`: fade intraday moves away from the open or a band, both sides, with adding. Both rejected; negative at zero cost; the short side lost on its own (PF 0.70 and 0.69); trend days (top quintile of |open→close|) wiped out range days. `igv-small-account-fade` (1-minute fades on IGV) rejected, both sides negative.
  - `qqq-atr-martingale`: the doubling fade passed on QQQ with a drawdown far smaller than predicted and its profit concentrated in a few sessions.
  - `intraday-channel-trend`: 15-minute breakout (momentum) rejected; no gross edge OOS on QQQ, SPY, IGV.
  - `small-cap-gap-up-fade`: shorting small-cap opening gaps and covering at the close passed; it loses on SPY's strongest days.
  - `spy-rsi2-dip-buy`: daily-horizon SPY dip buying passed; much of its OOS profit came from 9 April 2025.
- **What I already know about the test windows.** Every study above used the same IS/OOS split (OOS 2024-07-01 → 2026-09-25), so **the OOS window is not unseen data.** I know QQQ roughly doubled over the full window and rose about 55% OOS; SPY rose about 42% OOS; 2022 was a bear market; April 2025 had the tariff crash and a +10.5% SPY day on 9 April. I know that on QQQ, 10:00-and-later momentum was profitable OOS and that intraday fades of QQQ, SPY, and IGV lost at zero cost. I have not seen the 10:00 → close return on large-pop days in any of these studies, but the prior evidence points against this study's hypothesis, on QQQ in particular.
- **Where the parameters came from:** the measurement window (the first 30 minutes), the hold to the close, and the short-after-up-moves side come from the published opening-reversal literature below. The threshold (1.0 RMS), the 60-session lookback, and the instrument choice are fixed a priori below. None was chosen from a return or outcome.

## Hypothesis

When SPY rises unusually far in its first 30 minutes of regular trading, it falls on average from 10:00 to the close, by more than a round trip costs.

**Mechanism.** Opening order flow is dominated by orders that accumulated overnight: retail and attention-driven buying that executes at the open (Berkman, Koch, Tuttle & Zhang 2012; Barber & Odean 2008), and the unwinding of overnight hedges. Intermediaries absorb the imbalance and are paid through a price that overshoots and then reverts once the imbalance is filled. Fung, Mok & Lam (2000) and Grant, Wolf & Yu (2005) found significant intraday reversals after large price changes at the open in S&P 500 index futures over 1987–2002, and Grant, Wolf & Yu report the reversal is **more pronounced after large positive** opening moves. The counterparty is the late momentum buyer paying the overshoot. Both papers also report that the reversal shrinks sharply after a bid-ask cost proxy, so the edge, if present, is expected to be small.

**Known counter-forces.**
- **Intraday momentum.** Gao, Han, Li & Zhou (2018) find SPY's first half-hour return predicts the last half-hour return with the same sign; Baltussen, Da, Lammers & Martens (2021) tie this to dealer gamma hedging and leveraged-ETF rebalancing, which push late-day prices in the direction of the day's move. `qqq-intraday-trend` found 10:00-onward momentum on QQQ.
- **Drift.** A short-only book fights the equity premium. Most of that premium is earned overnight (Cliff, Cooper & Gulen 2008), which this trade does not hold, but any positive intraday drift is a cost.
- **Information days.** A large opening rise on news (CPI, FOMC, earnings of index heavyweights) may be information, not overreaction, and trend all day.

The report shows how the strategy behaves where each applies: the last half hour, the calendar and regime splits, and the largest-move days.

## Predictions beyond P&L

If the mechanism is right, then on SPY over the full evaluation sample:

1. **Size.** The mean gross trade return for pops with z ≥ 1.5 is greater than for 1.0 ≤ z < 1.5.
2. **Speed.** The mean gross fade-signed return from entry to the 11:59 bar's close (entry price → close of the last bar with minute < 12:00) is positive.
3. **Asymmetry** (Grant, Wolf & Yu 2005). The mean gross trade return of the short on pops is greater than that of the mirror diagnostic, which buys drops of z ≤ −1.0 at the same time and sells at the close.
4. **Timing, not drift.** Shorting pop days beats shorting random sessions over the same hours: the timing placebo p ≤ 0.05 (also acceptance line 6).
5. **Counter-force check.** The mean gross fade-signed return over the last half hour of signal days (close of the last bar with minute < 15:30 → session's last close) is ≥ 0. Gao et al. predict it is negative. A negative value is scored *not consistent* and means intraday momentum is working against the trade.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- Instruments: **SPY** (primary), **QQQ** (cross-market), **IGV** (reported under the same rules; not in the acceptance table because it is a software-sector ETF, not a broad index). Read with `agent-data/mdq.py`, 1-minute bars, 2021-09-27 → 2026-09-25. Split-adjusted by mdq (only IGV has a split). Dividends are not adjusted; they fall on ex-dates at the open and do not affect a 10:00 → close return.
- Bars: stored regular-hours 1-minute bars only. A bar's `ts` is its open; minute-of-day `m` is its New York open time. No resampling.
- Calendar: `mdq.nyse_sessions(2021-09-27, 2026-09-25)`. 2021-12-31 has no bars: it contributes no pop to any lookback and is a zero-return day. 2025-01-09 is not a session. Early closes (`mdq.EARLY_CLOSES`) end with a 13:00 bar; that bar is the session's last bar.
- Missing bars: never filled. The rule uses "the last bar with minute < T" and "the first bar with minute ≥ T", so a missing minute moves the price to the nearest existing bar on the correct side.
- Data checks `backtest.py` must pass before it writes results: every SPY and QQQ session with bars has its first bar at 09:30; the last bar is at 15:59, or 13:00 on an early close; the only calendar session without bars is 2021-12-31.

## Primary rule

Parameters, all fixed:

- `T = 10:00` — the decision time. The first 30 minutes is the "opening period" in the reversal literature above and is the opening range most commonly used by practitioners; the first half-hour is also the predictor window in Gao et al. (2018), so the counter-force is measured on the same window.
- `Z = 1.0` — the pop threshold in trailing RMS units. A priori: one standard deviation is the smallest conventional definition of a "large" move, and it keeps roughly one session in six, so the out-of-sample window can hold more than 60 trades. Not chosen from any outcome; the grid in *Reported checks* shows its neighbours.
- `L = 60` sessions — the RMS lookback: about three months, long enough for a stable estimate of a 30-minute move, short enough to follow volatility regimes. The RMS is zero-centred (no mean is subtracted), so drift does not enter the threshold.
- `COST = 1 bp` of notional per side for SPY and QQQ, 2 bp for IGV (the protocol default and the basis used by earlier IGV studies).

For each session `d` with bars:

1. **Opening price.** `O(d)` = open of the 09:30 bar. If the first bar is not at 09:30, the session has no pop and no trade.
2. **Decision price.** `P(d)` = close of the last bar with minute < 10:00 (normally the 09:59 bar). Known at 10:00:00.
3. **Pop.** `pop(d) = P(d) / O(d) − 1`.
4. **Scale.** `R(d) = sqrt(mean of pop(x)²)` over the `L` most recent sessions before `d` that have a pop. No session of `d` or later is used. Sessions with fewer than `L` prior pops do not trade (warm-up). The first evaluable session is 2021-12-21.
5. **Signal.** Short if `pop(d) ≥ Z × R(d)`. At most one trade per session. No long trades.
6. **Entry fill.** Open of the first bar with minute ≥ 10:00 (normally the 10:00 bar). If there is no such bar, no trade.
7. **Exit.** The only exit: close of the session's last bar (15:59, or 13:00 on an early close), treated as the closing print. No stop, no target, no re-entry. Nothing is held overnight.
8. **Sizing.** Short notional = 100% of equity at entry. Equity compounds daily.
9. **Returns.** With entry `E` and exit `X`: gross trade return `g = 1 − X/E`; net `n = g − c(1 + X/E)` with `c` = cost per side as a fraction. The day's return is `n` on a trade day and 0 otherwise. No borrow fee (intraday short in SPY/QQQ) and no interest on cash.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| `O(d)`, 09:30 bar open | 09:30:00 of `d` | 10:00:00 decision |
| `P(d)`, 09:59 bar close | 10:00:00 of `d` | 10:00:00 decision; fill is the 10:00 bar's **open**, never earlier |
| `R(d)` | close of session `d−1` (prior pops only) | 10:00:00 of `d` |
| Threshold `Z`, `L` | fixed in this file | — |
| Exit price | the session's last bar close | exit only; never feeds a signal |
| S1: prior close | last bar of the previous session with bars | 10:00:00 of `d` |
| SMA50 regime split (reporting only) | prior 50 session closes | classifying `d`; never a filter |

No normalization uses the full sample. The universe is two fixed ETFs that traded throughout.

## Samples

- Warm-up: 60 sessions with bars, 2021-09-27 → 2021-12-20. No trades.
- **In-sample:** 2021-12-21 → 2024-06-28.
- **Out-of-sample:** 2024-07-01 → 2026-09-25.
- The split is the one every earlier study in `research/` used, kept for comparability. It is **exposed**: I have read those studies' OOS results (see *Prior exposure*). There is no unseen stored data; the only clean test of this rule is paper trading after 2026-09-25.

## Benchmarks

Uncosted, on the same sessions:

- SPY buy and hold, close to close on regular-hours last-bar closes (dividends not included).
- **Unconditional short**: short SPY at the 10:00 bar's open and cover at the last close on every evaluable session. This is the drift the pop filter has to beat.
- Diagnostic (not a candidate): the mirror long, buying drops `pop ≤ −Z × R` with the same timing and costs.

## Secondary candidate

**S1 — gap-inclusive pop.** Identical to the primary except `pop(d) = P(d) / C(d−1) − 1`, where `C(d−1)` is the last bar's close of the previous session with bars, and `R(d)` is the RMS of this S1 pop over its own prior 60 values. This reads "pop" as the move into 10:00 from the last close, as some of the opening-reversal literature measures it. First evaluable session 2021-12-22. S1 has its own acceptance table: the seven lines below with S1 substituted for the primary, the S1 grid (same axes, `T` measured from the prior close), and the same cross-market and sample rules. **S1 is never promoted if the primary fails,** and a passing S1 with a failing primary is reported as a secondary result only.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full / IS / OOS at base cost: the protocol defaults (total return, CAGR, annualized volatility, Sharpe, max drawdown, t-stat of mean daily return, trades, win rate, profit factor, average net trade in bp, exposure), plus the benchmarks' Sharpe and max drawdown.
- Breakdowns: calendar year; pop-size bucket (1.0–1.5, 1.5–2.0, ≥ 2.0); gap direction on signal days (09:30 open above or below the prior close); regime (SPY's prior close above or below the mean of its prior 50 closes, reporting only); quintile of SPY's open-to-close move across all evaluable sessions; the entry → 11:59 and 15:29 → close segments of each trade (predictions 2 and 5). Exit reason and side are single-valued (session close, short) and are stated as such.
- Costs: 0, 0.5, 1, 2, 3 bp per side (0, 0.5×, 1×, 2×, 3×). Fill delay: entry at the open of the first bar with minute ≥ 10:01. Upper bound (labelled): entry at `P(d)`, the 09:59 close.
- Direction placebo on gross daily returns: 2,000 draws, each trade's gross return times an independent ±1, seed 20260926. p = (1 + #draws with gross Sharpe ≥ actual) / 2,001.
- Timing placebo: 2,000 draws, seed 20260927. Each draw picks, without replacement, as many sessions as the primary traded, uniformly from all evaluable full-sample sessions that have an entry bar, and shorts them 10:00 open → last close. Compared on gross Sharpe of daily returns over the full evaluation calendar. p = (1 + #draws ≥ actual) / 2,001.
- Block bootstrap: circular, 20-session blocks, 2,000 draws, seed 20260928, 95% interval of the full-sample net Sharpe.
- Plateau grid on IS: `Z ∈ {0.5, 0.75, 1.0, 1.5, 2.0}` × `T ∈ {09:45, 10:00, 10:30}` (pop measured to the close of the last bar before `T`, entry at the first bar at or after `T`), 15 cells, base cost. OOS is shown for selection bias only. Nothing is selected from it.
- Cross-market: identical rules and parameters on QQQ (acceptance) and IGV (reported, 2 bp).
- Verification: a synthetic self-test in `backtest.py` that aborts on failure, and `verify.py`, an independent re-implementation that replays every SPY and QQQ session (fast enough to run all) and must match every trade on side, entry session, entry time and price, exit time and price.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds on SPY at 1 bp per side:

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. IS Sharpe > 0, and at least 60% of the 15 IS grid cells (9 of 15) have Sharpe > 0.
4. Full-sample total return > 0 at 2 bp per side.
5. OOS Sharpe > 0 on QQQ under identical rules.
6. Timing placebo p ≤ 0.05 on the full sample.
7. Minimum sample: at least 60 OOS trades. Below that, the verdict is **Inconclusive**.

Changes from the protocol defaults, with reasons:
- Line 6 is added. A short-only book on a fixed intraday window makes money whenever the market falls from 10:00 to the close, so the question the direction placebo leaves open is whether pop days are better short days than random days. That is the mechanism.
- Line 7 is lowered from 100 to 60 OOS trades. The rule takes at most one trade per session and fires on about 15% of sessions; the pre-lock count is 82 OOS signals on SPY, so 100 is unreachable by construction. 60 is the smallest count at which an OOS Sharpe of 0.5 is not dominated by two or three trades, and leaves a margin for sessions without an entry bar.

A failed line fails the strategy.

## Seeds

Direction placebo 20260926; timing placebo 20260927; bootstrap 20260928. `verify.py` replays every session and draws no sample.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; a different threshold, lookback, or decision time; QQQ or IGV alone; the mirror long or any long side; S1 over the primary; a stop, a profit target, a VWAP or midday exit; a gap, regime, trend, weekday, or news-day filter; a volatility-scaled size; a different cost, fill, or sample split.

## References

- Baltussen, G., Da, Z., Lammers, S., & Martens, M. (2021). Hedging demand and market intraday momentum. *Journal of Financial Economics*, 142(1), 377–403.
- Barber, B. M., & Odean, T. (2008). All that glitters: The effect of attention and news on the buying behavior of individual and institutional investors. *Review of Financial Studies*, 21(2), 785–818.
- Berkman, H., Koch, P. D., Tuttle, L., & Zhang, Y. J. (2012). Paying attention: Overnight returns and the hidden cost of buying at the open. *Journal of Financial and Quantitative Analysis*, 47(4), 715–741.
- Cliff, M., Cooper, M. J., & Gulen, H. (2008). Return differences between trading and non-trading hours: Like night and day. Working paper, SSRN 1004081.
- Fung, A. K.-W., Mok, D. M. Y., & Lam, K. (2000). Intraday price reversals for index futures in the US and Hong Kong. *Journal of Banking & Finance*, 24(7), 1179–1201.
- Gao, L., Han, Y., Li, S. Z., & Zhou, G. (2018). Market intraday momentum. *Journal of Financial Economics*, 129(2), 394–414.
- Grant, J. L., Wolf, A., & Yu, S. (2005). Intraday price reversals in the US stock index futures market: A 15-year study. *Journal of Banking & Finance*, 29(5), 1311–1327.
