# VIXY variance premium

**Status: Void.** Out of sample, 2024-07-01 through 2026-10-01, the short returned 0 (Sharpe undefined, 0 round trips, 566 sessions). The stored adjusted VIXY series still jumps on 2013-06-10, ProShares' 1-for-5 reverse split, and the account froze that day. The result is not a test of the variance premium.

- [`report/REPORT.md`](report/REPORT.md): the final report, with figures in `report/figures/`.
- [`research/`](research/): everything needed to reproduce it.
  - `RULES.md` and `RULES.lock`: rules locked before the first run. The hash was not regenerated.
  - `backtest.py`: the self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay of `trades.csv`. Writes `jumps.json` and appends `RUNLOG.md`.
  - `posthoc.py`: rolling Sharpe and concentration. Does not open the store and does not affect the verdict.
  - `charts.py`: report figures, written to `report/figures/`.
  - `counts.py`: pre-lock coverage counts. No P&L.

Data is read through `agent-data/mdq.py`. Run everything from the repo root:

```bash
python research/vixy-variance-premium/research/backtest.py
```

```bash
python research/vixy-variance-premium/research/verify.py
```

```bash
python research/vixy-variance-premium/research/posthoc.py
```

```bash
python research/vixy-variance-premium/research/charts.py
```

A store run appends a line to `RUNLOG.md`. `research/README.md` was not updated. `results.json` keeps the engine status `Inconclusive` from the line-6 rule. The study status is Void.
