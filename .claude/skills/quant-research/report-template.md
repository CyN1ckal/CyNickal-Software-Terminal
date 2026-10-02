# <Strategy name>: <instruments>

<!-- The top-level overview. A reader who stops after §1 must know the verdict, the OOS result, why, and the main risks. Every number comes from output files in research/<slug>/research/. Delete comments. -->

| | |
|---|---|
| Date | <YYYY-MM-DD> |
| Status | **<Paper-trading candidate / Rejected / Inconclusive / Void>.** <One clause, e.g. "Failed 3 of 6 pre-registered tests."> |
| Instruments | <symbols, session, holding style> |
| Data | <first → last session>, read via `agent-data/mdq.py`. <Excluded gaps.> |
| Rules | [`research/<slug>/research/RULES.md`](../research/RULES.md), locked <date>, sha256 `<first 12 hex>` |
| Code | [`research/<slug>/research/`](../research/) · <N> store runs (see [RUNLOG](../research/RUNLOG.md)) |

## 1. Summary

**Result.** <Two or three sentences: what was tested, the OOS return and Sharpe after costs, and the full-sample figures. Lead with OOS.>

| | Full sample | In-sample | Out-of-sample |
|---|---:|---:|---:|
| Window | | | |
| Sessions | | | |
| Total return | | | |
| CAGR | | | |
| Annual volatility | | | |
| Sharpe | | | |
| Max drawdown | | | |
| Trades / profit factor | | | |
| Avg net trade | | | |
| *Benchmark Sharpe (max DD)* | | | |

It <passed all / failed K of> the <N> acceptance tests written before the first run (§8).

**Why.** <Numbered reasons, each backed by a check in the body. For a pass, these are the risks: decay, concentration, cross-market, costs, sample length. For a failure, these are the causes: gross edge, costs, correlation, decay.>

**Recommendation.** <Candidate: paper trading, with the stop rules in §10. Otherwise: "Do not trade it," and list the variants the rules forbid promoting.>

## 2. The strategy

### Rules

```
<pseudo-code of the primary rule, exactly as locked>
```

- **Why <design choice>:** <reason from RULES.md>.

### How it trades

| | |
|---|---|
| Sessions with a trade | |
| Trades per year | |
| Time in market | |
| Holding time | median / mean |
| Long / short | count, PF each |
| Win rate | avg winner / avg loser |

## 3. Hypothesis and predictions

<The mechanism in a short paragraph, with citations.>

| Pre-registered prediction | Evidence | Scored |
|---|---|---|
| <1> | <number, figure> | Consistent / Not consistent / Not testable |

<If P&L passed but the predictions did not, say the mechanism is unconfirmed.>

## 4. Method

- **Data.** <Source, coverage check results, gaps and how they were handled, corporate actions.>
- **Pre-registration.** <What RULES.md fixed. Prior exposure, in one or two sentences. Whether RULES.md was committed before the first run.>
- **Fills and costs.** <Base fill; cost basis compared with the quoted spread.>
- **Returns.** <Daily simple; flat days = 0; Sharpe definition.>
- **Verification.** <Self-test coverage; verify.py sample and match result.>
- **Runs.** <N store runs. Reasons. Any bug fix and how it moved the headline (direction and size). Any deviation from the literal rules.>

## 5. Results

![Growth of $1](figures/equity.svg)

<One sentence: what the figure shows, including the unflattering part. The IS/OOS boundary is marked.>

![Drawdown](figures/drawdown.svg)

| Strategy (<cost>) | Full: return / Sharpe / max DD | IS Sharpe | OOS: return / Sharpe / max DD |
|---|---|---:|---|
| **Primary** | | | |
| <Secondary, if any> | | | |
| *<Benchmark>* | | | |

![Calendar-year return](figures/by_year.svg)

| Year | Strategy | Sharpe | Max DD | Benchmark |
|---|---:|---:|---:|---:|

## 6. Is it real?

### Placebo

![Placebo](figures/placebo.svg)

<Null construction; null mean; actual; p. Say what the placebo does and does not show.>

### Bootstrap

<95% interval of full-sample Sharpe; OOS t-stat.>

### Parameter plateau

![Parameter grid](figures/grid.svg)

<Share of IS cells > 0; where the primary ranks. OOS panel: what picking the IS winner would have done; IS/OOS rank correlation. "Nothing was selected from the grid.">

### Costs and latency

![Cost sensitivity](figures/costs.svg)

| Cost per side | 0 | 0.5× | **1×** | 2× | 3× |
|---|---:|---:|---:|---:|---:|
| Full-sample Sharpe | | | | | |
| OOS Sharpe | | | | | |
| Full-sample return | | | | | |

<Break-even cost. Effect of one bar of extra delay.>

### Other markets (identical rules)

| | IS Sharpe | OOS Sharpe | Full: return / PF |
|---|---:|---:|---|

## 7. Where the result comes from

![By move quintile](figures/move_quintiles.svg)

<Quintile table. By side, by exit reason, by entry time or regime. State that none of these breakdowns was used to filter the rule.>

## 8. Acceptance tests (fixed before the first run)

| Criterion | Required | Result | |
|---|---|---|---|
| | | | ✅ / ❌ |

<One line on anything a pass or fail here does not mean, e.g. "a costed placebo is a low bar.">

## 9. Risks and weaknesses

![Rolling Sharpe](figures/rolling_sharpe.svg)

| Risk | Evidence | Mitigation |
|---|---|---|
| Edge decay | | |
| Concentration in a few days | | |
| Generalization | | |
| Execution (costs, latency, closing print) | | |
| Short sample / regime coverage | | |

## 10. <Deployment proposal (paper trading) | No deployment>

<Candidate: account and sizing; exact signal times; order types; early-close handling; logging fills into a terminal ledger; review date; numeric stop rules, including "a negative 6-month return is not by itself a stop" if the backtest had long flat spells.>

<Otherwise: "There is no paper-trading proposal. The primary failed the tests written to decide that question." A different rule or later data is a new study with its own RULES.md.>

## 11. Post hoc (not part of the verdict)

<Each analysis added after seeing results, labelled **(post hoc)**, with what it shows. None of it changes §8.>

### Ideas for a new study

<Variants suggested by this data. Each needs its own RULES.md and data this study did not use. Leave empty rather than pad.>

## 12. Reproduce

From the repo root:

```bash
python research/<slug>/research/backtest.py
```

```bash
python research/<slug>/research/verify.py
```

```bash
python research/<slug>/research/posthoc.py
```

```bash
python research/<slug>/research/charts.py
```

<What each writes; runtime; seeds; "reruns on the same store give identical numbers.">

### References

- <Author (year). Title. Venue.>
