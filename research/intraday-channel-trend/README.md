# Intraday channel trend

**Status: rejected.** Failed three of the five pre-registered acceptance tests. The OOS book returned −12.5% (Sharpe −0.56) after 1 bp per side.

- [`report/REPORT.md`](report/REPORT.md): the final report, with figures in `report/figures/`.
- [`research/`](research/): everything needed to reproduce it.
  - `RULES.md`: rules, checks, and acceptance criteria, written before the first run.
  - `backtest.py`: the self-test, then the pre-registered run. Writes `summary.json`, `daily.csv`, `trades.csv`, and a quick-look `equity.svg`.
  - `charts.py`: report figures, written to `report/figures/`.

This study predates the lock and run-log steps of the research protocol, so it has no `RULES.lock` or `RUNLOG.md`.

Data is read through `agent-data/mdq.py`. Run everything from the repo root:

```bash
python research/intraday-channel-trend/research/backtest.py
```

```bash
python research/intraday-channel-trend/research/charts.py
```
