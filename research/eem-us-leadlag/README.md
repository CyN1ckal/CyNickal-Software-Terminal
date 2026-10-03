# EEM US lead-lag

**Status: Rejected.** Failed 5 of 6 pre-registered tests. Out of sample, 2024-07-01 through 2026-10-01, the book returned −55.6% after 1 bp per side (Sharpe −2.005, profit factor 0.689, 566 trades, max drawdown −60.2%).

- [`report/REPORT.md`](report/REPORT.md): the verdict, the figures, and the acceptance table.
- [`research/`](research/): rules, code, and raw output.
  - `RULES.md` and `RULES.lock`: locked 2026-10-02T21:13:40+00:00, sha256 `fed01671ea16`, before any EEM or EFA return.
  - `backtest.py`: self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay of every EEM trade. Does not import `backtest.py`.
  - `posthoc.py`: rolling Sharpe, concentration, correlations, and the grid rank correlation. Labelled post hoc. Does not change the verdict.
  - `charts.py`: SVG figures in `report/figures/`.
  - `counts.py`: pre-lock coverage and corporate-action counts. No returns.

Run from the repo root:

```bash
python research/eem-us-leadlag/research/backtest.py
```

```bash
python research/eem-us-leadlag/research/verify.py
```

```bash
python research/eem-us-leadlag/research/posthoc.py
```

```bash
python research/eem-us-leadlag/research/charts.py
```
