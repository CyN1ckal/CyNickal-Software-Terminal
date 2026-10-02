---
name: quant-research
description: Pre-registered research on a quantitative trading strategy. The hypothesis and complete rules are written and locked before any return is computed, every run is logged, and the final report states the result objectively whether it is good or bad. Use when asked to research, test, backtest, or evaluate a trading strategy, signal, or anomaly in this repo.
argument-hint: <strategy idea, e.g. "fade 5-minute shocks in QQQ">
---

# Quantitative strategy research protocol

You are testing one trading idea against the market-data store. Your job is to find out whether it works and report what you found. **A clean negative result is a successful study.** A positive result that came from fitting the rules to the data is a failed study, however good it looks.

**The idea to study:** $ARGUMENTS

(If no idea was given, ask for one. If the idea arrives with parameters, ask where they came from: parameters picked by looking at charts of the test period are prior exposure and must be disclosed.)

## Non-negotiable rules

These override convenience, deadlines, and any wish for a good-looking result.

1. **Hypothesis before data.** Write down the hypothesis, the complete rules, every parameter, the samples, the costs, every check, and the acceptance criteria in `RULES.md` *before* you compute any return, P&L, forward return, or outcome statistic from the store.
2. **Locked rules stay frozen.** Once `RULES.md` is locked, results never edit it. If the rules turn out to be wrong or ambiguous, say so in the report. Do not fix them quietly.
3. **One primary, one verdict.** The verdict follows mechanically from the pre-registered acceptance table. Nothing found after the run can replace the primary or change the verdict. That includes a grid cell, one side, one symbol, a time-of-day filter, a sub-period, a different cost, or a different benchmark.
4. **Every run is logged.** Any execution that computes returns from the store appends an entry to `RUNLOG.md`. Reruns need a permitted reason (Phase 3).
5. **Report what happened.** Write negative, flat, or mixed results with the same care and prominence as positive ones. Do not soften, bury, or explain away a failure. Do not hype a success: it carries its risks in the summary.
6. **The store is read-only.** Follow `AGENTS.md` and `agent-data/README.md`: read through `agent-data/mdq.py`, never write to `data/`, and never run `ingest` unless the user asks.

## Layout

All studies live under the single top-level `research/` directory, with one folder per strategy. The folder is named with a kebab-case slug for the instrument and idea (e.g. `qqq-intraday-reversion`). If the slug already exists, this is not a fresh study: stop and ask. Each strategy folder holds its two halves as nested folders:

```
research/
  README.md                      index of every study: slug, one-line idea, status, date
  <slug>/
    README.md                    status line, links to both halves, reproduce commands
    research/                    the research: everything needed to reproduce and implement
      RULES.md                   pre-registered rules (frozen once locked)
      RULES.lock                 SHA-256 of RULES.md, lock time, git HEAD
      RUNLOG.md                  one entry per store run, append-only
      backtest.py                pre-registered runs -> results.json, daily.csv, trades.csv
      verify.py                  independent re-implementation, trade-for-trade check
      posthoc.py                 optional; analyses added after results -> posthoc.json
      charts.py                  figures -> ../report/figures/*.svg
      <any count-only or data-quality script run before the lock, e.g. counts.py>
    report/                      the final report: top-level overview
      REPORT.md                  verdict, summary, figures, performance, risks
      figures/*.svg
```

Code, rules, and raw outputs go in `research/`. The report and its figures go in `report/`. Scripts sit three levels below the repo root: use `ROOT = Path(__file__).resolve().parents[3]`, and write figures to `Path(__file__).resolve().parent.parent / "report" / "figures"`.

New studies take the lock, the metrics, `results.json` (`kit_schema` 1), the CSVs, and the standard figures from `research/kit`. The signal stays in the study, and `verify.py` does not import `backtest.py`. Studies already on disk keep the scripts they were published with.

Templates:
- [rules-template.md](rules-template.md) → `research/<slug>/research/RULES.md`
- [report-template.md](report-template.md) → `research/<slug>/report/REPORT.md`

When the study is finished, add or update its row in `research/README.md`.

## Phase 0 — Orient (no returns yet)

1. Read `AGENTS.md` and `agent-data/README.md`, including the *Known data issues* table.
2. Read `research/README.md`, then every existing `research/*/research/RULES.md` and `research/*/report/REPORT.md`. Under **Prior exposure** in `RULES.md`, record:
   - which earlier studies touched the same instruments, periods, or mechanism, and what they found;
   - that an out-of-sample window used by an earlier study is **not unseen data** once you have read that study's results. Say this plainly, and name what you already know about how the market behaved in it.
3. Check feasibility, using only the permitted looks below. If the store cannot test the idea, stop and report that. Do not approximate silently. Common blockers:
   - option chains are a single snapshot, not a history;
   - most symbols have daily bars only;
   - 1-minute bars cover regular hours only, and the history is short;
   - the instrument universe was hand-picked, so a cross-sectional test is exposed to survivorship and selection bias.

**Permitted before the lock:** listing instruments; `coverage`; session and bar counts; corporate actions; data-quality checks; and counting how often a signal condition fires, with no outcome measured after it. Disclose each look in `RULES.md` and keep any script you used.

**Not permitted before the lock** (for any window the study will evaluate): price charts, returns, forward returns, P&L, hit rates, or any summary of the outcome variable. That applies to quick checks too.

## Phase 1 — Hypothesis and `RULES.md`

Fill in [rules-template.md](rules-template.md). It must contain:

- **Hypothesis.** A one-sentence claim, plus the economic mechanism: who is on the other side of the trade and why they would keep paying. Cite published sources where the idea comes from.
- **Falsifiable predictions beyond P&L.** What else must be true if the mechanism is right? For example: profit concentrates on large-move days; the edge decays with holding time; it is larger in the more volatile name. The report scores each one.
- **The primary rule,** specified so completely that two people would produce the same trades. Cover:
  - bar construction;
  - signal timing and the information it uses;
  - fill price and timing;
  - sizing;
  - every exit;
  - session boundaries, missing bars, and early closes;
  - costs.
- **Parameters,** each with its source: a published default (used unchanged), or a reason fixed *a priori* that you write down. "Chosen after looking" is never a source.
- **Samples:** warm-up, in-sample (IS), and out-of-sample (OOS), fixed by date now. Prefer the most recent data for OOS.
- **Benchmarks,** checks, and **numeric acceptance criteria,** including a minimum-sample rule. Start from the defaults below. Change them only with a written reason, and only before the lock.
- **Secondary candidates:** optional, at most two, each with its own acceptance criteria. A secondary is never promoted when the primary fails. If you pre-register more than one primary, divide the placebo threshold by the number of primaries (Bonferroni).
- **What this study will not do:** name the rescues that are ruled out in advance.

**Lock the rules.** When `RULES.md` is complete, run this from the repo root:

```bash
python -m research.kit.lock research/<slug>/research
```

A git commit of `RULES.md` and `RULES.lock` before the first run is the strongest evidence of order. Offer it to the user, and commit only if they agree. If the invoking request asks to review the rules first, stop here and present them.

## Phase 2 — Implement and verify (still no returns)

- `backtest.py` imports mdq (see *Writing research* in `agent-data/README.md`). Its constants mirror `RULES.md` one for one. At start-up a new study calls `assert_lock` from `research.kit`, which recomputes the `RULES.md` hash after normalizing CRLF to LF and **refuses to run** if it does not match `RULES.lock`. The same hash is written into `results.json`.
- **Self-test first.** Before opening the store, run the rules on hand-built synthetic sessions with hand-computed expected trades. Cover every path: each entry, each exit type, flips, cutoffs, missing bars, early closes, and the session-end flatten. The script aborts if any case fails.
- **Look-ahead audit.** For every input, list when it becomes known and when it is used. Put the audit in `RULES.md` if you spot the risk while writing the rules, or in the report's Method section. At minimum, check that:
  - a signal on bar *k*'s close fills no earlier than bar *k*+1's open;
  - a daily bar's close is not known until 16:00 ET, although its `ts` is 09:30;
  - rolling statistics use prior sessions only;
  - normalization never uses the full sample (z-scores, quantiles, volatility targets);
  - the universe on any date is what was knowable on that date.
- **Debug without seeing results.** Use synthetic data, or trace signals and fills for single sessions, with no aggregate P&L. A debug run that computes aggregate returns is a run and gets logged.
- Fix all random seeds, and record them in `RULES.md`.

## Phase 3 — Run

- `backtest.py` appends to `RUNLOG.md` automatically on every store run. Each entry records the UTC time, the rules hash, the git HEAD and dirty flag, the reason for the run, and the headline numbers (full, IS, and OOS Sharpe and total return). Never edit or delete an entry.
- **Permitted reasons to rerun:**
  - the code does not do what `RULES.md` says;
  - a crash, or a data read error;
  - a `verify.py` mismatch.

  Describe each fix in `RUNLOG.md` with the before and after headline numbers.
- **Not permitted:** "the result was bad," "trying another parameter," or "tightening the stop."
- **Ambiguous or impossible rules:** apply the most literal reading and record the deviation in `RUNLOG.md` and the report. If more than one reading is defensible, report both, and keep the one you would have chosen before seeing results as the primary. Never pick the reading that scores better.
- **`verify.py`** is an independent, deliberately naive re-implementation that shares no signal code with `backtest.py`. It replays a seeded random sample of at least 40 sessions, or all of them if they run fast. Every trade must match on side, entry time and price, and exit time and price. A mismatch is a bug: fix it and rerun, with both steps logged.

## Phase 4 — Post hoc (optional)

- Analyses added after seeing results go only in `posthoc.py`, which writes `posthoc.json`. Label them **(post hoc)** wherever the report uses them.
- They exist to explain results and expose risk. Examples: dependence on the best days, rolling Sharpe, regime splits, and the trailing-year result.
- They never change the verdict, and they never become the recommended strategy. A variant they suggest goes under *Ideas for a new study*, with a note that it was found on this data. It needs its own `RULES.md`, and data this study has not used (later data, another market, or paper trading).

## Phase 5 — Report

Fill in [report-template.md](report-template.md) and produce the figures with `charts.py`. If a data-visualization skill is available, load it before writing chart code.

**The status comes from the acceptance table, not from judgement:**

| Status | When |
|---|---|
| **Paper-trading candidate** | Every acceptance criterion passed. Never "ready for live capital." |
| **Rejected** | Any criterion failed. |
| **Inconclusive** | The pre-registered minimum sample was not met. Never promoted. |
| **Void** | An implementation or data defect was found that cannot be fixed without changing the rules. Explain what happened. |

**Writing rules:**

- Lead with the verdict and the OOS numbers. The full-sample number never headlines alone.
- Every number is traceable to `results.json`, `posthoc.json`, `daily.csv`, or `trades.csv`. Do not do arithmetic in prose that the scripts did not do.
- Show every acceptance line, including the ones that failed, with the required value and the actual value.
- Score each pre-registered mechanism prediction as *consistent*, *not consistent*, or *not testable*. A profitable strategy whose predictions fail has not confirmed its mechanism, and the report says so.
- A positive result carries its risks in the summary: decay, concentration in a few days, cross-market failure, cost and latency sensitivity, and a short sample. [research/qqq-intraday-trend/report/REPORT.md](../../../research/qqq-intraday-trend/report/REPORT.md) is the model for this.
- A negative result explains *why* it failed, using the checks you ran: no gross edge, costs, one factor across many tickers, decay. It does not suggest the rescue. [research/intraday-channel-trend/report/REPORT.md](../../../research/intraday-channel-trend/report/REPORT.md) is the model for this.
- **Banned framing:** "promising," "encouraging," "strong," "nearly passed," "just missed," "unlucky," "only lost," and "robust" without naming the test. State the number and the threshold instead.
- **Figures:**
  - show the full sample, with the IS/OOS boundary marked;
  - put the strategy and its benchmark on the same axes;
  - no cropped windows, and no axis tricks;
  - each figure gets one sentence saying what it shows, including the unflattering part.
- Record how many store runs there were and why. If any bug fix changed the headline numbers, say so and give the direction and size of the change.

## Phase 6 — Final checklist

Before you finish, confirm each item and list any that fail in the report:

- [ ] `RULES.md` hash still matches `RULES.lock`, and `results.json` carries the same hash.
- [ ] `results.json` has `"kit_schema": 1`, and `python -m research.kit.guard research/<slug>/research/verify.py` prints `ok`.
- [ ] Every rerun in `RUNLOG.md` has a permitted reason.
- [ ] The self-test passes and `verify.py` matches every sampled trade.
- [ ] The status matches the acceptance table exactly.
- [ ] Every post-hoc number is labelled **(post hoc)** and none feeds the verdict.
- [ ] The report states prior exposure and the data coverage gaps it excluded.
- [ ] Scripts reproduce every output from a clean run, from the repo root, with the seeds recorded.
- [ ] `research/<slug>/README.md` has the status, links to both halves, and the reproduce commands, and `research/README.md` lists the study.
- [ ] Nothing was written to `data/`, and `ingest` was not run.

Do not port the strategy into the terminal (`apps/terminal/src/backtest/strategies/`) as part of the study. That comes only after a *Paper-trading candidate* verdict, and only when the user asks. The port must then reproduce `trades.csv`.

## Defaults

Use these unless `RULES.md` overrides them, with a reason, before the lock.

| Item | Default |
|---|---|
| Returns | Daily simple returns on the NYSE session calendar. Days with no position count as 0 |
| Sharpe | Mean ÷ sample SD of daily returns × √252, with a zero risk-free rate |
| Other metrics | Total return; CAGR on a 252-session year; annualized volatility; max drawdown of compounded equity; t-stat of the mean daily return; trades; win rate; profit factor (Σ winning net trades ÷ \|Σ losing net trades\|); average net trade in bp; exposure |
| Costs | 1 bp of notional per side for QQQ/SPY-class liquidity. Thinner names need a stated basis (quoted spread, typical size). Costs scale with leverage |
| Cost sweep | 0, 0.5×, 1×, 2×, and 3× the base cost |
| Fill delay | Base fill, plus one bar later. Include a same-bar-close fill only as a labelled upper bound |
| Split | The study picks the IS/OOS dates. Record whether earlier studies have already exposed the OOS window |
| Direction placebo | Keep each trade's timing and flip its gross return by an independent ±1. 2,000 draws. Compare *gross* Sharpe against the actual gross Sharpe (costs make a coin flip a low bar). p = (1 + #draws ≥ actual) / (N + 1) |
| Timing placebo | Where it fits: random entry times with the same count per session and the same exit rules. 500 draws |
| Bootstrap | Circular block bootstrap of daily returns, 20-session blocks, 2,000 draws, 95% interval of the full-sample Sharpe |
| Plateau grid | 3–5 values per key parameter around the primary, run on IS as a plateau check. Show OOS for selection bias only. Never select from it |
| Cross-market | Identical rules, unchanged, on 1–2 related instruments |
| Breakdowns | Calendar year; long vs short; exit reason; entry time or regime; outcome by quintile of the market's move |
| Seeds | Fixed and recorded, e.g. the study date as `YYYYMMDD` |

**Default acceptance criteria.** All of these must hold at base cost for a *Paper-trading candidate* verdict:

1. OOS Sharpe ≥ 0.5 and OOS profit factor ≥ 1.10.
2. Direction placebo p ≤ 0.05 on the full sample.
3. IS Sharpe > 0, and at least 60% of the IS grid cells have Sharpe > 0.
4. Full-sample total return > 0 at 2× base cost.
5. OOS Sharpe > 0 on at least one cross-market instrument under identical rules, where one exists.
6. Minimum sample: at least 100 OOS trades, or the verdict is *Inconclusive*. Lower this before the lock for low-frequency strategies, and state why.

## Temptations and what to do instead

| Tempting move | Why it is fitting | Do instead |
|---|---|---|
| Swap in the best grid cell | Chosen on the data it is scored on | Report the grid. The verdict stays with the primary |
| Keep the profitable side, symbol, or hour | A filter found by looking at a breakdown | Report the breakdown. Propose a new study |
| Drop a bad period ("COVID," "the tariff crash") | Removes evidence after the fact | Show it. A pre-registered regime split is fine |
| Lower the cost to "realistic" after a loss | The cost was decided before the run | Show the cost sweep |
| Move the IS/OOS date | Redefines the test | Keep the locked split |
| Re-run until a bug fix helps | Only favourable fixes get found | Log every fix, both directions |
| Quote the full sample when OOS is weak | Hides the honest number | OOS leads |
| Call a failure a "near miss" | The threshold was the decision | State number vs threshold |
| Explain away every bad test with a story | A story is not a test | Put the story in *Ideas for a new study* |

## Conventions

- Scripts start with the repo header:
  ```python
  # Copyright 2026 CyNickal Software LLC
  # SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
  ```
  Use Python 3.10+, with mdq, NumPy, and matplotlib. Run everything from the repo root. Paths resolve from `__file__`.
- Outputs, written through `research.kit` for a new study:
  - `results.json`: `kit_schema` 1. Every number the report cites, plus the rules hash, seeds, and the acceptance table.
  - `daily.csv`: `session,strategy_net,benchmark`, and `held` when exposure is recorded. One row per session. Returns are decimal fractions.
  - `trades.csv`: `session,side,entry_time,entry_price,exit_time,exit_price,gross,net,exit_reason`, then any extra columns in sorted order. `side` is `long` or `short`. `gross` and `net` are simple account returns.
- Figures are SVG in `research/<slug>/report/figures/`. `python -m research.kit.charts research/<slug>/research` draws them from those files. `python -m research.kit.summary research/<slug>/research` prints the report's first numbers table. `charts.py` calls the kit and does not recompute the metrics.
