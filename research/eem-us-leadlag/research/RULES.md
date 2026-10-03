# EEM US lead-lag: pre-registered rules

Written 2026-10-02, before any EEM, EFA, or lead-lag return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `research/eem-us-leadlag/research/counts.py`, output `counts.json`. Schema `user_version` 6. For EEM, EFA, and SPY, `coverage_summary` on 1d, `coverage` on 1d, `corporate_actions`, and `bars(symbol, "1d")`. `bars()` was used to count sessions, read the first and last session dates, count duplicate dates, and count bars with a nonpositive price or a high/low that does not contain the open and the close. No price, ratio, return, or P&L was printed or written. No signal count was computed, because that would require a SPY close-to-close return.
  - EEM: 3,960 daily bars, 2011-01-04 through 2026-10-02. Coverage rows 4,109 (complete 4,104, missing 4, partial 1). Sessions with bars match the bar-call count. No duplicate dates. `corporate_action` is empty. No 1-minute bars. Nonpositive bars 0. Bad OHLC bars 0.
  - EFA: the same daily counts, the same first and last dates, the same empty corporate-action table, and no 1-minute bars. The EEM and EFA daily date sets are equal.
  - SPY: 3,959 daily bars, 2011-01-04 through 2026-10-01. Coverage rows 4,108 (complete 4,103, missing 4, partial 1). No duplicate dates. `corporate_action` is empty. Nonpositive bars 0. Bad OHLC bars 0. 1-minute bars: 1,254 sessions, 2021-09-27 through 2026-09-25. This study does not read them.
  - The only date in EEM or EFA and not in SPY is 2026-10-02. SPY has no date that EEM lacks.
  - `nyse_sessions` from the first SPY bar date through the last has 3,962 dates. The three dates in that calendar and not in the SPY bars are 2012-10-29, 2012-10-30, and 2018-12-05. Those three are the `needs_attention` rows (status `missing`, 0 bars) on each of the three symbols. 2025-01-09 is also `missing` with 0 bars and is already `SPECIAL_CLOSURES` in mdq, so it is not in `nyse_sessions`. The only `partial` daily row on each symbol is 2021-12-31, which has a bar. `sessions_not_recorded` is empty inside each stored span.
  - Named dates present in all three bar sets: 2021-12-31, 2024-06-28, 2024-07-01, 2026-10-01. Absent from all three: 2012-10-29, 2012-10-30, 2018-12-05, 2025-01-09. 2026-10-02 is present for EEM and EFA and absent for SPY.
- **Earlier studies on the same instruments, periods, or mechanism:** Reports and rules were read. `trades.csv` and `daily.csv` were not. No earlier study in this repo uses EEM or EFA. None is this one-session sign rule. The overlapping facts are SPY, the cash session, and the 2024-07-01 out-of-sample window.
- **What I already know about the test windows:** The out-of-sample window 2024-07-01 through 2026-10-01 is not unseen. `spy-overnight-premium` (Rejected) already measured SPY on these stored daily bars through 2026-10-01: out-of-sample close-to-close about +40.5% (Sharpe about 0.985, max drawdown about −19.9%) and out-of-sample open-to-close about +25.1% (Sharpe about 0.709, max drawdown about −18.2%). Its own rule, long every SPY night and flat through the cash session, returned about +0.27% out of sample (Sharpe about 0.053, profit factor about 1.003, 566 trades) and failed 2 of 6 tests. That study's full-sample close-to-close Sharpe was about 0.75. `spdr-sector-momentum` (Rejected) reports the same out-of-sample SPY buy-and-hold path, about +40.5% (Sharpe about 0.985). `spy-rsi2-dip-buy` (Paper-trading candidate, not for live capital) used regular-hours minute closes, not these daily bars, and reported SPY buy-and-hold about +41.7% (Sharpe about 1.03) from 2024-07-01 through 2026-09-25. It also reported that stored daily bars from 2024-11-20 onward differed from the regular-hours minute print. This study uses the stored daily bars anyway. I do not know EEM's or EFA's open-to-close path, and I do not know whether the sign of the prior SPY close-to-close return lines up with it. One line each on the other studies that already describe this window:
  - `qqq-intraday-trend`: QQQ, flat every night. Out-of-sample through 2026-09-25 about +17.4%, Sharpe about 0.82. The same rules on SPY were negative out of sample (Sharpe about −0.36).
  - `intraday-channel-trend`: 15-minute channel on QQQ, SPY, and IGV, flat overnight. Out-of-sample book about −12.5%, Sharpe about −0.56.
  - `qqq-15m-turtle-overnight`: Turtle on QQQ 15-minute bars, held overnight. Out-of-sample about +0.49%, Sharpe about 0.09. SPY under the same rules had out-of-sample Sharpe about −0.48.
  - `qqq-atr-scale-in`, `qqq-bollinger-adding`, `qqq-atr-band-dip-eod`: QQQ intraday, flat by the close. All three lost money out of sample (Sharpes about −0.57, −0.89, −0.40).
  - `qqq-atr-martingale`: QQQ fade that can be held overnight. Out-of-sample Sharpe about 1.02. SPY under the same rules had out-of-sample Sharpe about 0.33.
  - `index-opening-pop-fade`: short SPY from 10:00 to the close. Out-of-sample about −11.96%, Sharpe about −0.66.
  - `igv-small-account-fade`: flat overnight. Out-of-sample Sharpe about −3.82.
  - `treasury-etf-trend` (Inconclusive), `commodity-etf-momentum` (Inconclusive), `fx-etf-momentum` (Rejected), and `spdr-sector-momentum` (Rejected) use the same out-of-sample calendar through 2026-10-01. They are not EEM rules. Their reports describe 2024–2026 equities as a rising market. `treasury-etf-trend` prints SPY calendar-year figures of about +23.4% in 2024, +16.4% in 2025, and +12.1% in 2026 through 2026-10-01.
  - `qqq-strategy-portfolio` and `qqq-return-stack` combine earlier QQQ series. They are not this rule.
  - `low-liq-high-vol-mean-reversion`, `small-cap-gap-up-fade`, `finviz-gap-up-fade`, and `micro-futures-trend` do not trade EEM. They are not a measurement of this lead.
- **Where the parameters came from:** The hypothesis, the one-session sign rule, the open-to-close hold, the flat overnight book, EEM as the primary, EFA as the cross-market line, the SPY signal, the 1 bp cost, the 2024-07-01 split, the sample end 2026-10-01, the deadzone grid, the decision not to select a deadzone, and the seeds were fixed by the request before any EEM or lead-lag return was computed. The cost is the protocol default for SPY-class liquidity. The request states that EEM and EFA are that class. The deadzone values are round absolute-return steps around the primary value of zero. They were not fitted. The house split date matches the earlier index studies. It was not moved after seeing those studies' SPY path, and it is not moved now.

## Hypothesis

The sign of SPY's prior-session close-to-close return predicts the sign of EEM's next open-to-close return. When SPY rose, EEM's next cash session rises. When SPY fell, EEM's next cash session falls.

**Mechanism.** Rapach, Strauss, and Zhou (2013) find that lagged US stock returns predict monthly returns in other industrialized markets, and that US return shocks are reflected abroad with a lag. Their test is monthly local-market returns, not a one-session sign rule and not an ETF cash session. This study applies only the lead, as a one-session sign rule, to EEM. The other side is a trader who prices EEM from its own prior close and does not fully adjust to the US session until after the next cash open. The position is opened at that next cash open and closed at that same cash close.

**Known counter-forces.** EEM is a US-listed ETF. It trades the same hours as SPY, and the two closes are the same timestamp. The published lead is about later local-market returns. For an ETF, much of the next overseas session can land in the overnight gap. This rule is flat overnight, so that gap is not in the trade. An EEM dividend whose ex-date drop lands in the overnight gap is also outside the trade. No dividend is stored. The out-of-sample SPY path already known from `spy-overnight-premium` was up, in the cash session as well as close to close, so a long-biased implementation can look like beta. The primary is not long-biased: it shorts when the prior SPY session fell. The report scores both sides. None of these facts changes the rule.

## Predictions beyond P&L

If the mechanism is right, then:

1. Full-sample gross P&L is positive on the long trades and positive on the short trades. Gross P&L is the sum of trade-level gross simple returns, separately for `side = long` and `side = short`. It is not a compounded equity and it is not net of cost. The prediction is consistent only if both sums are strictly positive. If either side has no trades, that side is not testable, and the prediction is not consistent.
2. The mean net trade when the prior SPY move is in the top half of `|SPY return|` is higher than the mean net trade when it is in the bottom half. The sample is the primary trades. Let `m` be the median of the absolute prior SPY simple return on those trades. The top half is `|r| >= m`. The bottom half is `|r| < m`. The mean is the arithmetic mean of trade-level net simple returns. The cut is scored after the run. It is not a trading threshold and it is not the deadzone grid. If the bottom half is empty, the prediction is not testable.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*. A pass of the acceptance table with a failed prediction has not confirmed the mechanism.

## Data

- Instruments: EEM (primary), SPY (signal and a benchmark), EFA (cross-market line only). Read with `agent-data/mdq.py`, `bars(symbol, "1d")`, split adjustment left on. There are no stored splits, so adjustment changes nothing. Dividends are not adjusted because none are stored. An EEM dividend that would have landed in the overnight gap is outside the trade either way.
- Bars: stored daily bars only. Do not read the 1-minute tape for the signal, the fills, the benchmarks, the grid, the placebos, or the delayed path. EEM and EFA have no 1-minute bars. SPY's 1-minute history starts in 2021-09 and is a different series. A daily bar's `ts` is 09:30 America/New_York. Its close is not known at 09:30. From late 2024 the stored SPY daily open and close can differ from the regular-hours minute print (`spy-rsi2-dip-buy`). This study uses the stored daily prints for all three symbols. It does not switch to the minute print.
- Calendar: evaluation sessions are the SPY daily-bar dates from the third SPY bar through 2026-10-01, inclusive. The EEM and EFA dates in that span are the same set. Do not insert 2012-10-29, 2012-10-30, 2018-12-05, or 2025-01-09. Do not use 2026-10-02. 2021-12-31 is a session and is included. Early closes are included. They still have a daily open and a daily close.
- Missing bars: leave them missing. No forward fill. A session with no EEM bar is a flat day in `daily.csv` if it is still an evaluation session. It is not a trade.
- Data checks the script must pass before it writes results: EEM and EFA each have 3,960 daily bars from 2011-01-04 through 2026-10-02; SPY has 3,959 from 2011-01-04 through 2026-10-01; the three date sets through 2026-10-01 are identical; no date is duplicated; the four missing dates above are absent; 2021-12-31, 2024-06-28, and 2024-07-01 are present; 2026-10-02 is not an evaluation date; every open, high, low, and close is strictly positive; `high + 1e-6 >= max(open, close)` and `low - 1e-6 <= min(open, close)`; `corporate_action` is empty for all three. The evaluation list has length 3,957, which is the 3,959 SPY dates minus the first two. A failure aborts with no `results.json`, `daily.csv`, or `trades.csv`.

## Primary rule

Parameters, all fixed:

- `PRIMARY = EEM`. `SIGNAL = SPY`. `CROSS = EFA`. EFA does not replace EEM.
- `COST_BPS_SIDE = 1`. Round trip = 2 bp of notional. Source: protocol default for SPY-class liquidity, confirmed by the request.
- `DEADZONE = 0`. The grid does not replace it.
- `FIRST_SPY = 2011-01-04`. `LAST = 2026-10-01`. `OOS_START = 2024-07-01`.
- `NOTIONAL = 1`. One unit. No volatility target.

Let `spy[0] .. spy[N-1]` be SPY's daily bars from 2011-01-04 through 2026-10-01 in date order. `N` is 3,959 unless the data check aborts. Evaluation dates are `spy[2].session .. spy[N-1].session`.

1. **Signal.** On evaluation session `t = spy[i]` with `i >= 2`, the prior SPY session is `s = spy[i-1]` and the one before it is `spy[i-2]`. The closures with no bar are already absent, so this lookup skips them. `r = spy[i-1].close / spy[i-2].close - 1`. `signal = 1` if `r > 0`, `-1` if `r < 0`, and `0` if `r == 0`. The same-day SPY close, `spy[i].close`, is not an input to the signal.
2. **Position.** If `signal` is `1` and EEM has a daily bar on `t`, buy one unit at EEM's open and sell it at EEM's close. Weight `+1`. If `signal` is `-1` and EEM has a bar on `t`, short one unit at the open and cover at the close. Weight `-1`. If `signal` is `0`, or EEM has no bar on `t`, the weight is `0`. No stop, no target, no volatility filter, and no deadzone in the primary. The primary trades every nonzero SPY move that has an EEM bar.
3. **Exits.** The only exit is EEM's daily close on `t`. Exit reason `session_close`. Nothing is held across the close. The next session is a new decision. There is no flip inside the session, because there is only one fill pair.
4. **Fills.** Enter at EEM's daily open on `t` (09:30 America/New_York). Exit at EEM's daily close on `t`. `trades.csv` writes `YYYY-MM-DD HH:MM` with no offset. The exit stamp is 13:00 when `t` is in `mdq.EARLY_CLOSES`, and 16:00 otherwise. `EARLY_CLOSES` lists early closes from 2019 only. A pre-2019 early close is stamped 16:00. The stamp does not change the fill prices or the return. Prices in `trades.csv` are the bar open and close rounded to 6 decimal places.
5. **Sizing.** The daily simple return uses weight `+1`, `-1`, or `0` on a notional of 1 times current equity. Equity starts at 1 before the first evaluation session. After session `t`, `equity *= 1 + strategy_net`. Cash earns no interest. There is no borrow fee beyond the locked commission. Each of the full, in-sample, and out-of-sample windows compounds from 1 on its own first session.
6. **Costs.** `cost_side = COST_BPS_SIDE / 10000`. At the base cost, `cost_side = 0.0001`. Charged on notional, both sides, only when the weight is nonzero, including a zero open-to-close move. `otc = eem.close / eem.open - 1`. `gross = weight * otc`. `strategy_net = gross - 2 * cost_side` when the weight is nonzero, and `0` when it is zero. A flat day has no cost. The open-to-close return is the whole trade. The overnight gap is not in `gross`.

`daily.csv` has one row per evaluation session. Columns, in order: `date`, `strategy_net`, `strategy_gross`, `eem_otc`, `spy_c2c`, `weight`, `signal`, `spy_prior_ret`, `sample`, `equity`, `efa_net`, `efa_gross`. `spy_prior_ret` is `r` as defined above. `spy_c2c` on `t = spy[i]` is `spy[i].close / spy[i-1].close - 1`, uncosted. That is the same-day SPY move. It is a benchmark, not the signal. `eem_otc` is the uncosted long open-to-close, or 0 if EEM has no bar. `sample` is `is` when `date < 2024-07-01` and `oos` otherwise. `equity` is equity after `strategy_net`. `efa_net` and `efa_gross` use the same signal, the same cost, and EFA's open and close.

`trades.csv` is the EEM primary at 1 bp per side and deadzone 0. One row per nonzero weight. Columns, in order: `side`, `entry_time`, `entry_px`, `exit_time`, `exit_px`, `gross`, `net`, `exit_reason`, `spy_prior_ret`. `side` is `long` or `short`. `gross` and `net` are the trade-level simple returns.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| SPY close on `s` and on the SPY session before `s` | 16:00 ET on `s`, or 13:00 on an early close. The daily bar's `ts` is 09:30 and is not the knowledge time. | The signal for the next evaluation session `t > s` only. The fill is `t`'s open. |
| Same-day SPY close on `t` | 16:00 ET on `t` | The close-to-close benchmark on `t`. Not the signal. Not the fill. |
| EEM open on `t` | 09:30 ET on `t` | Entry fill |
| EEM close on `t` | 16:00 ET on `t`, or 13:00 on an early close | Exit fill. Not the signal. |
| EFA open and close | Same clock as EEM | The cross-market book only |
| High, low, volume | Not used | The OHLC validity count only |
| Deadzone grid | Fixed in this file | The IS plateau. Not the primary. |
| Median of `\|r\|`, quintile ranks | After the sample exists | Prediction 2 and the breakdown. Not a trading rule. |
| Any z-score, full-sample volatility target, or same-day SPY return inside the signal | Not used | Not used |

A signal from session `s` does not fill on session `s`. It fills at the next evaluation session's open.

## Samples

- Warm-up: `spy[0]` and `spy[1]`. They form the first signal and are not rows in `daily.csv`. No trade is opened on them.
- **In-sample:** the first evaluation session through the last evaluation session dated before 2024-07-01. 2024-06-28 has a bar. 2024-07-01 has a bar.
- **Out-of-sample:** 2024-07-01 through 2026-10-01.
- The split is the house date used by the earlier index studies. Those studies have already exposed this out-of-sample window, including the SPY path named above. It is not unseen. The first evaluation session falls in 2011, so the warm-up does not remove any out-of-sample session.

## Benchmarks

Uncosted, on the same evaluation sessions. A missing bar contributes 0. Neither benchmark pays the strategy's cost. Neither is dividend-adjusted.

- EEM open-to-close, always long: `eem_otc`.
- SPY close-to-close: `spy_c2c`, including that session's overnight gap. This is not the signal series. The signal is the previous session's close-to-close return.

## Secondary candidates

None. EFA is the cross-market line in acceptance test 5. It is not a second primary and it is not promoted if EEM fails.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, IS, and OOS at base cost, for the primary and for both benchmarks: total return, CAGR on a 252-session year, annualized volatility, Sharpe, max drawdown of compounded equity, t-stat of the mean daily return, trades, win rate, profit factor, average net trade, exposure. Sharpe is the mean daily simple return divided by its sample standard deviation (divisor `n - 1`), times `sqrt(252)`, with a zero risk-free rate. Days with no position count as 0. Profit factor is the sum of winning net trade returns divided by the absolute sum of losing net trade returns. A trade with net return 0 is not a win and does not enter either sum. Win rate is wins divided by trades. CAGR is `equity_end ^ (252 / n_sessions) - 1`. Max drawdown is the minimum of `equity_t / peak_t - 1` with peak starting at 1.
- Breakdowns: calendar year (compounded inside the year from 1); long versus short; exit reason; quintile of the signed prior SPY simple return. Quintiles use the primary trades only, sorted ascending by `spy_prior_ret` with a stable sort. Quintile of rank index `i` (0-based) in a sample of `n` is `min(5, 1 + (i * 5) // n)`. Quintile 1 is the most negative fifth. This is a description, not a filter.
- Costs: 0, 0.5, 1, 2, and 3 bp per side, which is 0×, 0.5×, 1×, 2×, and 3× the base cost. The round trip is twice the per-side cost. Report full-sample Sharpe, full-sample total return, OOS Sharpe, and OOS total return.
- Fill delay: the same signal shifted one evaluation session later. The delayed weight on session `i` is the primary signal on session `i - 1`, set to 0 when that session has no EFA-or-EEM bar for the book being delayed, and the delayed book's first session is flat. Same open-to-close exit, same 1 bp per side. Report full and OOS Sharpe and total return. There is no same-bar-close upper bound. The signal is SPY's prior close, which prints at the same time as EEM's prior close. Selling or buying EEM at that same close would require the SPY close before it is known. That fill is not computed.
- Direction placebo on gross daily returns: 2,000 draws, seed `20261101`, `numpy.random.default_rng`. Keep each trade's session and multiply that trade's gross simple return by an independent `±1` from `Generator.choice([-1.0, 1.0])`. Flat days stay 0. Compare the draw's gross Sharpe with the actual full-sample gross Sharpe. `p = (1 + count of draws with Sharpe >= actual) / 2001`.
- Timing placebo: 500 draws, seed `20261103`. Permute the primary signal across the evaluation sessions with `Generator.permutation`, which preserves the count of `+1`, `-1`, and `0`. Then apply the EEM bar gate and the open-to-close gross return. No cost. Gross Sharpe. Same `p` formula with denominator 501. Not an acceptance line.
- Block bootstrap: circular 20-session blocks of the full-sample `strategy_net`, 2,000 draws, seed `20261102`. `n_blocks = ceil(n / 20)`. Starts are `Generator.integers(0, n, size=(2000, n_blocks))`. The sample is those blocks concatenated and truncated to `n`. The 95% interval is the 2.5th and 97.5th percentiles of the 2,000 Sharpes, NumPy linear quantile. Also report the median and the fraction of draws with Sharpe less than or equal to 0.
- Plateau grid on IS only: deadzone in `{0, 0.0025, 0.005, 0.01, 0.015}`. If the absolute prior SPY return is strictly below the deadzone, the weight is 0. Otherwise the weight is the signal, subject to the EEM bar. The primary cell is 0. A cell's Sharpe uses every IS session, including the new flat days. OOS Sharpe for each cell is reported for selection bias only. Nothing is selected from the grid.
- Cross-market: EFA, identical signal, identical cost, identical deadzone 0, identical samples. OOS Sharpe is acceptance line 5.
- Verification: the self-test below, before the store is opened, and `verify.py` on every EEM trade plus a seeded sample of 40 evaluation sessions (seed `20261104`). `verify.py` does not import `backtest.py`.

### Self-test

Before the store is opened, `backtest.py` runs these synthetic sessions and aborts if any assertion fails. Dates `D0` through `D5` are 2011-01-04, 01-05, 01-06, 01-07, 01-10, 01-11. `D6` is 2024-07-03, which is in `EARLY_CLOSES`. `D7` is 2024-07-05 and stands in for a bar past `LAST`.

SPY closes: `D0 100`, `D1 110`, `D2 90`, `D3 90`, `D4 80`, `D5 88`, `D6 88`. EEM bars: `D2` open 50 close 55, `D3` open 40 close 44, `D4` open 44 close 30, `D5` absent, `D6` open 70 close 77, `D7` open 10 close 20. `LAST` is `D6`.

- `D2` is long. The signal is `110/100 - 1 > 0`. Same-day SPY close is 90 and is not used. Gross `0.10`. Net `0.10 - 0.0002`. Exit stamp 16:00.
- `D3` is short. The signal is `90/110 - 1 < 0`. EEM rises from 40 to 44, so the short's gross is `-0.10` and its net is `-0.1002`.
- `D4` has a zero SPY move (`90/90 - 1`) and is flat, including no cost, even though EEM falls from 44 to 30.
- `D5` has a negative signal and no EEM bar. It is a flat row and not a trade.
- `D6` is long, gross `0.10`, net `0.0998`, exit stamp 13:00. `D7` is not a row.
- With deadzone `0.15`, the only trade is the `D3` short. `|0.10|` is below `0.15` and `90/110 - 1` is not.

The delayed book on this tape is flat on `D2`, short on `D3` is not the assertion target beyond a check that `D3`'s delayed weight equals `D2`'s signal (`+1`) and that `D2`'s delayed weight is 0.

## Acceptance

The status is mechanical.

- If line 6 fails, the status is **Inconclusive**, even if another line also fails. The other lines are still reported.
- If line 6 passes and any of lines 1 through 5 fails, the status is **Rejected**.
- If lines 1 through 6 all pass, the status is **Paper-trading candidate**. That is not a recommendation of live capital.

The primary is a paper-trading candidate only if every line holds at 1 bp per side:

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full-sample gross Sharpe.
3. IS Sharpe > 0, and at least 60% of the IS grid cells have Sharpe > 0. The grid has 5 cells, so this is at least 3 of 5.
4. Full-sample total return > 0 at 2 bp per side (2× the base cost).
5. OOS Sharpe > 0 on EFA under the identical rule.
6. At least 100 OOS trades. This rule trades almost every session. The floor is not lowered.

No line is changed from the protocol defaults. A failed line fails the strategy. A grid cell, one side, or EFA does not replace a failed primary.

## Not done in this study

The report will not promote any of these in place of the primary: a grid deadzone; the long side or the short side alone; switching the primary to EFA; holding EEM overnight; replacing the SPY signal with EEM's own prior return; a 12-month momentum overlay; a stop or a volatility filter; a different cost; the one-session-delayed fill; a sample that includes 2026-10-02; a sample split other than 2024-07-01; the 1-minute tape; a dividend adjustment the store does not contain.

The user asked for the study to be run and forbade a git commit. `RULES.md` is locked with `research/eem-us-leadlag/research/_lock.py` and is not committed. `research/README.md` is not edited.

Seeds: direction `20261101`, bootstrap `20261102`, timing `20261103`, verify `20261104`.
