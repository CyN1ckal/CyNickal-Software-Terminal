# IGV small-account minute fade: pre-registered rules

Written 2026-09-26, before any return, P&L, forward return, or hit rate was computed from the store. Results may not edit this file. Anything computed after the first store run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file.** `research/igv-small-account-fade/counts.py` (dollar volume, prices, and signal counts only). No outcome was measured.
  - Intraday history long enough to test is QQQ, SPY, and IGV, each 1,254 sessions from 2021-09-27 through 2026-09-25. ADBE and GOOGL have 19 sessions of 1-minute bars (2026-08-31 through 2026-09-25). AAPL, AMZN, META, and AAL have daily bars only. `$SPX` has no bars. There is no NBBO, no odd-lot flag, and no auction imbalance.
  - Split-adjusted 1-minute dollar volume (close × volume):
    - QQQ: p1 $4.85M, p50 $27.6M, p99 $197M. Session p50 $13.7B. Bars under $1M: 0.0012%.
    - SPY: p1 $8.71M, p50 $45.8M, p99 $341M. Session p50 $24.3B. Bars under $1M: 0.0006%.
    - IGV: p1 $28.7k, p5 $52.3k, p10 $84.2k, p25 $180k, p50 $427k, p75 $1.07M, p90 $2.55M, p99 $11.0M. Session p50 $273M. Share of 1-minute bars under $250k: 33.7%. Under $1M: 73.5%.
  - Split-adjusted IGV closes: p5 $52.75, p50 $83.50, p95 $110.49.
  - Primary signal counts, using the definitions in Primary rule and not any forward price: IGV 3,942 raw signals (3,258 on sessions through 2024-06-28, 684 from 2024-07-01), of which 2,648 are at least 15 minutes apart (2,136 and 512). Capacity-window minutes: IGV 166,106, QQQ 6, SPY 3. QQQ and SPY raw signals: 0 and 0.
- **Earlier studies.** Same instruments and the same out-of-sample dates.
  - `qqq-intraday-trend`: noise-boundary momentum on QQQ. Paper-trading candidate. Out-of-sample Sharpe 0.82, total return +17.4% after 1 bp per side. The same rules lost money out of sample on SPY (−0.36) and IGV (−0.38). Profit concentrated in a few days; removing April 2025 cut the out-of-sample Sharpe from 0.82 to 0.45.
  - `qqq-intraday-reversion`: fade 4σ five-minute shocks. Rejected. Out-of-sample Sharpe −0.76 on QQQ. Identical rules on IGV: full-sample Sharpe −0.30, out-of-sample −0.46. The event study found the average path after those shocks kept going rather than reverting. That study did not condition on absolute dollar volume.
  - `intraday-channel-trend`: 15-minute Donchian breakout with a chandelier, equal-weight QQQ/SPY/IGV. Rejected. Out-of-sample book return −12.5%, Sharpe −0.56. Each name, including IGV, lost money out of sample.
- **What is already known about the test window.** The out-of-sample window 2024-07-01 through 2026-09-25 is not unseen. It contains the April 2025 tariff crash and rebound. On that window, QQQ open-to-close trend following made money, and both shock-fading and channel breakouts lost money on QQQ, SPY, and IGV.
- **Where the parameters came from.** The account size, the participation limits, the 2× relative-volume multiple, the 10 bp floor, the half retrace, and the 15-minute hold are round values fixed below from the mechanism. They were not chosen by looking at returns. The signal count above was used only to see that the out-of-sample sample can clear 100 trades. It did, so no threshold was widened.

## Hypothesis

A one-minute move in IGV that is large for that clock minute, and that prints on at least twice the minute's usual dollar volume while that dollar volume is still between $250,000 and $1,000,000, is temporary price pressure from flow a $250,000 order cannot take the other side of; a fixed $5,000 fade, filled at the next print, is paid for supplying that immediacy.

**Mechanism.** A small order moves the price when the book is thin (Kyle 1985). When the same clock minute trades at least twice its recent dollar volume, the flow is unusually aggressive relative to that minute's normal book, which is the price-pressure signature in Campbell, Grossman, and Wang (1993). Short-horizon reversal associated with that pressure shows up where liquidity is limited and turnover is elevated (Avramov, Chordia, and Goyal 2006). The premium is payment for immediacy when the book cannot absorb a large clip (Grossman and Miller 1988). Here the capacity constraint is the minute itself. A $5,000 order is at most 2% of a minute with at least $250,000 of dollar volume. A $250,000 order is more than 25% of a minute with less than $1,000,000. The trade is small enough for a $25,000 account and too large a share of the minute for that institutional clip. Nagel (2012) is the related point that reversal pays liquidity providers when intermediary capacity is scarce. This study does not condition on VIX or any other intermediary-stress series.

**Known counter-forces.** The move can be information, and adverse selection is worse when the book is thin. The same ETFs' extreme five-minute shocks did not revert at the next minute in `qqq-intraday-reversion`, including IGV, so the reversion may already be over inside the signal minute. Intraday momentum, documented on QQQ in `qqq-intraday-trend`, pays the other side on large trend days. A displayed IGV spread near one cent can eat a small gross edge. The report scores the pre-registered predictions and shows results by side, by the day's move, and by the dollar-volume half of the window.

## Predictions beyond P&L

If the mechanism is right, then:

1. Among primary trades, the average gross return is higher when the signal minute's dollar volume is under $500,000 than when it is from $500,000 up to $1,000,000.
2. Across every primary signal, not only the trades taken, the average fade-signed log return from the next bar's open to five minutes after the signal is positive, and it is greater than the same average measured to thirty minutes after the signal.
3. The same rule with the $1,000,000 cap removed, and no other change, has a lower average gross return per trade than the primary. This comparison is not a candidate.
4. QQQ and SPY, under the identical rule, each produce fewer than 30 raw signals on the full sample. The pre-lock count already returned 0 and 0. The backtest confirms the count. This is a screen check, not a profitability claim.

Each one is scored *consistent*, *not consistent*, or *not testable*. A passing P&L with failed predictions has not confirmed the mechanism.

## Data

- Instruments: IGV is the book. QQQ and SPY are the liquid controls, run under identical rules. Read with `agent-data/mdq.py`, split-adjusted. IGV's only split is 5-for-1 on 2024-03-07; dollar volume is invariant to that adjustment because price is divided by the ratio and volume is multiplied by it. Dividends are not adjusted. The strategy is flat overnight, so an ex-date open gap is not held. ADBE and GOOGL are excluded because 19 sessions cannot support the minimum sample. Names with daily bars only are excluded because the hypothesis is intraday.
- Bars: stored 1-minute regular-hours bars. A bar's `ts` is its open. Minute `k` is the bar that opens at 09:30 plus `k` minutes. `k = 0` is 09:30 and `k = 389` is 15:59. No coarser bar is used for the primary.
- Calendar: evaluation sessions are NYSE sessions from `mdq.nyse_sessions`, which treats 2021-12-31 as a session. That date has no IGV bars (the terminal calendar had marked it a holiday and it was never fetched). It contributes a zero strategy return if it falls inside the evaluation window. 2025-01-09 is a special closure and is not a session. Early closes are `mdq.EARLY_CLOSES` (13:00 ET, expected last minute `k = 210`).
- Missing bars: not forward-filled. A signal minute needs the previous clock minute present in that session. A missing later minute is skipped; the fill uses the next existing minute, as specified under Fills.
- Data checks the script must pass before it writes results: IGV, QQQ, and SPY each have 1-minute bars; no bar opens outside 09:30–15:59 ET; IGV has a split with ratio 5; 2021-12-31 is not among the sessions that contain bars.

## Primary rule

Parameters, all fixed:

- `ACCOUNT = 25000` dollars. A round small account.
- `NOTIONAL = 5000` dollars per trade, fixed, not resized as equity changes. One fifth of the account, and at most 2% of a signal minute that passes the minimum dollar volume.
- `D_MIN = 250000`, `D_MAX = 1000000` dollars. Participation bounds for the $5,000 ticket and a $250,000 clip.
- `RV_MIN = 2`. Unusually heavy flow versus that clock minute's own recent dollar volume (Campbell, Grossman, and Wang).
- `MAG_FLOOR = 0.001` (10 bp) and `MAG_MULT = 2`. The minute must be large versus its own recent typical move, and at least 10 bp so a one-cent bounce on an ~$83 price (about 1.2 bp) is not a signal.
- `LOOKBACK = 20` prior occurrences of that clock minute.
- `HOLD = 15` minutes from the signal minute.
- `K_MIN = 15` (09:45). The open is overnight information, not this minute's pressure.
- `K_MAX = 360` (15:30) on a normal session, and `K_MAX_EARLY = 150` (12:00) on an early close, so a 15-minute hold fits before the flatten.
- `FLATTEN_K = 385` (15:55) and `FLATTEN_K_EARLY = 205` (12:55).
- `COST_BPS = 2` per side of `NOTIONAL`. See Costs.
- `RETRACE = 0.5`, `STOP_MULT = 1`. Half of a liquidity-pressure move is the target; a further extension of the whole move is information and stops the fade.

1. **State, known at the close of minute `k`.** Let session `d` have bars keyed by `k`. If minute `k−1` is missing, minute `k` has no return and cannot signal. Otherwise `r(d,k) = ln(close(d,k) / close(d,k−1))` and `D(d,k) = close(d,k) × volume(d,k)`. Let `H` be the previous `LOOKBACK` sessions that themselves had a return at the same `k` (both `k` and `k−1` present). `med_abs` is the median of `|r|` over `H`. `med_D` is the median of `D` over `H`. With 20 observations the median is the average of the 10th and 11th sorted values. If fewer than 20 prior occurrences exist, minute `k` cannot signal. The current session is not in `H`.
2. **Signal.** Evaluated at the close of `k`. Let `k_max` be `K_MAX_EARLY` on `mdq.EARLY_CLOSES` and `K_MAX` otherwise. A signal requires all of: `K_MIN ≤ k ≤ k_max`; `D_MIN ≤ D(d,k) < D_MAX`; `med_D > 0` and `D(d,k) / med_D ≥ RV_MIN`; `|r(d,k)| ≥ max(MAG_FLOOR, MAG_MULT × med_abs)`. Fade it. `r > 0` is a short. `r < 0` is a long. Record `p_prev = close(d,k−1)`, `p_sig = close(d,k)`, and `move = p_sig − p_prev`.
3. **Position.** At most one open trade. A signal on a bar that also produces an exit decision is ignored. A signal while a trade is open is ignored. After an exit whose reason is `stop`, no new entry is allowed for the rest of that session. After a `target`, `time`, or `flatten` exit, later signals that session are allowed. There is no flip and no overnight hold.
4. **Exits,** decided on a later bar's close, in this order. The entry fills on a later minute `n`, so the first exit decision is the close of `n` or a later existing minute. Let `m` be the decision minute and `C` its close. Side `s` is `+1` for a long and `−1` for a short. `target_px = p_sig − RETRACE × move`. `stop_px = p_sig + STOP_MULT × move`.
   1. **Stop**, if `s = +1` and `C ≤ stop_px`, or `s = −1` and `C ≥ stop_px`.
   2. **Target**, if `s = +1` and `C ≥ target_px`, or `s = −1` and `C ≤ target_px`.
   3. **Flatten**, if `m` is the last existing minute of the session, or `m ≥ FLATTEN_K_EARLY` on an early close, or `m ≥ FLATTEN_K` on a normal session.
   4. **Time**, if `m ≥ k + HOLD`.
   A close cannot be both through the stop and through the target, because those prices are on opposite sides of `p_sig`. High and low are not used. These are close-confirmed exits, not intrabar stop orders.
5. **Fills.** A decision at the close of minute `m` fills at the open of the first existing minute strictly after `m`. An entry whose next minute does not exist is cancelled. An exit whose next minute does not exist fills at the close of `m`. The position is open from that entry open onward, so the entry minute's own close can be the first exit decision. A gap after `m` does not create a bar; the fill is the next print's open.
6. **Sizing.** Each trade buys or shorts `NOTIONAL / entry_price` shares. Shares may be fractional. Equity starts at `ACCOUNT` and compounds dollar P&L, but the next trade is still `NOTIONAL` dollars. Daily simple return is that session's dollar P&L divided by `ACCOUNT`. A session with no exiting trade returns 0. The return series includes every NYSE session from the first evaluation session through the last, including sessions with no IGV bars.
7. **Costs.** `COST_BPS / 10000 × NOTIONAL` dollars on the entry and the same amount on the exit, so a round trip costs 4 bp of the $5,000 ticket, which is $2. The basis is the displayed IGV spread, not a fitted number. A Vanguard quote page showed a $0.01 bid-ask spread (0.01%), and a US News quote on 2026-09-18 showed $104.45 / $104.46. Half of one cent on the stored median price $83.50 is about 0.6 bp. Two bp per side is about three times that half-spread, to cover a sub-$1M minute and a small adverse tick. It is wider than the 1 bp used for QQQ and SPY in the earlier studies. No borrow fee and no interest on cash. The cost sweep scales this 2 bp figure. Costs do not scale with a leverage that this rule does not use.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| `close(d,k)`, `volume(d,k)`, hence `r` and `D` | Close of minute `k` on session `d` | Signal decision at that close |
| `close(d,k−1)` | Close of the previous clock minute, which must already have printed | Signal at `k` |
| `med_abs`, `med_D` | Close of the prior 20 sessions that had minute `k` | Signal at `k` on `d`. Session `d` is appended only after its decision |
| Early-close flag | The published calendar, known before the session | Entry cutoff and flatten time |
| Fill price | Open of a minute strictly after the decision, or the decision close when no later minute exists | That fill |
| Session open-to-close quintile | The session's last print. Not known at entry | Breakdown only. Not an entry filter |
| Daily close of a daily bar | 16:00 ET. Not used | Not used |

Normalization uses prior occurrences of that clock minute only. It does not use the full sample, and it does not use a future session. The universe is the three ETFs already stored. It is not a point-in-time liquid universe, and the rules do not claim it is.

## Samples

- Warm-up: the first 20 IGV sessions that contain at least one 1-minute bar. Those sessions are not in the return series. A clock minute can signal only once it has 20 prior occurrences, which is never during those 20 sessions when the minute exists in each of them, and is later when it does not.
- **In-sample:** the first evaluation session through 2024-06-28.
- **Out-of-sample:** 2024-07-01 through the last stored session, 2026-09-25.
- The split matches the earlier studies so the windows can be compared. It is not a fresh split. The out-of-sample window has already been used, as recorded under Prior exposure.

## Benchmarks

On the same evaluation sessions, uncosted:

- IGV close to close: last 1-minute close of the session divided by the previous session's last 1-minute close, minus one. If either close is missing, the benchmark return is 0.
- IGV open to close: last 1-minute close divided by the first 1-minute open of that session, minus one. If the session has no bars, the return is 0.

Neither benchmark is an acceptance hurdle. The fade is not a claim about beating a long IGV position.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at 2 bp per side: the protocol defaults. Sharpe is the mean daily simple return divided by its sample standard deviation, times √252, with a zero risk-free rate. CAGR uses a 252-session year. Days with no position are 0. Profit factor is the sum of winning trades' net dollars divided by the absolute sum of losing trades' net dollars. Average net trade is in bp of the $5,000 notional.
- Breakdowns: calendar year; long versus short; exit reason; entry clock bucket 09:45–11:00, 11:00–13:00, 13:00–15:00, 15:00–15:30; quintile of the session's open-to-close return; quintile of the open-to-signal return `(p_sig / session open − 1)`.
- Costs: 0, 1, 2, 4, and 6 bp per side, which are 0, 0.5×, 1×, 2×, and 3× the 2 bp base. Fill delay: one extra existing bar, as defined in the next paragraph. Upper bound: entry at the signal close and exit at the decision close, labelled as an upper bound, not as a result.
- The one-bar delay: the fill is the open of the second existing minute strictly after the decision. If that minute does not exist, an entry is cancelled and an exit fills at the last existing minute's close when one later minute exists and at the decision close when none does. When exactly one later minute exists, the exit fills at that minute's open.
- Direction placebo on gross dollar P&L: each trade's gross dollars are multiplied by an independent ±1, costs are not in this comparison, and the daily gross-return series is rebuilt on the same sessions. 2,000 draws, NumPy seed `20260926`. Compare the actual full-sample gross Sharpe to those draws. `p = (1 + count of draws whose Sharpe ≥ actual) / 2001`.
- Timing placebo: in each session, draw the same number of candidate minutes as the primary's entries that session, from minutes that pass only the clock and the dollar-volume window and have a previous clock minute and a later minute. Assign a random side. Apply the same one-position, stop-lockout, and exit rules. 500 draws, seed `20260927`. Report the mean gross Sharpe and the same p-value formula with denominator 501. This is not an acceptance line.
- Block bootstrap: circular blocks of 20 sessions, 2,000 draws, seed `20260928`, 2.5th and 97.5th percentiles of the full-sample net Sharpe.
- Plateau grid on the in-sample window: `RV_MIN ∈ {1.5, 2, 3}`, `MAG_MULT ∈ {1.5, 2, 3}`, `D_MAX ∈ {500000, 1000000, 2000000}`, `HOLD ∈ {5, 15, 30}`. 81 cells. Out-of-sample Sharpe for each cell is reported for selection bias only. Nothing is selected from the grid.
- Cross-market: the identical primary parameters on QQQ and on SPY.
- The three mechanism comparisons in Predictions 1–3.
- Verification: a self-test on synthetic sessions covering a long target, a short stop with no re-entry, a time exit, a flatten, an early close, a missing previous minute, a dollar-volume rejection, a relative-volume rejection, a magnitude rejection, a gap fill, a last-bar close fill, a signal ignored while in a position, and a new entry after a target. `verify.py` recomputes every session with no shared signal code. Seed for any sampled path: `20260929`. The check covers every session.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 2 bp per side:

1. Out-of-sample Sharpe ≥ 0.5 and out-of-sample profit factor ≥ 1.10.
2. Direction placebo `p ≤ 0.05` on the full-sample gross Sharpe.
3. In-sample Sharpe > 0, and at least 60% of the 81 in-sample grid cells have Sharpe > 0. A cell with undefined Sharpe counts as not greater than 0.
4. Full-sample total return > 0 at 4 bp per side (2× the base).
5. QQQ and SPY each have fewer than 30 raw signals on the full sample. This replaces the protocol's cross-market Sharpe line. The mechanism says those tapes are outside the dollar-volume window. Requiring a positive Sharpe there would require the screen to fail. The pre-lock count is 0 and 0. A control that somehow produced 30 or more signals would mean the screen did not bind, and the study would be rejected.
6. At least 100 out-of-sample trades. Below that, the verdict is **Inconclusive**.

Line 5 is the only change from the protocol defaults. The reason is the capacity screen, stated above.

A failed line fails the strategy.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or the short side alone; the thinner or the thicker half of the dollar-volume window; the uncapped rule; QQQ or SPY as the book; a time-of-day or regime filter; a 5-minute bar; ADBE or GOOGL; a different cost, fill, account size, or sample split; an intrabar stop; a borrow fee; whole-share rounding; an overnight hold. A cash account that cannot short is a deployment limit. The long side alone is not a substitute if the primary fails.
