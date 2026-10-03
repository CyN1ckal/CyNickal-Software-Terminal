# SPY pre-holiday

**Status: Rejected.** Failed 2 of 6 pre-registered tests. Out of sample, 2024-07-01 through 2026-10-01, the book returned +0.45% after 1 bp per side (Sharpe 0.122, profit factor 1.090, 23 trades, max drawdown −2.11%).

- [`report/REPORT.md`](report/REPORT.md): the verdict, the figures, and the acceptance table.
- [`research/`](research/): rules, code, and raw output.
  - `RULES.md` and `RULES.lock`: locked 2026-10-02T21:12:37+00:00, sha256 `04497efa4c9f`, before any return.
  - `backtest.py`: self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay of every SPY pre-holiday trade.
  - `posthoc.py`: concentration, rolling Sharpe, trailing year, and holiday labels. Labelled post hoc. Does not change the verdict.
  - `charts.py`: SVG figures in `report/figures/`.
  - `counts.py`: pre-lock coverage and calendar counts. No prices.

Run from the repo root:

```bash
python research/spy-pre-holiday/research/backtest.py
```

```bash
python research/spy-pre-holiday/research/verify.py
```

```bash
python research/spy-pre-holiday/research/posthoc.py
```

```bash
python research/spy-pre-holiday/research/charts.py
```
