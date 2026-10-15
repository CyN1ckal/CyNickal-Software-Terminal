# SSR next-session long

**Status: Rejected.** Failed 6 of 10 pre-registered tests. Out of sample, 2024-07-01 through 2026-10-01, the equal-weight long of names whose short-sale restriction began the previous session returned −94.1% after 10 bp per side (Sharpe −1.360, profit factor 0.796, 1,362 trades). The average net trade was −16.3 bp. At zero cost the out-of-sample return was −85.9%.

- [`report/REPORT.md`](report/REPORT.md): the verdict, the figures, and the acceptance table.
- [`research/`](research/): rules, code, and raw output.
  - `RULES.md` and `RULES.lock`: locked 2026-10-15T04:51:11+00:00, sha256 `33e478d8cfd4`, before any return.
  - `backtest.py`: self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay of every hold-1 trade.
  - `posthoc.py`: trailing 252-session Sharpe. Labelled post hoc. Does not change the verdict.
  - `hold_sweep.py`: post hoc holds 1 through 31. Writes `hold_sweep.json`. Does not change the verdict.
  - `charts.py`: SVG figures in `report/figures/`.
  - `counts.py` and `health_audit.py`: pre-lock counts and the health pass. No returns.

Run from the repo root:

```bash
python research/ssr-next-day-long/research/backtest.py
```

```bash
python research/ssr-next-day-long/research/verify.py
```

```bash
python research/ssr-next-day-long/research/posthoc.py
```

```bash
python research/ssr-next-day-long/research/charts.py
```

```bash
python research/ssr-next-day-long/research/hold_sweep.py
```
