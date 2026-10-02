# <Strategy name>: pre-registered rules

Written <YYYY-MM-DD>, before any return was computed from the store. Results may not edit this file. Anything computed after the first run goes in the report, labelled **(post hoc)**. It never replaces the primary.

<!-- Delete every comment before locking. Replace every <placeholder>. A rule that is left vague here will be resolved after seeing results, and that is fitting. -->

## Prior exposure

- **Looks at the store before this file:** <none | the exact counts/coverage checks run, with the script name and what they returned. No outcome statistics.>
- **Earlier studies on the same instruments, periods, or mechanism:** <slug: what it found. "None" if none.>
- **What I already know about the test windows:** <e.g., "the OOS window 2024-07-01 → 2026-09-25 was used by qqq-intraday-trend; I know QQQ trended strongly in April 2025 and that noise-boundary momentum was positive OOS on QQQ and negative on SPY.">
- **Where the parameters came from:** <published defaults | a priori reasoning below | the user's suggestion, and its origin.>

## Hypothesis

<One sentence: what is predictable, in which instrument, over what horizon.>

**Mechanism.** <Who is on the other side, why they trade that way, and why they keep paying for it. Cite sources.>

**Known counter-forces.** <What could make the effect run the other way. The report must show how the strategy behaves where they apply.>

## Predictions beyond P&L

If the mechanism is right, then:

1. <e.g., profit concentrates in the top quintile of |open→close| days.>
2. <e.g., the average fade-signed forward return decays after 15 minutes.>
3. <…>

Each one is scored in the report as *consistent*, *not consistent*, or *not testable*.

## Data

- Instruments: <symbols>. Read with `agent-data/mdq.py`. <Split-adjusted; dividends not adjusted, and why that does or does not matter.>
- Bars: <timeframe; how any coarser bar is built (mdq.resample, 09:30 anchor); what the timestamp means.>
- Calendar: <book sessions; how missing sessions are counted; known gaps such as 2021-12-31 and 2025-01-09; early closes via `mdq.EARLY_CLOSES`.>
- Missing bars: <forward-fill or leave missing, and where each applies.>
- Data checks the script must pass before it writes results: <e.g., full sessions have 390 1-minute bars and early closes have 211.>

## Primary rule

Parameters, all fixed: <NAME = value (source), …>

1. **<Indicator/state>.** <Exact formula, with the lookback and the prior-only data it uses.>
2. **Signal.** <Evaluated at which bar's close; the exact inequality; the allowed decision times.>
3. **Position.** <Maximum positions; flips; re-entry blocks.>
4. **Exits,** in the order they are checked: <target / stop / time / opposite signal / session end, each exact.>
5. **Fills.** <Price and bar, e.g. the next bar's open; cutoffs; what happens when no later bar exists.>
6. **Sizing.** <Notional; leverage; how equity compounds; book weights for more than one name.>
7. **Costs.** <bp per side and the basis for it; flips pay both legs; borrow; cash interest.>

## Look-ahead audit

| Input | Known at | Used at |
|---|---|---|
| <e.g., σ(d,k)> | <close of session d−1> | <decision at minute k of d> |

## Samples

- Warm-up: <…>
- **In-sample:** <first evaluable session> → <date>.
- **Out-of-sample:** <date> → <last session>.
- <Why this split. Whether an earlier study has exposed the OOS window.>

## Benchmarks

- <e.g., buy-and-hold close to close; long open to close daily. Uncosted, on the same sessions.>

## Secondary candidates (optional)

<At most two, each fully specified, each with its own acceptance line. A secondary is never promoted when the primary fails. Delete this section if unused.>

## Reported checks

All of these appear in the report whatever they show.

- Metrics for full / IS / OOS at base cost: <list, or "the protocol defaults">.
- Breakdowns: year; side; exit reason; <entry time | regime>; quintile of <market move>.
- Costs: <0, 0.5, 1, 2, 3> bp per side. Fill delay: <+1 bar>. Upper bound: <signal-close fill, if used>.
- Direction placebo on gross returns: <2,000> draws, seed <…>.
- Timing placebo: <details | not applicable, because…>.
- Block bootstrap: <20>-session blocks, <2,000> draws, seed <…>.
- Plateau grid on IS: <param ∈ {…} × param ∈ {…}>, <N> cells. OOS is shown for selection bias only.
- Cross-market: identical rules on <symbols>.
- Verification: a self-test on synthetic sessions, and `verify.py` on <≥40> seeded random sessions (seed <…>).

## Acceptance

The primary is a **paper-trading candidate** only if every line holds at <base cost> per side:

1. OOS Sharpe ≥ <0.5> and OOS profit factor ≥ <1.10>.
2. Direction placebo p ≤ <0.05> on the full sample.
3. IS Sharpe > 0, and at least <60%> of the IS grid cells have Sharpe > 0.
4. Full-sample total return > 0 at <2× base> cost per side.
5. OOS Sharpe > 0 on <at least one of: …> under identical rules.
6. Minimum sample: at least <100> OOS trades. Below that, the verdict is **Inconclusive**.

<Reason for any change from the protocol defaults.>

A failed line fails the strategy.

## Not done in this study

The report will not promote any of these in place of the primary: a grid cell; the long side or short side alone; a single symbol; a time-of-day or regime filter; a different cost, fill, or sample split; <study-specific rescues>.
