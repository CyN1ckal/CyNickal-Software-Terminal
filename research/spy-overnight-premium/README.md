# SPY overnight premium

**Status: Rejected.** Failed 2 of 6 pre-registered tests. Out of sample, 2024-07-01 through 2026-10-01, the book returned +0.27% after 1 bp per side (Sharpe 0.053, profit factor 1.003, 566 trades, max drawdown −11.4%).

- [`report/REPORT.md`](report/REPORT.md): the verdict, the figures, and the acceptance table.
- [`research/`](research/): rules, code, and raw output.
  - `RULES.md` and `RULES.lock`: locked 2026-10-02T19:47:53+00:00, sha256 `14f65d00f766`, before any return.
  - `backtest.py`: self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay of every SPY night.
  - `posthoc.py`: concentration, rolling Sharpe, and the daily-versus-minute price count. Labelled post hoc. Does not change the verdict.
  - `charts.py`: SVG figures in `report/figures/`.
  - `counts.py`: pre-lock coverage and corporate-action counts. No prices.

Run from the repo root:

```bash
python research/spy-overnight-premium/research/backtest.py
```

```bash
python research/spy-overnight-premium/research/verify.py
```

```bash
python research/spy-overnight-premium/research/posthoc.py
```

```bash
python research/spy-overnight-premium/research/charts.py
```
