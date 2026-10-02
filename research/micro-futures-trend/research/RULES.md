# Diversified trend following on micro futures, $100k account: pre-registered rules

Written 2026-09-26, before any return, P&L, or outcome statistic was computed from the data. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

## Prior exposure

- **Looks at the data before this file.** The store (`data/market-data.sqlite`) holds no futures, and the terminal's `ingest` cannot store them: it confirms tickers with OpenFIGI and files bars on NYSE sessions. The study therefore keeps its own copy of the vendor data in `research/micro-futures-trend/research/data/raw/`, downloaded with `fetch.py` from the MBoum v1 endpoint `/v1/markets/stock/history?interval=1d`, with the user's permission. Nothing was written to `data/`. Before writing this file, I ran the following, none of which computes a return, P&L, or signed price change:
  - Probes of the vendor: the v3 historical endpoint has no futures; v1 serves continuous front-month series (`ES=F` etc., `instrumentType=FUTURE`) for 2021-09-27 → 2026-09-25 only (1,260 bars; `range=max` and weekly bars give the same span; monthly bars go back to 2016). Expired single contracts (`ESH25.CME`) are not served.
  - `dq.py`: bar counts, duplicate/weekend dates, missing or non-positive OHLC, zero-volume bars, unchanged-close counts, and volume-jump counts by year and month.
  - Decimal precision of the stored closes: the vendor rounds every price to 2 decimals. `6E=F` has 26 distinct closes in 1,260 bars and `6J=F` has 2. FX futures are unusable.
  - `rollcheck.py` and a listing of daily volumes in eight roll months: roll days from volume jumps. Clean in 2021–2024; in 2025–2026 the vendor's continuous series for gold and crude no longer shows the volume jump, so roll days cannot be identified reliably from volume.
  - The fraction of bars whose open is within 5 bp of the previous close, by Monday vs Tuesday–Friday (a non-directional check of what the `open` field is). It was 39% (ES) to 85% (ZT) on Tuesday–Friday and 11% for CL. This is consistent with `close` = settlement and `open` = the 18:00 ET Globex reopen, with the gap being post-settlement trading, weekends, and rolls.
- **Earlier studies on the same instruments, periods, or mechanism.** None on futures. `qqq-intraday-trend` (paper-trading candidate), `intraday-channel-trend` (rejected), and `qqq-intraday-reversion` (failed) studied *intraday* trend and reversion in QQQ, SPY, and IGV over 2021-09 → 2026-09. They overlap this study's calendar window and the equity-index market (ES/NQ track SPY/QQQ), but not its holding period (months, not minutes) or its mechanism.
- **What I already know about the test windows.** This window is not unseen data to me. From general market knowledge I know that managed-futures trend indices had a very strong 2022 (mostly inside this study's warm-up), lost money in 2023, were roughly flat to modestly positive in 2024, and had a sharp drawdown in the first half of 2025 around the April 2025 tariff shock and its reversal. I also know the broad paths: US equity indices rose strongly in 2023–2024 and recovered after April 2025; gold rallied strongly through 2024–2025; Treasury yields rose into late 2023 and then ranged; crude drifted lower over 2023–2025. I know less about 2026. The OOS window (2024-10 → 2026-09) is therefore partly known to me. Every parameter below is a published default or fixed by reasoning written here, not tuned; the prior knowledge is disclosed so the reader can discount accordingly.
- **Where the parameters came from.** Published defaults: Hurst, Ooi & Pedersen (2017), *A Century of Evidence on Trend-Following Investing* (equal-weighted 1-, 3-, and 12-month time-series momentum signals, volatility-scaled positions, monthly rebalancing); Moskowitz, Ooi & Pedersen (2012), *Time Series Momentum* (EWMA volatility with a 60-day centre of mass); Carver (2015), *Systematic Trading* (equal risk per asset class, the instrument diversification multiplier, whole-contract positions). The 20% portfolio volatility target, the duration conversions for rates, and the cost model are fixed below with their reasons.

## Hypothesis

A time-series trend rule (1-, 3-, and 12-month past returns, volatility-scaled, rebalanced monthly), traded with whole CME **micro** contracts in an account starting at $100,000 across 11 futures markets in three asset classes, earns a positive risk-adjusted return after commissions and slippage.

**Mechanism.** Prices underreact to news at first because of anchoring, slow-moving capital, and central-bank and hedger flows that lean against moves. Later they overreact as herding and feedback trading extend the move (Hurst, Ooi & Pedersen 2017; Moskowitz, Ooi & Pedersen 2012). The counterparties are hedgers and institutions with mandates that pay to transfer risk, and discretionary traders who fade moves. Trend following has been profitable across a century of data in those papers, with its best returns in extended moves and crises.

**Known counter-forces.** Choppy, range-bound markets and sharp V-shaped reversals (whipsaw); crowding of CTA flows; published-anomaly decay (McLean & Pontiff 2016). Specific to this study:
- **Whole contracts.** At $100,000 the per-market risk budget is about one micro contract, so rounding can zero out or double a position.
- **A 4-year evaluation window.** The standard error of an annual Sharpe ratio over two years is about 0.7, so this study has low power. A true Sharpe of 0.5 fails the OOS line about half the time, and a true Sharpe of 0 passes it about 24% of the time. The verdict is applied as written regardless.

## Predictions beyond P&L

If the mechanism is right, then:

1. **Positive trade-level skew.** In the primary book, the average winning trade is larger than the average losing trade in absolute dollars (payoff ratio > 1).
2. **Convexity to equity moves.** Grouping the primary's calendar-month returns by quintile of the ES benchmark's calendar-month return, the mean return of the top and bottom quintiles combined exceeds the mean of the middle three.
3. **Breadth.** The secondary (fractional) book's full-sample gross P&L is positive in at least 2 of the 3 asset classes.

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- **Source.** MBoum v1 daily bars for continuous front-month futures, saved as the vendor's JSON in `data/raw/<SYM>.json` by `fetch.py` on 2026-09-26. Each bar has `date` (the trading day), `open`, `high`, `low`, `close`, and `volume`. The data covers 2021-09-27 → 2026-09-25. It is not in the store and is not read through mdq, because the store cannot hold futures (see Prior exposure).
- **Markets (11).** Each is traded as the micro contract named here:

  | Class | Series | Micro contract | Value of one contract |
  |---|---|---|---|
  | Equity | ES=F | MES | $5 × price |
  | Equity | NQ=F | MNQ | $2 × price |
  | Equity | RTY=F | M2K | $5 × price |
  | Equity | YM=F | MYM | $0.50 × price |
  | Rates | ZT=F | 2YY (micro 2-yr yield) | $100,000 / 1.9 = $52,632 notional-equivalent |
  | Rates | ZF=F | 5YY | $100,000 / 4.2 = $23,810 |
  | Rates | ZN=F | 10Y | $100,000 / 6.3 = $15,873 |
  | Rates | ZB=F | 30Y | $100,000 / 11.5 = $8,696 |
  | Commodities | GC=F | MGC | 10 × price |
  | Commodities | HG=F | MHG | 2,500 × price |
  | Commodities | CL=F | MCL | 100 × price |

  **Rates.** The micro yield futures have a DV01 of $10 per contract, but the vendor does not serve their history (`5YY=F`, `10Y=F`, and `30Y=F` return 1 bar; `2YY=F` has 269 zero-volume bars). Their P&L is modelled from the price-quoted note and bond futures. A 1% move in the proxy's price is a yield move of about 1%/D, so one micro yield contract behaves like a notional of $10 / (D × 1e-4) = $100,000 / D in the proxy. D is a fixed modified duration typical of 2022–2026: 1.9 (ZT), 4.2 (ZF), 6.3 (ZN), and 11.5 (ZB). Being long a rates market here means long the bond price, which is short the micro yield contract. An error in D changes risk per contract, not the direction of any position.
- **Excluded before any return was computed:** FX (6E, 6J, 6B, 6A, 6C, 6S), because the vendor rounds prices to 2 decimals; silver (SI), which has 50 zero-volume bars and passes through thin serial months; the micro yield series themselves (as above); natural gas, agriculture, and crypto, which were not downloaded because their micro contracts are new or thin (micro ags listed in 2025) or have very different carry and roll behaviour (crypto).
- **Valid bar.** A bar is valid when open, high, low, and close are all > 0 and volume > 0. Invalid bars are ignored: the market's P&L that day is 0, and no signal, volatility, or fill uses it.
- **Calendar.** The book calendar is every date on which at least one of the 11 markets has a valid bar. Seven dates in 2025 have no valid bar in any market (2025-05-26, 06-18, 06-19, 07-03, 07-04, 08-29, 11-03) and are not sessions. Daily returns are computed on book sessions.
- **Returns (primary basis: open to close).** For market i on valid bar t, the return is `r_i,t = close_t / open_t − 1`. `open` is the Globex open and `close` is the settlement, both of one contract. The price change from the previous close to this open is never used. That leg carries each roll gap (the continuous series switches contract between bars, at dates that cannot be identified reliably in 2025–2026) plus post-settlement and weekend trading. Dropping it removes roll gaps without having to detect them. It is unbiased, but it discards real price variance, so P&L and volatility are understated in magnitude. The close-to-close basis is a reported check only (below).
- **Price index.** `I_i` is the cumulative product of `(1 + r_i,t)` over the market's valid bars.
- **Checks the script must pass before it writes results:** 1,260 raw bars per market with no duplicate dates; the valid-bar count per market equals what `dq.py` reports; every market has at least 253 valid bars on or before the first rebalance date.

## Primary rule

Parameters, all fixed:

| Name | Value | Source |
|---|---|---|
| Lookbacks `K` | 21, 63, and 252 valid bars | Hurst, Ooi & Pedersen (2017), 1, 3, and 12 months |
| Volatility | EWMA of `r²`, centre of mass 60 (λ = 60/61), seeded with the mean of the first 60 `r²`, annualized ×252 | Moskowitz, Ooi & Pedersen (2012) |
| Rebalance | Last book session of each calendar month | Hurst, Ooi & Pedersen (2017), monthly |
| Class weights | Equity 1/3, rates 1/3, commodities 1/3, split equally within each class (each equity or rates market 1/12, each commodity 1/9) | Carver (2015), equal risk per asset class |
| `IDM` | 2.0 | Carver (2015). With assumed correlations of 0.85 within equities, 0.9 within rates, 0.3 within commodities, and 0 across classes, 1/√(w′Hw) = 1.96 |
| `σ_P` | 20% a year | A priori. Hurst et al. use 10%, but at $100,000 a 10% target leaves nearly every micro position below half a contract. 20% is Carver's retail-scale level |
| Starting equity | $100,000 | The user's account size |

1. **Signal.** At the close of rebalance date d, using each market's valid bars up to and including d: `s_i = (1/3) · Σ_K sign(I_i,d / I_i,d−K − 1)`, over K ∈ {21, 63, 252}, where `d−K` is K valid bars earlier and `sign(0) = 0`. So `s_i` ∈ {−1, −1/3, 1/3, 1} (0 only with an exact zero).
2. **Volatility.** `σ_i,d` is the EWMA estimate through bar d: `σ²_t = λ σ²_{t−1} + (1−λ) r²_t`, then `σ = √(252 σ²)`.
3. **Position.** `n_i = round_half_away_from_zero( s_i · E_d · σ_P · w_i · IDM / (V_i,d · σ_i,d) )` contracts. `E_d` is account equity at the close of d. `V_i,d` is the value of one contract: the multiplier × the vendor close at d, or the fixed notional-equivalent for rates. Positions are signed: + is long.
4. **Holding.** `n_i` is held unchanged until the next rebalance. There are no stops, and no exits other than the next rebalance. A market whose rounded position is 0 is flat.
5. **Fills.** A change of `n_i` fills at the **open of market i's next valid bar after d**. The new position earns that bar's open-to-close return, and the old position earns nothing on it (see Returns). If no later valid bar exists, the change is not made.
6. **P&L.** Each valid bar t with position `n` (set by the latest fill at or before t's open) earns:
   - equity and commodities: `n × multiplier × (close_t − open_t)`;
   - rates: `n × notional_equiv × r_i,t`.

   Equity compounds: book P&L for the session is added to E.
7. **Costs,** per contract per side, all-in commission and exchange fees of $1.00 plus 1 tick of slippage:

   | Contract | Tick value | Cost per side |
   |---|---|---|
   | MES | $1.25 | $2.25 |
   | MNQ | $0.50 | $1.50 |
   | M2K | $0.50 | $1.50 |
   | MYM | $0.50 | $1.50 |
   | MGC | $1.00 | $2.00 |
   | MHG | $1.25 | $2.25 |
   | MCL | $1.00 | $2.00 |
   | Micro yield | $1.00 | $2.00 |

   - Rebalance fills pay `|Δn|` × the cost per side, on the fill bar.
   - Holding a contract across a roll pays two sides per contract held (close old, open new), charged on these roll dates: equity index on the third Friday of March, June, September, and December; CL on the 20th of every month; GC on the last session of January, March, May, July, and November; HG on the last session of February, April, June, August, and November; rates (micro yield futures are monthly contracts) on the last session of every month. A date that is not a book session moves to the next book session. The cost is charged to the market's P&L on that session if `n ≠ 0` at its open.
   - No interest is earned on cash and none is paid on margin, so returns are excess returns. A real account would also earn T-bill interest on idle collateral.

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| `close_d` (settlement) | ~13:30–16:00 ET on d, depending on the market | Signal and sizing at the rebalance decision after d's close |
| `I_i,d`, `I_i,d−K` | Close of d | Decision after d's close |
| `σ_i,d` | Close of d (uses `r` through d only) | Decision after d's close |
| `E_d` | Close of d | Decision after d's close |
| Fill price | Open of the next valid bar (18:00 ET Globex reopen, at least an hour after d's last settlement) | Fill |
| Roll dates for costs | The exchange calendar, known in advance | Cost charge |

No statistic is estimated on the full sample. IDM, the weights, the durations, and the costs are fixed constants.

## Samples

- **Warm-up.** 2021-09-27 → 2022-09-30. The first rebalance is the last book session of the first calendar month-end at which every market has at least 253 valid bars (expected: 2022-09-30).
- **In-sample:** the first session after the first rebalance (expected 2022-10-03) → 2024-09-30.
- **Out-of-sample:** 2024-10-01 → 2026-09-25.
- **Why this split.** It is the most recent two years, and the only split that leaves two years each side after a 12-month warm-up. The OOS window overlaps the calendar window of the earlier intraday studies (equity ETFs only) and is partly known to me in broad terms (see Prior exposure).

## Benchmarks

Uncosted, on the same sessions and the same open-to-close basis:

- **B1: long ES at 1× notional.** The daily return is `r_ES,t`.
- **B2: long-only risk parity.** The secondary fractional book with `s_i ≡ +1` for every market (same volatility scaling, weights, IDM, and monthly rebalance), uncosted. It separates the value of the trend timing from being long the same volatility-scaled markets.

## Secondary candidate

**S1: fractional book (signal test).** Identical to the primary, except that `n_i` is not rounded (fractional contracts) and costs apply to fractional `|Δn|`. It measures the signal without whole-contract granularity. It is never promoted in place of the primary.

S1 acceptance: OOS Sharpe ≥ 0.5, direction placebo p ≤ 0.05 on the full sample, and IS Sharpe > 0, all at base cost.

## Definitions

- **Daily return:** book P&L on a session ÷ E at the previous session's close.
- **Sharpe:** mean ÷ sample SD of daily returns × √252, with a zero risk-free rate.
- **Trade:** per market, a maximal run of bars with a nonzero position of the same sign. It starts at the fill that opens it and ends at the fill that sets the position to 0 or flips the sign (a flip ends one trade and starts another at the same fill). A size change of the same sign stays inside the trade. A trade still open on 2026-09-25 is marked at that bar's close with exit reason `end`; no exit cost is charged.
  - Trade P&L is the sum of the market's bar P&L over the trade, net of the costs charged to it: its entry fill, same-sign resizes, rolls, and its exit fill. A flip's fill cost is split between the closing and the opening legs in proportion to contracts.
  - Trade return in bp is trade net P&L ÷ (|contracts at entry| × V at entry).
  - Profit factor is Σ winning trade net P&L ÷ |Σ losing trade net P&L|.
- **Exposure:** the share of sessions on which at least one market has a nonzero position.
- **Sleeve Sharpe:** the Sharpe of that asset class's P&L ÷ total book E at the previous close.

## Reported checks

All of these appear in the report whatever they show.

- **Metrics** for full / IS / OOS at base cost: the protocol defaults (total return, CAGR, annual volatility, Sharpe, max drawdown, t-stat, trades, win rate, profit factor, average net trade in bp, exposure), for the primary, S1, B1, and B2.
- **Breakdowns:**
  - calendar year;
  - long vs short trades;
  - by asset class and by market (P&L and Sharpe contribution);
  - trades by exit type (flip, to flat, end);
  - primary monthly returns by quintile of B1's monthly return.
- **Costs:** 0, 0.5×, 1×, 2×, and 3× the base cost (commission and slippage scaled together).
- **Fill delay:** fills at the open of the second valid bar after d.
- **Return basis (sensitivity, not a verdict input):** the primary on close-to-close returns. On roll days found by the `rollcheck.py` rule, the return is open to close instead: the largest volume-ratio day in each expected roll month, plus any day with a ratio ≥ 3.0. The report states that this rule is known to misidentify some 2025–2026 rolls.
- **Granularity:**
  - the correlation of the primary's and S1's daily returns;
  - per market, the share of rebalances at which S1's position is nonzero but the primary's rounds to 0;
  - the average of |primary contracts| ÷ |S1 contracts| where S1 is nonzero.
- **Direction placebo** on gross trade P&L: each trade's gross bar P&L is multiplied by an independent ±1, then the gross daily Sharpe is recomputed (using the actual equity path as denominator). 2,000 draws, seed 20260926. p = (1 + #draws ≥ actual) / 2,001.
- **Timing placebo:** not run. The direction placebo and B2 already separate timing from exposure, and random monthly signs add little.
- **Block bootstrap:** circular, 20-session blocks, 2,000 draws, seed 20260926. 95% interval of the full-sample Sharpe.
- **Plateau grid on IS:** lookback scale ∈ {0.5, 0.75, 1.0} (K = round(21x), round(63x), round(252x)) × rebalance ∈ {weekly (last book session of each ISO week), monthly}: 6 cells, the primary among them. Every cell uses the same warm-up and first P&L session. OOS is shown for selection bias only. Nothing is selected from the grid.
- **Breadth (the cross-market line):** no untouched related market with usable data exists (FX and silver are excluded above), so criterion 5 uses the three asset-class sleeves of the primary.
- **Verification:**
  - a self-test on hand-built synthetic bars with hand-computed expected positions, trades, and P&L. It covers the signal signs, rounding half away from zero, the month-end rebalance, fills at the next valid bar's open, invalid bars, roll costs, flips, and end-of-data marking;
  - `verify.py`, a naive, independent re-implementation, must reproduce every primary trade (market, side, entry date and price, exit date and price) and every daily P&L to within $0.01.

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at base cost:

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. IS Sharpe > 0, and at least 60% of the IS grid cells (4 of 6) have Sharpe > 0.
4. Full-sample total return > 0 at 2× base cost.
5. OOS Sharpe > 0 in at least 2 of the 3 asset-class sleeves.
6. Minimum sample: at least 40 OOS trades. Below that, the verdict is **Inconclusive**.

**Changes from the protocol defaults.**
- Line 5 uses asset-class sleeves instead of a separate instrument, because none with usable data remains.
- Line 6 is 40 rather than 100. Monthly rebalancing of 11 markets over two years produces tens of trades, not hundreds.

A failed line fails the strategy.

## Not done in this study

The report will not promote any of these in place of the primary:
- a grid cell, weekly rebalancing, or other lookbacks;
- the fractional book S1;
- the close-to-close return basis;
- one asset class, one market, or one side alone;
- a different volatility target, IDM, account size, or duration conversion;
- a different cost, fill, or sample split;
- adding or dropping markets after seeing results.
