# QQQ holdings relative strength: pre-registered rules

Written 2026-10-03, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the store before this file:** `research/qqq-holdings-relative-strength/research/counts.py` counted NYSE sessions and daily bars for QQQ and the 101 names from 2021-10-04 through 2026-10-02. It did not print a price, a return, a rank, or a P&L. The calendar has 1,255 NYSE sessions in that span: 251 warm-up sessions (2021-10-04 through 2022-09-30) and 1,004 evaluated sessions (2022-10-03 through 2026-10-02). Of the evaluated sessions, 437 are on or before 2024-06-28 and 567 are on or after 2024-07-01, with no session between those dates. 2022-10-03, 2024-06-28, 2024-07-01, and 2026-10-02 are sessions. 2021-12-31 is a session. 2025-01-09 is not a session in `mdq.nyse_sessions`. QQQ has 1,254 daily bars, first 2021-10-04 and last 2026-10-01; the only missing session in the calendar is 2026-10-02. ALNY has 1,254 bars and is missing only 2023-09-13. SPCX has 1,151 bars (104 calendar sessions missing). FER has 635 bars, first 2023-08-01. First bars of the later listings match the ingest note: CEG 2022-01-19 (1,181 bars), WBD 2022-04-04 (1,129), GEHC 2022-12-15 (952), ARM 2023-09-14 (766), ALAB 2024-03-20 (637), NBIS 2024-10-21 (489), SNDK 2025-02-13 (411), CRWV 2025-03-28 (381), HONA 2026-06-15 (77). The other names have a bar on every calendar session in the span. No symbol failed to resolve.
- **Earlier studies on the same instruments, periods, or mechanism:** `spdr-sector-momentum` is 12–1 month cross-sectional momentum on the SPDR sector ETFs, long the top 3 and short the bottom 3. It was rejected. Its report summary, which was read, gives an out-of-sample return of +6.2% and Sharpe 0.253 from 2024-07-01 through 2026-10-01, profit factor 0.935. `commodity-etf-momentum` and `fx-etf-momentum` are the same momentum family on other ETFs. The research index lists the commodity study as inconclusive and the currency study as rejected. Neither was a paper-trading candidate. This constituent ranking has not been run.
- **What I already know about the test windows:** The out-of-sample window 2024-07-01 through 2026-10-02 is not unseen for the index. It overlaps the shared out-of-sample window 2024-07-01 through 2026-10-01 used by the sector, commodity, and currency momentum studies, and QQQ's path in that window is already described by earlier QQQ reports. Those reports were not re-read for this file. The names in this universe were not the traded set of those studies.
- **Where the parameters came from:** The 126-session formation, the 21-session skip, and the top quintile are the published 6–1 month defaults specified for this study, used unchanged and not estimated here. Jegadeesh and Titman (1993) rank on an intermediate past return and skip the most recent month because that month reverses. The 21-session rebalance, the long-only book, the 5 bp cost, the alphabetical half split, and the sample dates are fixed by this study's design before any return.

## Hypothesis

The stocks inside QQQ that have led the index over the prior six months, skipping the most recent month, will have a higher average return over the next month than the stocks that have lagged it.

**Mechanism.** Jegadeesh and Titman (1993) find that buying intermediate-horizon winners and selling losers earns a return that their tests do not attribute to systematic risk, and that the most recent month's return reverses. The other side is an investor who is slow to absorb firm-specific news, or who sells a winner early and holds a loser. They keep paying if that delay persists and if a one-month book is too short for them to close it. This study trades only the long side of that ranking, inside one index, against the index's own past return.

**Known counter-forces.** The last month reversing is why the skip exists; a formation that includes it can wash the ranking out. Winners crash together (the momentum-crash pattern). These names are the index, so their excess returns are smaller than a market-wide sort and are highly correlated with QQQ. The membership list is the 2026-10-02 constituent list, which a trader in 2022 could not have known. Prices omit dividends, so the formation understates total-return momentum. Five basis points per side is enough to eat a thin excess.

## Predictions beyond P&L

If the mechanism is right, then:

1. The held top quintile has a higher average forward open-to-next-rebalance return than the bottom quintile of the same ranking. The bottom quintile is not held. The forward return of a name is its fill-price ratio from the rebalance open to the next rebalance open, minus one. A name missing either of those prices is left out of that rebalance's average. Each quintile's rebalance return is the equal-weight mean of the names that remain. The score uses rebalances where both quintiles have at least one priced name. Consistent if the unweighted mean of the top-quintile rebalance returns is strictly greater than the same mean for the bottom quintile. This does not promote a short of the bottom quintile.
2. Among the six in-sample grid cells, the three skip=21 cells are not uniformly worse than the three skip=0 cells. Uniformly worse means that for every lookback in {63, 126, 252}, the in-sample Sharpe at skip 21 is strictly below the in-sample Sharpe at skip 0. Consistent if that is not true. Not testable if any of the six in-sample Sharpes is null. This matches the published short-term reversal inside the formation window, and it is not a license to replace the primary with another cell.
3. The book's daily returns move with QQQ, so the out-of-sample Sharpe does not come from being flat in a QQQ drawdown alone. Consistent only if both parts hold. The correlation part holds if the Pearson correlation of out-of-sample daily strategy net returns and QQQ close-to-close returns is strictly positive. The flatness part holds if mean `held` on out-of-sample sessions where QQQ's compounded equity is strictly below its running peak is at least half of mean `held` on the other out-of-sample sessions. Equity and the peak start at 1 at the out-of-sample boundary; the peak is updated after each session's benchmark return. Not testable if the correlation is undefined or if either exposure group is empty.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- Instruments: the 101 equity names listed below, plus QQQ as the formation benchmark and the close-to-close benchmark. Read with `agent-data/mdq.py`, split-adjusted. Dividends are not adjusted, so price momentum understates total-return momentum. The list is the equity holdings of Invesco QQQ, CUSIP 46090E103, business date 2026-10-02. It is end-of-sample membership. Index membership is look-ahead. Published weights are not used. No name is dropped after seeing who won.
- Names, in the order supplied: NVDA AAPL MSFT MU AMD AMZN META GOOGL TSLA SPCX GOOG INTC AVGO WMT CSCO LRCX PLTR AMAT COST PANW NFLX CRWD KLAC TXN SNDK MRVL LIN AMGN ADI QCOM STX SHOP GILD ASML TMUS PEP WDC ISRG ARM FTNT VRTX BKNG SBUX ADP LITE CDNS ADBE SNPS MAR DDOG CEG CSX MELI MNST APP WBD DASH CTAS INTU CMCSA MDLZ REGN ROST MPWR TER ORLY ABNB HON AEP NXPI ALAB MSTR FAST NBIS PCAR BKR FANG PDD HONA PYPL XEL ADSK RKLB MCHP CCEP EXC KDP CRWV IDXX FER TTWO ODFL TRI WDAY PAYX ROP AXON DXCM ALNY GEHC CPRT.
- Bars: daily. A daily bar's `ts` is 09:30 America/New_York. Its close is not known until 16:00 ET, or 13:00 ET on an early close. No coarser bar is built.
- Calendar: `mdq.nyse_sessions` from 2021-10-04 through 2026-10-02. Evaluated sessions are those from 2022-10-03 through 2026-10-02. The warm-up sessions are lookback endpoints only. 2025-01-09 is absent from this calendar. 2021-12-31 is present and may be an endpoint.
- Missing bars: leave missing. Do not forward-fill. A non-positive price is treated as missing. QQQ has no bar on 2026-10-02, so the benchmark return on that session is 0 and a skip=0 formation cannot use that close.
- Data checks the script must pass before it writes results: the calendar has 1,255 sessions and 1,004 evaluated sessions; all 101 names and QQQ resolve; the self-test passes.

## Primary rule

Parameters, all fixed: FORMATION = 126 sessions (published 6-month default), SKIP = 21 sessions (published 1-month skip), REBALANCE = 21 evaluated sessions (one-month hold), MIN_NAMES = 5, QUINTILE = N // 5, COST = 0.0005 per unit of absolute weight change (5 bp per side; these are stocks, not the QQQ ETF), SEED = 20261003.

The session index `t` is the index on the full NYSE calendar above, including warm-up. Evaluated sessions are a contiguous suffix of that calendar.

1. **Formation.** At a signal session `t`, the stock return is `close[t-21] / close[t-21-126] - 1`, using that name's own closes on those two session dates. The QQQ return is QQQ's closes on those same two dates. Excess is the stock return minus the QQQ return. The last 21 closes, `close[t-20]` through `close[t]`, are not in the ratio. A name is eligible only if it has a bar on both endpoints and on the signal session. If either endpoint is missing, or the signal session is missing, the name is ineligible. If either QQQ endpoint is missing, no name is eligible.
2. **Signal.** Walk evaluated sessions in order. The first signal is the first evaluated session on which QQQ has both formation endpoints and at least 5 names are eligible. Later signals are every 21st evaluated session after that one. The signal uses only information known at that session's close. There is no mid-month signal.
3. **Position.** Let N be the eligible count. If N < 5, the target book is empty. Otherwise `k = max(1, N // 5)` and the target is the first k names in the ranking, each with weight `1/k`. Rank by excess descending. Ties break by symbol ascending, so the alphabet wins the higher rank. No price breaks a tie. Weights of a fully filled target sum to `k * (1/k)`. Cash earns 0. The book is long only.
4. **Exits.** The only exit is the next rebalance fill, or the sample-end mark defined below. There is no stop, target, or time stop inside the hold.
5. **Fills.** The scheduled fill is the next evaluated session's open after the signal. A signal on the last evaluated session has no fill and does not change weights. Execution at a fill open:
   - A name whose target weight is above its current weight is bought up to the target only if it has a positive open on that session. If it does not, it is not bought. The missed increase is not retried later in the hold. Its unfilled weight stays in cash at return 0. The other filled names keep weight `1/k` and are not grossed up.
   - A name whose target weight is below its current weight is sold down to the target if it has a positive open. If it does not, it stays at the old weight, keeps its slot, and the exit or reduction waits for the next session open that has a positive price. That retry runs on later sessions, including sessions that are not rebalance dates. The next signal's target replaces a pending target. A delayed exit can leave gross exposure above 1 until it fills. No borrow fee is charged. That fee was not specified.
   - A name that is in the target at the same weight, and that has an open, is closed and reopened at that same open so the holding period ends. The trade boundary does not require turnover.
   - A name that loses its bar while held contributes 0 on any open-to-open leg that lacks a positive price on either end. It stays in the slot until the exit rule above removes it. Do not forward-fill the missing open.
6. **Sizing.** Each held name's weight is the weight set at the fill that opened the lot. The account compounds. Daily net return multiplies equity. Flat days are 0. A trade's `session` is its entry session, which is what places it in the in-sample or out-of-sample trade count.
7. **Costs.** On a session where weights change, the cost is `0.0005 * sum(|new weight - old weight|)`, subtracted from that session's return. It is not changed after the run. Cash interest is 0. A sample-end mark pays no exit cost.

**Daily return.** Output rows are the evaluated sessions only. On the first evaluated session the strategy return is 0 and `held` is 0, unless a same-bar close fill is being run as the upper bound. On a later evaluated session `i`, the gross return is the sum over names of `w_prev * (P[i] / P[i-1] - 1)`, where `w_prev` is the weight in force at the previous session's open after that open's fill, and `P` is the split-adjusted open. A leg with either open missing or non-positive contributes 0. The cost above, if this open changes weights, is then subtracted. `held` on that row is the sum of `w_prev`, the weight that earned the row. The benchmark on session `i` is QQQ's split-adjusted `close[i] / close[i-1] - 1` when both closes exist and are positive, otherwise 0. The benchmark is uncosted. The first evaluated session's benchmark is 0.

**Trades.** One lot per name. Gross is the sum of that lot's daily account contributions, not the compounded path and not `weight * (exit/entry - 1)` when a missing bar zeroed a leg. Net is gross minus the entry cost and the exit cost. The entry cost is `0.0005 * max(new weight - old weight, 0)` and the exit cost is `0.0005 * max(old weight - new weight, 0)`, using the base rate. Those two pieces sum to the book's cost. Entry and exit times are the daily bar's New York open, `isoformat` with `timespec="seconds"`. Entry and exit prices are those opens. `side` is `long`. `exit_reason` is `rebalance` when the lot ends at a fill, and `sample_end` when it is still open after the last evaluated session is processed. The sample-end price is that session's open when it is positive, otherwise the most recent earlier positive open. The mark adds no further return and no exit cost. Trades are ordered by entry session, then symbol.

**Same-bar close fill, upper bound only.** The fill session is the signal session and `P` is the close. The signal close is used as a fill, which a trader does not get. It is reported and is not the primary.

**One-session delay.** The fill session is two evaluated sessions after the signal instead of one. Exits move with it. The same cost rule applies at the delayed fill.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| Membership list | End of sample, 2026-10-02 file | Every signal. Not knowable on the early signal dates. Disclosed, not repaired. |
| Formation closes | Close of the endpoint sessions, both on or before `t-21` | Signal at the close of `t` |
| Signal-session close | 16:00 ET on `t` (13:00 ET on an early close) | Eligibility at `t`, and the same-bar close upper bound |
| QQQ formation closes | Close of the same two endpoint dates | Signal at `t` |
| Fill open | 09:30 ET on the next session | The fill, after the signal close |
| Open-to-open return booked on session `i` | Open of session `i` | The daily row for session `i`. It does not use session `i`'s close |
| QQQ close-to-close benchmark | Close of session `i` | The daily row for session `i` |
| Ranking, quintile count, half split | The half split ignores returns. The rank uses only the formation | The fill after the signal |

No input is normalized on the full sample. The universe is not rebuilt from winners.

## Samples

- Warm-up: NYSE sessions from 2021-10-04 through 2022-09-30. Formation endpoints only.
- **In-sample:** evaluated sessions from 2022-10-03 through 2024-06-28.
- **Out-of-sample:** evaluated sessions from 2024-07-01 through 2026-10-02.
- The split gives the most recent portion of this store to the out-of-sample window. That window is not unseen for QQQ, as recorded under prior exposure. In-sample is `session <= 2024-06-28`. Out-of-sample is `session >= 2024-07-01`.

## Benchmarks

- Uncosted QQQ close-to-close on the same evaluated sessions. A missing QQQ close contributes 0. Dividends are not in the benchmark.

## Secondary candidates

None.

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full, in-sample, and out-of-sample at base cost: the protocol defaults, compound return model.
- Breakdowns: calendar year; side; exit reason; quintile of the day's QQQ close-to-close move.
- Costs: 0, 0.5, 1, 2, and 3 times 5 bp per unit absolute weight change. Fill delay: one session later than the base open. Upper bound: the same-bar close fill.
- Direction placebo on gross trade pieces: 2,000 draws, seed 20261003. Each draw multiplies every piece of a trade by one independent sign. p = (1 + count of draws with Sharpe >= actual) / (2000 + 1).
- Timing placebo: on each primary signal date, draw `k` names uniformly from the eligible names, without using the rank, and hold them under the same fill, missing-bar, and exit rules. Gross Sharpe, 500 draws, seed 20261003, its own generator. Same p formula. This is a random book of the same size, not a second market and not a clock-time shift. The strategy has no intraday entry clock to move.
- Block bootstrap: 20-session blocks, 2,000 draws, seed 20261003, of the primary net daily path. The 95% interval is not an acceptance line.
- Plateau grid on the in-sample window: lookback in {63, 126, 252} and skip in {0, 21}. Six cells. The primary is lookback 126 and skip 21. Rebalance stays 21 and the quintile stays `N // 5`. For skip 0 the formation is `close[t] / close[t-lookback] - 1`. Each cell finds its own first signal and then steps 21 evaluated sessions. Out-of-sample Sharpes are shown for selection bias only. Nothing is selected from the grid.
- Cross-market: sort the 101 symbols alphabetically. HALF1 is the first 51: AAPL ABNB ADBE ADI ADP ADSK AEP ALAB ALNY AMAT AMD AMGN AMZN APP ARM ASML AVGO AXON BKNG BKR CCEP CDNS CEG CMCSA COST CPRT CRWD CRWV CSCO CSX CTAS DASH DDOG DXCM EXC FANG FAST FER FTNT GEHC GILD GOOG GOOGL HON HONA IDXX INTC INTU ISRG KDP KLAC. HALF2 is the last 50: LIN LITE LRCX MAR MCHP MDLZ MELI META MNST MPWR MRVL MSFT MSTR MU NBIS NFLX NVDA NXPI ODFL ORLY PANW PAYX PCAR PDD PEP PLTR PYPL QCOM REGN RKLB ROP ROST SBUX SHOP SNDK SNPS SPCX STX TER TMUS TRI TSLA TTWO TXN VRTX WBD WDAY WDC WMT XEL. Run the identical rank rule inside each half, still versus full-sample QQQ, still `k = max(1, N // 5)` with N at least 5, where N is the eligible count inside that half. Report both as cross-market symbols HALF1 and HALF2. The split does not use returns. It is not a second market. It is the only identical-rules check this store can support, and acceptance line 5 is still scored on it.
- Verification: a store-free self-test of ranking, tie break, N < 5 flat, quintile count, the skip window not reading the last 21 closes, and a missing endpoint making a name ineligible; and `verify.py` matching trades on side and entry and exit time and price. The verifier draws at least 40 entry sessions with seed 20261003 and also checks the full trade list. It does not import the study signal.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at 5 bp per unit of absolute weight change:

1. Out-of-sample Sharpe >= 0.50 and out-of-sample profit factor >= 1.10.
2. Direction placebo p <= 0.05 on the full sample.
3. In-sample Sharpe > 0, and at least 60% of the in-sample grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 2× base cost.
5. Out-of-sample Sharpe > 0 on at least one of HALF1 and HALF2 under identical rules.
6. Minimum sample: at least 100 out-of-sample trades. Below that, the verdict is **Inconclusive**, even if another line fails.

These are the protocol defaults. No threshold was changed.

A failed line fails the strategy.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the skip=0 formation; the bottom-quintile short; one half; one symbol; a dividend-adjusted rerun; a membership list that is not the 2026-10-02 list; a different cost, fill, delay, or sample split; a volatility target; published-weight sizing.
