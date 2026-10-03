# QQQ holdings OBV divergence

**Status: Rejected.** Failed 5 of 6 pre-registered tests. Out of sample, 2024-07-01 through 2026-10-02, the book returned −11.3% after 5 bp per side (Sharpe −0.22, profit factor 0.96, 520 trades).

- [`report/REPORT.md`](report/REPORT.md): the verdict, the figures, and the acceptance table.
- [`research/`](research/): rules, code, and raw output.
  - `RULES.md` and `RULES.lock`: locked 2026-10-03T15:31:05+00:00, sha256 `9bd635f6d06f`, before any return.
  - `backtest.py`: self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay of every trade. Does not import `backtest.py`. Matched 914 trades.
  - `posthoc.py`: trailing 126-session Sharpe and counts from the saved grid and trades. Labelled post hoc. Does not change the verdict.
  - `charts.py`: SVG figures in `report/figures/`.
  - `counts.py`: pre-lock coverage counts. No returns.

`research/README.md` was not updated. The request for this study forbade that edit.

Run from the repo root:

```bash
python research/qqq-holdings-obv-divergence/research/backtest.py
```

```bash
python research/qqq-holdings-obv-divergence/research/verify.py
```

```bash
python research/qqq-holdings-obv-divergence/research/posthoc.py
```

```bash
python research/qqq-holdings-obv-divergence/research/charts.py
```
