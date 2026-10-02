# QQQ intraday trend following

**Status: paper-trading candidate.** Passed all six pre-registered acceptance tests. OOS Sharpe 0.82 after 1 bp per side.

- [`report/REPORT.md`](report/REPORT.md): the final report, with figures in `report/figures/`.
- [`research/`](research/): everything needed to reproduce and implement it.
  - `RULES.md`: rules, checks, and acceptance criteria, written before the first run.
  - `backtest.py`: pre-registered runs. Writes `results.json`, `daily.csv`, and `trades_p1.csv`.
  - `posthoc.py`: checks added after seeing results. Writes `posthoc.json`.
  - `charts.py`: report figures, written to `report/figures/`.

This study predates the lock and run-log steps of the research protocol, so it has no `RULES.lock` or `RUNLOG.md`.

Data is read through `agent-data/mdq.py`. Run everything from the repo root:

```bash
python research/qqq-intraday-trend/research/backtest.py
```

```bash
python research/qqq-intraday-trend/research/posthoc.py
```

```bash
python research/qqq-intraday-trend/research/charts.py
```
