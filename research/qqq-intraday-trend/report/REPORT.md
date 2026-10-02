# QQQ intraday trend following: noise-boundary momentum

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Proposal for paper trading.** Not for live capital. |
| Instrument | QQQ, regular hours, 1-minute bars, flat every night |
| Data | 2021-09-27 to 2026-09-25, read via `agent-data/mdq.py` |
| Rules | [`research/qqq-intraday-trend/research/RULES.md`](../research/RULES.md), written before any run |
| Code | [`research/qqq-intraday-trend/research/`](../research/) |

## 1. Summary

**Proposal.** Trade QQQ intraday in the direction of the day's move, but only once price has left a "noise area" around the open. The noise area is sized from how far QQQ usually strays from its open at each time of day. Decisions are made every half hour from 10:00 to 15:30 ET. A position exits when price falls back through the band or VWAP, and every position is flat at the close. The rule is published (Zarattini, Aziz & Barbon, 2024). It was used here with its published defaults, not fitted to this data.

**Result, after 1 bp per side, at 1× notional (P1):**

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2021-10-18 → 2026-09-25 | → 2024-06-28 | 2024-07-01 → |
| Sessions | 1,239 | 677 | 562 |
| Total return | **+58.8%** | +35.2% | +17.4% |
| CAGR | 9.9% | 11.9% | 7.5% |
| Annual volatility | 9.0% | 8.7% | 9.3% |
| Sharpe | **1.09** | 1.34 | **0.82** |
| Max drawdown | −9.8% | −4.9% | −9.8% |
| Trades / profit factor | 1,069 / 1.26 | 611 / 1.29 | 458 / 1.22 |
| Avg net trade | 4.5 bp | 5.1 bp | 3.7 bp |
| *QQQ buy & hold Sharpe (max DD)* | *0.73 (−35.6%)* | *0.53 (−35.6%)* | *1.01 (−22.9%)* |

It passed all six acceptance tests written before the first run (§8). Daily returns have a **0.05 correlation** with QQQ, and on QQQ's 20 worst days it averaged **+118 bp** and made money on 75% of them.

**Why it is only a paper-trading proposal.** The same evidence carries four warnings:

1. **Since May 2025 the edge has been flat:** Sharpe −0.15 over 353 sessions, while QQQ rose 57%. Take out April 2025 (the tariff crash and rebound) and the out-of-sample Sharpe falls from 0.82 to 0.45.
2. **The profit comes from a few big days.** Removing the 20 best sessions (1.6% of days) takes the Sharpe to about zero. That is how trend following behaves, but it means long flat stretches. The longest in this sample was 356 sessions.
3. **The identical rules lost money out-of-sample on SPY (−0.36) and IGV (−0.38).** Both were positive in-sample, so the QQQ result may not generalize.
4. **It needs low costs and fast fills.** Sharpe falls to zero at about 3 bp per side, and one extra minute of fill delay cuts out-of-sample Sharpe from 0.82 to 0.55.

**Recommendation.** Paper-trade P1 at 1× for at least six months, with the execution spec and stop rules in §10. Treat it as a diversifier that pays off in crashes, alongside a long QQQ book, not as a standalone return engine. The vol-targeted variant (P2) is **not** recommended yet: it raised return but not out-of-sample Sharpe, and it raised drawdown to −16%.

## 2. The strategy

### Rules (P1)

For each session `d`, at minute `k` after 09:30:

```
move(d,k)  = |close(d,k) / open(d) − 1|
σ(d,k)     = mean of move(·,k) over the previous 14 sessions
UB(d,k)    = max(open(d), prevclose(d)) × (1 + σ(d,k))
LB(d,k)    = min(open(d), prevclose(d)) × (1 − σ(d,k))
VWAP(d,k)  = Σ typical·volume / Σ volume, typical = (H+L+C)/3, from 09:30 through k

At 10:00, 10:30, … 15:30 (using the close of the minute just ended):
    target = LONG   if close > max(UB, VWAP)
           = SHORT  if close < min(LB, VWAP)
           = FLAT   otherwise
    if target ≠ position: trade at the next minute's open
At the close: exit everything at the session's last print.
```

- **Why max(open, prior close):** a gap counts as part of the day's noise. A breakout has to clear both the overnight level and the open.
- **Why VWAP:** it acts as a trailing stop. A long that falls back below VWAP is exited even if it is still above the band.
- **Only 12 decision points a day:** checking every minute trades noise, as the terminal's own 1-minute runs show (§7).
- **P2** is identical except for sizing: leverage = min(2, 2% ÷ σ14), where σ14 is the trailing 14-day standard deviation of daily returns.

### How it trades

| | |
|---|---|
| Sessions with a trade | 733 of 1,239 (59%) |
| Trades per year | ≈ 215 |
| Time in market | 26.5% of regular-hours minutes |
| Holding time | median 60 min, mean 119 min. 36% of trades are held to the close |
| Long / short | 535 / 534 trades. Both sides profitable (PF 1.39 / 1.15) |
| Win rate | 41%. Winners are larger than losers |

### Why it might work

Two published mechanisms push QQQ in the direction of a large intraday move, late in the session:

- **Hedging flows.** Option dealers who are short gamma, and leveraged and inverse ETFs rebalancing into the close, must buy after rallies and sell after declines. The flows grow with the size of the day's move, and they concentrate in the afternoon (Baltussen, Da, Lammers & Martens, *JFE* 2021).
- **Slow digestion of news** on days with genuine information (Gao, Han, Li & Zhou, *JFE* 2018).

On ordinary days, these flows are small and prices mean-revert. The noise band is what separates the two: it keeps the strategy out until the move is bigger than usual for that time of day. §6 shows this is where the profit comes from: large-move days make money, small-move days pay for it.

## 3. Method

- **Data.** QQQ 1-minute bars came from `data/market-data.sqlite` via `agent-data/mdq.py`. They are regular hours only and split-adjusted (QQQ has no splits in the store). mdq's coverage check found 1,254 sessions: complete except the ten 13:00 early closes, and 2021-12-31, which was never fetched (a known terminal calendar bug). As a result, 2022-01-03 has no prior close and is not traded. The first 15 sessions are warm-up.
- **Pre-registration.** [`RULES.md`](../research/RULES.md) fixed the following before the first run:
  - the rules and parameters;
  - the in-sample/out-of-sample split (2024-06-28 / 2024-07-01);
  - costs, sizing, and every check reported below;
  - six acceptance criteria.

  Analyses added after seeing results are marked **post hoc**.
- **Fills.** A signal uses the close of the minute that just ended. The fill is at the **next minute's open**, not at that close. The final exit uses the last 1-minute close as a stand-in for the closing auction.
- **Costs.** 1 bp of notional per side, scaled by leverage. QQQ's one-cent quoted spread is about 0.15 bp at recent prices, so this is roughly 7× the spread. No borrow, and no interest on idle cash.
- **Returns.** Daily simple returns; flat days count as zero. Sharpe uses a zero risk-free rate and √252.
- **Verification.** A separate, deliberately simple re-implementation, built on plain dictionaries of mdq bars, replayed 40 randomly chosen sessions. It produced the **same 43 trades, with the same sides, entries, and exits**, as the vectorized engine.

## 4. Results

![Growth of $1](figures/equity.svg)

![Drawdown](figures/drawdown.svg)

| Strategy (1 bp/side) | Full: return / Sharpe / max DD | In-sample Sharpe | Out-of-sample: return / Sharpe / max DD |
|---|---|---:|---|
| **P1 noise-boundary, 1×** | +58.8% / **1.09** / −9.8% | 1.34 | +17.4% / **0.82** / −9.8% |
| P2 same, vol-targeted ≤2× | +94.0% / 1.17 / −16.0% | 1.53 | +23.8% / 0.79 / −16.0% |
| S intraday momentum (Gao et al.) | −29.4% / −1.45 / −33.3% | −1.98 | −6.8% / −0.70 / −12.5% |
| *QQQ buy & hold* | *+101.8% / 0.73 / −35.6%* | *0.53* | *+55.4% / 1.01 / −22.9%* |
| *QQQ long open→close daily* | *+32.6% / 0.40 / −23.7%* | *0.54* | *+4.6% / 0.20 / −20.4%* |

![Calendar-year return](figures/by_year.svg)

| Year | P1 | Sharpe | Max DD | QQQ buy & hold |
|---|---:|---:|---:|---:|
| 2021 (from Oct 18) | +1.9% | 1.42 | −2.4% | +8.5% |
| 2022 | **+14.1%** | 1.16 | −4.9% | **−33.5%** |
| 2023 | +12.7% | 1.96 | −2.9% | +53.8% |
| 2024 | +10.0% | 1.48 | −3.3% | +24.8% |
| 2025 | +6.3% | 0.60 | −7.0% | +20.2% |
| 2026 (to Sep 25) | +3.6% | 0.74 | −3.3% | +21.2% |

- P1 made money in every calendar year, including 2022, when QQQ fell a third.
- Its Sharpe has fallen each year since 2023, as the rolling chart in §9 shows.
- It trails buy-and-hold in every up year.

## 5. Is it real?

### Placebo

The test keeps every P1 trade's entry and exit time, randomizes its direction, and charges the same costs. This measures whether the band and VWAP choose the *direction* well, or whether a profit could come from timing alone. Of 2,000 random-direction books, the best reached a Sharpe near 0.8. The actual 1.09 beat **all 2,000 (p < 0.0005)**. The random books averaged −0.45, which is what the costs alone do.

![Placebo](figures/placebo.svg)

### Bootstrap

A block bootstrap of daily returns (20-day blocks, 2,000 draws) gives a 95% interval for the full-sample Sharpe of **0.43 to 1.70**. None of the draws was ≤ 0. The out-of-sample t-stat alone is only **1.22**. Two years of data cannot prove the edge by itself; the case rests on the full sample and the placebo.

### Parameter plateau

The grid below covers lookback L × band multiplier V at 30-minute decisions. The full grid also included 15- and 60-minute spacing, 75 cells in all. In-sample, every one of the 75 cells had a positive Sharpe, and the published default ranked 17th. Nothing was selected from the grid.

![Parameter grid](figures/grid.svg)

The out-of-sample panel checks what selecting from the grid would have done:

- **The in-sample winner** (60-minute decisions, L 7, V 1) fell from 1.75 to **0.25** out-of-sample. Picking it would have been worse than keeping the default.
- **60% of cells stayed positive out-of-sample.** The rank correlation between in-sample and out-of-sample Sharpe was 0.44.
- **Spacing:** longer decision spacing held up better. Out-of-sample mean Sharpe was −0.08 at 15 minutes, 0.11 at 30 minutes, and 0.24 at 60 minutes.
- **Band width:** V ≤ 0.75 (a narrower band, so more trades) lost money out-of-sample at every lookback.
- **Caveat:** the default's 0.82 is near the top of its out-of-sample neighbourhood. Expect live results closer to the neighbourhood average than to the headline.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 bp | 0.5 bp | **1 bp** | 2 bp | 3 bp |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | 1.58 | 1.34 | **1.09** | 0.61 | 0.12 |
| Out-of-sample Sharpe | 1.26 | 1.04 | **0.82** | 0.38 | −0.06 |
| Full-sample return | +96.6% | +76.7% | **+58.8%** | +28.2% | +3.5% |

- **Costs.** Each basis point per side costs about 0.5 of Sharpe. Break-even is about 3 bp. P2 is more sensitive because its leverage multiplies costs: at 2 bp, its out-of-sample Sharpe is 0.27.
- **Latency.** Filling one minute later (at 10:01 instead of 10:00) gives Sharpe 0.88 full-sample and **0.55** out-of-sample. Part of the edge is the first minutes after a band break, so execution has to be automated.

### Other markets (the same rules, unchanged)

| | In-sample Sharpe | Out-of-sample Sharpe | Full: return / PF |
|---|---:|---:|---|
| **QQQ** | 1.34 | **0.82** | +58.8% / 1.26 |
| SPY | 0.89 | −0.36 | +10.9% / 1.07 |
| IGV | 0.41 | −0.38 | +0.2% / 1.01 |

This is the weakest part of the evidence. The paper that proposed the rule was built on SPY, and here SPY's edge disappeared after mid-2024. QQQ is more volatile and has a larger leveraged-ETF complex (TQQQ/SQQQ), which fits the hedging-flow explanation. But the test cannot tell that apart from QQQ simply being the lucky one of three.

## 6. Where the profit comes from

![By move quintile](figures/move_quintiles.svg)

Sessions are bucketed by the size of QQQ's open-to-close move:

| Quintile of \|open→close\| | 0–25 bp | 25–50 bp | 50–84 bp | 84–135 bp | ≥135 bp |
|---|---:|---:|---:|---:|---:|
| P1 average day | −14.1 bp | −10.7 bp | −11.2 bp | +1.1 bp | **+54.2 bp** |
| Winning share of traded days | 8% | 13% | 32% | 57% | 78% |

The strategy loses a little on quiet days, when it is stopped out of false breaks, and makes it back several times over on the fifth of days that trend.

It profits from large moves in either direction. That is why returns barely correlate with QQQ (0.05), and why it helped most on QQQ's worst days:

| | |
|---|---|
| QQQ's 20 worst days (avg −436 bp close-to-close) | P1 avg **+118 bp**, positive on 15 of 20 |
| Best P1 days | 2025-04-09 +877 bp (QQQ +12.0%), 2025-10-10 +312 bp (QQQ −3.5%), 2022-08-26 +279 bp (QQQ −4.1%), 2024-12-18 +261 bp (QQQ −3.6%), 2026-06-05 +256 bp (QQQ −4.8%) |
| Worst P1 days | 2022-12-01 −218 bp, 2022-06-17 −210 bp, 2022-04-27 −200 bp, 2024-08-06 −161 bp |

![By entry time](figures/by_entry.svg)

- **By entry time:** 10:00 and 10:30 entries are 42% of trades and 42% of profit. Entries at 11:00 and 12:00 lost money, and 11:30 roughly broke even. The 12:30 bucket (+18.5 bp on 71 trades) is probably noise. Nothing here was acted on, because excluding times after seeing this chart would be fitting.
- **By volatility regime (post hoc):** splitting sessions into terciles of trailing 14-day volatility, Sharpe was 1.41 (low, ≤1.0% daily), 0.67 (mid), and 1.29 (high). It is not only a high-volatility strategy.

## 7. Alternatives considered

| Candidate | Result on QQQ | Verdict |
|---|---|---|
| **Noise-boundary momentum (P1)** | Sharpe 1.09 full / 0.82 out-of-sample | Proposed |
| Market intraday momentum (S): first 30 minutes' return sets the direction of the last 30 minutes (Gao et al. 2018) | Sharpe −1.45 full / −0.70 out-of-sample, t = −3.2 | Rejected: this signal ran *backwards* in 2021–2026 |
| Opening-range breakout with a 200-day filter (`research/opening-range-breakout`, earlier study) | +5.7%, Sharpe 0.20, PF 1.03 (2022-07 → 2026-09) | Too thin |
| The terminal's stored QQQ backtests (MA cross 10/30, N-bar momentum, 1m/5m, long-only) | PF 0.97–1.04 at **zero** cost. The 1m momentum run made 13,710 round trips | Rejected: checking every minute makes cost-dominated trades |

The comparison shows that frequency is what matters. Rules that check every minute trade too often to survive costs. A rule that trades only after an unusually large move, a few times a day, keeps an average net trade of +4.5 bp.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| Out-of-sample Sharpe | ≥ 0.50 | 0.82 | ✅ |
| Out-of-sample profit factor | ≥ 1.10 | 1.22 | ✅ |
| Placebo p-value (full) | ≤ 0.05 | < 0.0005 | ✅ |
| In-sample Sharpe | > 0 | 1.34 | ✅ |
| In-sample grid cells with Sharpe > 0 | ≥ 60% | 100% | ✅ |
| Full-sample return at 2 bp/side | > 0 | +28.2% | ✅ |

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| **Edge decay** | Trailing-year Sharpe hit **−1.09** in the 12 months to April 2026 (post hoc), and has been positive in 92.5% of 252-day windows. From May 2025 to April 2026: −5.8%, while QQQ rose 40%. The last 6 months: Sharpe 1.60, +6.0% | Paper-trade first. Use the stop rules in §10 |
| **Few big days carry the result** | Without its best 5 / 10 / 20 days, full-sample Sharpe is 0.75 / 0.46 / −0.06 (post hoc). April 2025 alone moved out-of-sample Sharpe from 0.45 to 0.82 | Size so a year of bleed is tolerable. Judge it over years, not months |
| **Does not generalize** | SPY and IGV lost money out-of-sample under the same rules | Don't add markets without separate evidence |
| **Execution** | Break-even at about 3 bp/side. One minute of delay costs about 0.3 of out-of-sample Sharpe | Automated orders at the mark. Measure slippage from the first day |
| **Short sample** | 5 years of 1-minute history, one regime family. No 2008 or 2020 | Fetch older QQQ 1-minute history before committing capital |
| **Closing price** | The model exits at the 15:59 bar's close. The official close is the 16:00 auction, and the store's daily bar disagrees with that minute by more than $1 on some sessions (see the ORB study) | Use market-on-close orders, and compare fills with the model |
| **Leverage (P2)** | Adds return, not out-of-sample Sharpe. Drawdown −16% vs −9.8%. 32% of days sit at the 2× cap | Stay at 1× until paper results justify more |

## 10. Deployment proposal (paper trading)

- **Account:** paper, notional = 1× equity (P1). No leverage.
- **Signals:** computed at 10:00:00, 10:30:00, … 15:30:00 ET, from the 1-minute bar that has just closed. σ comes from the prior 14 sessions, taken from the store at the open. Prior close = the prior session's last regular-hours print.
- **Orders:**
  - Entries, exits, and flips at the marks: marketable limit orders, with the limit 1 tick through the touch, sent within 5 seconds of the mark.
  - End of day: a market-on-close order for any open position, sent right after the 15:30 decision (Nasdaq's MOC cutoff is 15:50).
- **Early closes:** the last decision is 12:30, and the exit is at the 13:00 close.
- **Log every fill** into a manual ledger in the terminal (a `trade_fill` with `external_id` = the broker order id). The STATS panel and `mdq.py book` will then report it exactly like the backtest.
- **Review at 6 months (≈125 sessions).**

  **Stop** if any of these holds:
  1. average slippage > 1 bp per side above the model;
  2. drawdown > 15%, which is 1.5× the backtest maximum;
  3. live fills differ from a replay of the same days through `backtest.py` by more than 2 bp per trade on average. That would be a model or execution error, not bad luck.

  **Continue** if none of those holds. A negative 6-month return is **not** by itself a stop: the backtest had a year-long bleed.

## 11. Next steps

1. **Port P1 to the terminal's backtest engine** (`apps/terminal/src/backtest/strategies/`, next to `Momentum.cpp`). It can then be run from the docked window, recorded as a backtest ledger, and marked on charts. Record it with `flatten_at_session_end` on. Its fills should match `trades_p1.csv`.
2. **Extend the history.** Ingest QQQ 1-minute data before 2021, if the vendor has it, to test 2018–2021, including the March 2020 crash, as a true hold-out.
3. **Fix the 2021-12-31 calendar bug** (NyseCalendar.cpp) and re-ingest that session.
4. After 6 months of paper results, decide on live capital and on P2-style leverage.

## 12. Reproduce

From the repo root:

```bash
python research/qqq-intraday-trend/research/backtest.py
```

```bash
python research/qqq-intraday-trend/research/posthoc.py
```

```bash
python research/qqq-intraday-trend/research/charts.py
```

`backtest.py` writes `results.json`, `daily.csv`, and `trades_p1.csv` in about 5 s. `posthoc.py` writes `posthoc.json`. `charts.py` writes `research/qqq-intraday-trend/report/figures/*.svg`. The random draws are seeded (20260926), so reruns on the same store give identical numbers.

### References

- Zarattini, C., Aziz, A., & Barbon, A. (2024). *Beat the Market: An Effective Intraday Momentum Strategy for S&P500 ETF (SPY).* SSRN working paper.
- Gao, L., Han, Y., Li, S. Z., & Zhou, G. (2018). Market intraday momentum. *Journal of Financial Economics*, 129(2).
- Baltussen, G., Da, Z., Lammers, S., & Martens, M. (2021). Hedging demand and market intraday momentum. *Journal of Financial Economics*, 142(1).
