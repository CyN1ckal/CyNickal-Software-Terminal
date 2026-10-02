# Diversified trend following on micro futures: a $100k account, 11 CME markets

| | |
|---|---|
| Date | 2026-09-26 |
| Status | **Rejected.** Failed all 7 performance lines of the pre-registered acceptance table; the minimum sample was met. |
| Instruments | MES, MNQ, M2K, MYM (equity index); micro 2Y/5Y/10Y/30Y yield futures (rates, modelled from ZT/ZF/ZN/ZB); MGC, MHG, MCL (commodities). Whole contracts, $100,000 start, monthly rebalance, positions held for weeks to months |
| Data | MBoum continuous front-month daily bars, 2021-09-27 → 2026-09-25, stored in the study folder (the store cannot hold futures). Evaluation 2022-10-03 → 2026-09-25, 997 sessions. FX and silver excluded before the run (vendor rounds prices to 2 decimals; silver has thin serial months) |
| Rules | [`research/micro-futures-trend/research/RULES.md`](../research/RULES.md), locked 2026-09-26 16:24 UTC, sha256 `af095ca58156` |
| Code | [`research/micro-futures-trend/research/`](../research/) · 1 pre-registered run, 1 verification, 2 post hoc runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** The Hurst–Ooi–Pedersen trend rule (1-, 3-, and 12-month signals, volatility-scaled, monthly rebalance) traded with whole micro contracts lost money in both halves. Out of sample (2024-10-01 → 2026-09-25) it returned **−7.8%** after costs, Sharpe **−0.19**, profit factor 0.90 over 81 trades. In sample it returned −14.8% (Sharpe −0.46). Over the whole evaluation it returned **−21.5%** (Sharpe −0.33, max drawdown −33.6%), while simply being long the same volatility-scaled markets returned +27.2% and long ES returned +93.1%.

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | 2022-10-03 → 2026-09-25 | 2022-10-03 → 2024-09-30 | 2024-10-01 → 2026-09-25 |
| Sessions | 997 | 502 | 495 |
| Total return | **−21.5%** | −14.8% | **−7.8%** |
| CAGR | −5.9% | −7.7% | −4.0% |
| Annual volatility | 15.2% | 15.1% | 15.3% |
| Sharpe | **−0.33** | −0.46 | **−0.19** |
| Max drawdown | −33.6% | −24.7% | −20.0% |
| Trades / profit factor | 155 / 0.65 | 74 / 0.49 | 81 / 0.90 |
| Avg net trade | −65.9 bp | −145.6 bp | +6.9 bp |
| *B1 long ES: Sharpe (max DD)* | *1.18 (−17.4%)* | *1.42 (−11.7%)* | *0.94 (−17.4%)* |
| *B2 long-only risk parity: Sharpe (max DD)* | *0.38 (−26.6%)* | *0.48 (−26.6%)* | *0.29 (−23.1%)* |

It failed 7 of the 7 performance tests written before the first run (§8). The minimum-sample line (≥ 40 OOS trades) was met with 81, so the verdict is Rejected, not Inconclusive.

**Why it failed:**

1. **There was no gross edge.** Before any cost the book lost 18.3% (gross Sharpe −0.26 full, −0.12 OOS). At zero cost the OOS Sharpe was still −0.15 (§6). Costs ($3,120 over four years) are not the problem.
2. **Whole contracts are not the problem either.** The fractional book S1, with no rounding, did slightly worse: Sharpe −0.36 full and −0.21 OOS. Its daily returns correlated 0.96 with the primary's.
3. **The direction of the trades carried no information.** Flipping each trade's direction at random gave a mean gross Sharpe of −0.02; the actual −0.26 sits in the lower half of that distribution (p = 0.77).
4. **Two of three asset classes lost.** Net P&L was −$12,676 in rates (OOS sleeve Sharpe −1.12), −$10,416 in equities, and +$1,638 in commodities (§6). Rates were held every session and flipped 16–21 times in 48 rebalances **(post hoc)**.
5. **It lost when equities rallied.** In the fifth of months when ES rose most (mean +7.4%), the book lost 2.3% a month on average. The "crisis alpha" convexity trend following is known for did not appear here (§3).

**Recommendation.** Do not trade it. The rules forbid promoting any of these after the fact: the close-to-close return basis (full Sharpe −0.03, OOS +0.04), the weekly short-lookback grid cell (the only positive IS cell, +0.06, which was −0.63 out of sample), the commodities sleeve alone, or the fractional book.

**What this does and does not say.** Four years is a short test for a strategy whose published edge comes from decades and hundreds of markets. The block-bootstrap 95% interval of the full-sample Sharpe is −1.07 to +0.42, which does not exclude a small positive long-run Sharpe. What the test does say is that over 2022-10 → 2026-09, this rule on these 11 markets earned nothing gross, underperformed simply holding the same markets long, and had a drawdown of a third of the account.

## 2. The strategy

### Rules

```
Universe: ES NQ RTY YM | ZT ZF ZN ZB (as micro yield futures) | GC HG CL
Returns:  r = close/open - 1 on each valid bar (Globex open -> settlement; never close(t-1) -> open(t))
At the close of the last session of each month, for each market:
    s     = mean( sign(12m ret), sign(3m ret), sign(1m ret) )     # 252 / 63 / 21 valid bars
    sigma = sqrt(252 * EWMA(r^2, centre of mass 60))
    n     = round_half_away( s * Equity * 20% * w_class * 2.0 / (contract value * sigma) )
    w_class = 1/12 for each equity or rates market, 1/9 for each commodity
Fill the change at the next valid bar's open. Hold until the next month end.
Costs: $1 + 1 tick per contract per side; 2 sides per contract held at each roll.
```

- **Why open-to-close returns:** the vendor's continuous series switch contract between bars, and in 2025–2026 the switch dates cannot be found from volume. Using only open→close removes every roll gap without having to detect it, at the price of dropping post-settlement and weekend moves (§4, §6).
- **Why 20% volatility and IDM 2.0:** at 10% (Hurst et al.) almost every micro position rounds to zero at $100,000; 2.0 is the diversification multiplier implied by the assumed correlations (RULES.md).

### How it trades

| | |
|---|---|
| Trades per year | 39.2 **(post hoc)** |
| Time in market | 100% of sessions (rates always had a position) |
| Holding time | median 59 days / mean 74 days **(post hoc)** |
| Largest position | 4 contracts **(post hoc)** |
| Long / short | 82 trades, PF 0.71 / 73 trades, PF 0.61 |
| Win rate | 31% · avg winner $846 / avg loser −$580 |

Whole-contract rounding mattered most in equity index. At rebalances where S1 held a position, the primary rounded MNQ to zero 88% of the time, MES 67%, M2K 44%, and MYM 31%; rates never rounded to zero (§7).

## 3. Hypothesis and predictions

Prices underreact to news early and overreact later, so past returns over 1–12 months predict the sign of the next month's return. The counterparties are hedgers, central banks, and discretionary traders who lean against moves (Moskowitz, Ooi & Pedersen 2012; Hurst, Ooi & Pedersen 2017).

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| 1. Payoff ratio > 1 (average winning trade larger than average losing trade) | 1.46 ($846 vs $580), with a 31% win rate | Consistent |
| 2. Mean monthly return in the top and bottom ES quintiles exceeds the middle three | Tails −1.33% vs middle +0.14% a month; the top ES quintile was the book's worst (−2.31%) | Not consistent |
| 3. S1 gross P&L positive in ≥ 2 of 3 asset classes | Equity −$10,897, rates −$11,261, commodities +$1,875 | Not consistent |

Only the payoff-shape prediction held, and a payoff ratio above 1 alongside a 31% win rate and a profit factor of 0.65 is a shape without an edge. The mechanism is not supported in this sample.

## 4. Method

- **Data.** The store has no futures, and `ingest` cannot store them (it confirms tickers with OpenFIGI and uses NYSE sessions). `fetch.py` downloaded MBoum v1 daily continuous series with the user's permission (22 requests for the series, about 11 more for probes, well under the 2,000 limit). The raw JSON is kept in `research/data/raw/`. Nothing was written to `data/`, and `ingest` was not run. The vendor serves only 5 years of daily history for futures, so the evaluation window is four years after a one-year warm-up.
- **Coverage and exclusions.**
  - FX futures: the vendor rounds every price to 2 decimals, which leaves `6E=F` with 26 distinct closes in 1,260 bars. Excluded.
  - Silver: 50 zero-volume bars and switching through thin serial months. Excluded.
  - Micro yield futures: not served (`5YY`, `10Y`, `30Y`), or sparse (`2YY`). Their P&L is modelled from ZT, ZF, ZN, and ZB with fixed durations (1.9 / 4.2 / 6.3 / 11.5).
  - Bars with a non-positive price or zero volume are ignored. Seven 2025 dates have no valid bar in any market and are not sessions.
  - Copper's 2-decimal rounding (about $4.50/lb) is a coarse tick but was kept.
- **Pre-registration.** RULES.md fixed the universe, signal, sizing, costs, samples, checks, and acceptance before any return was computed. Pre-lock looks (all recorded in RULES.md) were counts, volume, decimal precision, and a non-directional check of what the `open` field means. **Prior exposure:** from general knowledge I knew that trend-following indices lost in 2023 and in early 2025, and roughly how equities, gold, rates, and crude moved. The OOS window was not unseen data. RULES.md and RULES.lock were not committed to git before the first run; the order is recorded by the lock timestamp (16:24:41 UTC) and the first RUNLOG entry (16:29:23 UTC).
- **Fills and costs.** Changes fill at the next valid bar's open (the 18:00 ET Globex reopen). The cost per side is $1 all-in plus one tick ($1.25 MES, $0.50 MNQ/M2K/MYM, $1 MGC/MCL/micro yield, $1.25 MHG), with two sides per held contract at each roll. Rates roll monthly because micro yield futures are monthly contracts, so each rates market cost $548–$576 over the test, more than any other market. No interest was earned on cash, so these are excess returns.
- **Returns.** Daily simple returns on the book calendar: session P&L ÷ previous equity, compounding. Sharpe is mean ÷ SD × √252 with a zero risk-free rate. Trades are assigned to IS or OOS by entry date.
- **Verification.** The self-test in `backtest.py` checks hand-computed positions, trades, and P&L. It covers rounding, EWMA seeding, month-end selection, a fill that skips an invalid bar, a roll cost moved to the next session, a flip with a split cost, end-of-data marking, a fill with no later bar, and fractional sizing. `verify.py` is an independent, naive re-implementation. It matched all 155 trades exactly and every one of 1,253 sessions' P&L to within $0.000001.
- **Runs.**
  - One pre-registered run. No rerun was needed.
  - Before that run, the self-test caught a NumPy sign bug, and the month-end roll-cost date was corrected to exclude the partial final month. Neither changed a result that had been seen.
  - Two post hoc runs (`posthoc.py`) reproduced the primary unchanged.
  - Rules read literally where silent: trades are windowed by entry date; the roll cost uses the position held through the session.

## 5. Results

![Growth of $1](figures/equity.svg)

The trend books lost in each of the first three calendar years (−8.0%, −8.6%, −10.7%) and never recovered, while both benchmarks ended higher. The primary and the fractional S1 are almost indistinguishable, which shows rounding was not the issue.

![Drawdown](figures/drawdown.svg)

The primary's maximum drawdown was −33.6%, against −17.4% for long ES.

| Strategy (1× cost) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** (whole micros) | −21.5% / −0.33 / −33.6% | −0.46 | −7.8% / −0.19 / −20.0% |
| S1 fractional | −23.4% / −0.36 / −33.6% | −0.52 | −8.9% / −0.21 / −19.6% |
| *B1 long ES (uncosted)* | +93.1% / 1.18 / −17.4% | 1.42 | +30.6% / 0.94 / −17.4% |
| *B2 long-only risk parity (uncosted)* | +27.2% / 0.38 / −26.6% | 0.48 | +8.0% / 0.29 / −23.1% |

![Calendar-year return](figures/by_year.svg)

| Year | Primary | Sharpe | Max DD | B1 long ES | B2 long-only RP |
|---|---:|---:|---:|---:|---:|
| 2022 (Q4) | −8.0% | −1.36 | −11.6% | +7.9% | +8.5% |
| 2023 | −8.6% | −0.59 | −11.8% | +17.7% | −1.9% |
| 2024 | −10.7% | −0.75 | −15.8% | +14.0% | −4.7% |
| 2025 | +3.4% | 0.28 | −13.3% | +11.8% | +23.8% |
| 2026 (to 9/25) | +1.1% | 0.18 | −13.6% | +19.3% | +1.3% |

S1 by year: −8.9%, −9.6%, −9.4%, +3.7%, −1.0%.

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

Each of the 155 trades kept its timing and size while its gross daily P&L was multiplied by an independent ±1, over 2,000 draws. The null mean gross Sharpe was −0.02 (SD 0.34, 95th percentile 0.52). The actual gross Sharpe was −0.26, p = 0.77. S1's placebo gave p = 0.80. The trade directions did no better than coin flips.

### Bootstrap

A circular block bootstrap (20-session blocks, 2,000 draws) puts the 95% interval of the full-sample Sharpe at −1.07 to +0.42 (median −0.33). The OOS t-statistic of the mean daily return is −0.27. The interval is wide: four years cannot rule out a small positive long-run edge, and they cannot rule out a large negative one either.

### Parameter plateau

![Parameter grid](figures/grid.svg)

1 of 6 IS cells had a Sharpe above 0: 0.5× lookbacks (10/32/126 bars) with weekly rebalancing, at +0.06. The required minimum was 4. That cell had the worst OOS Sharpe of the grid (−0.63), so picking the IS winner would have made the OOS result worse. The other five IS cells ranged from −0.41 to −0.53. OOS Sharpe was negative in every cell (−0.04 to −0.63). Nothing was selected from the grid.

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost multiple | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | −0.29 | −0.30 | **−0.33** | −0.37 | −0.40 |
| OOS Sharpe | −0.15 | −0.16 | **−0.19** | −0.25 | −0.28 |
| Full-sample return | −19.9% | −20.4% | **−21.5%** | −23.4% | −25.4% |

There is no break-even cost: the book loses at zero cost. Filling one bar later changed little: full Sharpe −0.35, OOS −0.19, full return −22.3%.

### Return basis (pre-registered sensitivity)

On close-to-close returns, with roll days found by the volume rule, the book returned −5.4% (Sharpe −0.03) over the full sample and −1.0% (Sharpe +0.04, PF 1.38) out of sample. RULES.md stated before the run that this rule misidentifies some 2025–2026 rolls.

**(post hoc)** On the primary's own positions, the close→open leg that the primary ignores would have added $2,808 within 3 sessions of a roll date and $84 on all other sessions. The roll-window part is mostly the roll spread between contracts rather than a price move. So the gap between the two bases comes mostly from roll artefacts in the close-to-close series, not from real overnight trend moves the primary missed. Adding the whole dropped leg to the primary would still leave it deeply negative.

### Other markets (identical rules)

No untouched market with usable data remained. FX and silver were excluded for data quality before the run. The pre-registered breadth test uses the three sleeves instead:

| Sleeve | IS Sharpe | OOS Sharpe | Full: net P&L / Sharpe |
|---|---:|---:|---|
| Equity (MES, MNQ, M2K, MYM) | −0.48 | −0.12 | −$10,416 / −0.30 |
| Rates (2YY, 5YY, 10Y, 30Y) | +0.10 | −1.12 | −$12,676 / −0.42 |
| Commodities (MGC, MHG, MCL) | −0.64 | +0.88 | +$1,638 / +0.19 |

Only commodities were positive out of sample (1 of 3; 2 required). The sleeves traded places between halves: rates were the one positive IS sleeve and the worst OOS; commodities were the worst IS and the only positive OOS.

## 7. Where the result comes from

![By asset class](figures/sleeves.svg)

Out of sample, rates did most of the damage (−$13,541) and commodities made money (+$9,316). In sample, equities (−$8,030) and commodities (−$7,678) both lost, while rates made +$865.

| Market | Micro | Net P&L | Sharpe full / OOS | Trades | Share of sessions held | Rounded to 0 vs S1 |
|---|---|---:|---|---:|---:|---:|
| ES | MES | −$1,519 | −0.11 / −0.45 | 8 | 34% | 67% |
| NQ | MNQ | +$124 | 0.08 / — | 2 | 12% | 88% |
| RTY | M2K | −$4,738 | −0.54 / 0.10 | 15 | 57% | 44% |
| YM | MYM | −$4,283 | −0.32 / 0.08 | 13 | 69% | 31% |
| ZT | 2YY | −$2,193 | −0.21 / −0.62 | 17 | 100% | 0% |
| ZF | 5YY | −$2,271 | −0.23 / −0.53 | 17 | 100% | 0% |
| ZN | 10Y | −$3,525 | −0.42 / −1.03 | 21 | 100% | 0% |
| ZB | 30Y | −$4,688 | −0.61 / −1.49 | 22 | 100% | 0% |
| GC | MGC | −$3,613 | −0.12 / 0.73 | 13 | 53% | 48% |
| HG | MHG | +$729 | 0.12 / 0.64 | 17 | 84% | 17% |
| CL | MCL | +$4,521 | 0.35 / 0.14 | 10 | 81% | 19% |

NQ had no OOS position, so it has no OOS Sharpe. "Rounded to 0 vs S1" is the share of monthly rebalances at which S1 held a position and the primary's rounded to zero.

![By move quintile](figures/move_quintiles.svg)

| ES monthly quintile | Months | ES mean | Primary mean |
|---|---:|---:|---:|
| Q1 (worst) | 10 | −4.1% | −0.44% |
| Q2 | 10 | −0.4% | −0.17% |
| Q3 | 10 | +1.7% | −0.55% |
| Q4 | 9 | +3.6% | +1.25% |
| Q5 (best) | 9 | +7.4% | −2.31% |

- **By side:** long trades lost $8,310 (PF 0.71) and short trades lost $13,145 (PF 0.61).
- **By exit:** trades that ended in a flip lost $34,301 over 103 trades; trades that ended flat made $6,420 over 48; trades still open at the end made $6,427 over 4.

None of these breakdowns was used to filter the rule.

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| OOS Sharpe | ≥ 0.50 | −0.19 | ❌ |
| OOS profit factor | ≥ 1.10 | 0.90 | ❌ |
| Direction placebo p (full sample) | ≤ 0.05 | 0.77 | ❌ |
| IS Sharpe | > 0 | −0.46 | ❌ |
| IS grid cells with Sharpe > 0 | ≥ 4 of 6 | 1 of 6 | ❌ |
| Full-sample return at 2× cost | > 0 | −23.4% | ❌ |
| OOS sleeves with Sharpe > 0 | ≥ 2 of 3 | 1 of 3 | ❌ |
| Minimum sample: OOS trades | ≥ 40 | 81 | ✅ (verdict is not Inconclusive) |

S1, the fractional signal test, also failed its own line: OOS Sharpe −0.21 (≥ 0.5 required), placebo p 0.80 (≤ 0.05), and IS Sharpe −0.52 (> 0).

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

**(post hoc)** The trailing 252-session Sharpe was positive in 24% of windows. It ranged from −1.33 (year ending 2025-02-27) to +0.61 (year ending 2026-08-06). The last year (2025-09-24 → 2026-09-25) returned +4.9%, Sharpe 0.42. This is a later, favourable stretch of a failed rule, not evidence for it.

| Risk | Evidence | What it means |
|---|---|---|
| Short sample | 4 evaluation years; bootstrap Sharpe interval −1.07 to +0.42 | The test has low power. A long-run Sharpe of about 0.3–0.5 cannot be ruled out, but nothing here supports trading it |
| Concentration in a few days **(post hoc)** | Without the 10 worst sessions: +20.6%; without the 10 best: −41.3% | The result swings on a handful of days in either direction |
| Few independent bets | Four highly correlated rates markets always held; equity positions mostly rounded away | The "11-market" book is closer to 3–4 bets. Rates alone decided the OOS result |
| Proxy data | Micro yield P&L modelled from ZT/ZF/ZN/ZB with fixed durations; front-month continuations; 2-decimal prices | Rates contracts per unit of risk may be off by the duration error; the signs of positions are unaffected |
| Open-to-close basis | The ignored close→open leg was worth +$2,808 near rolls and +$84 elsewhere on the primary's positions **(post hoc)** | Not large enough to change the verdict |
| Missing universe | FX and silver excluded; no agriculture, energy beyond crude, or non-US markets | Historically, much of trend following's return came from currencies and a wide commodity set. This book lacks both |
| Prior knowledge | I knew in broad terms that trend-following indices lost in 2023 and early 2025 | Parameters were published defaults, so this did not steer them, but the OOS window was not blind |

## 10. Ideas for a new study

These were suggested by this data and must be tested on data this study has not used: later data, other markets, or paper trading.

- **A data source that fixes the gaps:** back-adjusted continuous contracts with full precision and a long history (e.g. 20+ years). That would allow FX, more commodities, and a proper multi-decade test, where the published evidence for this rule lives. It is the single change most likely to make a future test informative.
- **(found on this data)** Always-on rates positions flipped often. A test of whether a no-trade band (hold the old position unless the signal changes by more than one vote) reduces whipsaw would need its own rules and unseen data.
- **(found on this data)** Commodities were the only positive sleeve out of sample. That is a single-sleeve observation from two years and should not be traded without a separate study.
