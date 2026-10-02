# FX ETF time-series momentum

**Status: Rejected.** Failed 4 of 5 scored tests. Out of sample, 2024-07-01 through 2026-10-01, the book returned +6.36% after 5 bp per side (Sharpe 0.4908, 40 round trips, profit factor 3.026). The full sample returned −11.65% (Sharpe −0.124).

- [`report/REPORT.md`](report/REPORT.md): the verdict, the figures, and the acceptance table.
- [`research/RULES.md`](research/RULES.md): rules locked 2026-10-02 before any return. Hash in [`research/RULES.lock`](research/RULES.lock).
- [`research/`](research/): code and raw output.
  - `backtest.py`: self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay. Does not import `backtest.py`.
  - `posthoc.py`: rolling Sharpe, trailing year, and concentration. Does not open the store.
  - `charts.py`: SVG figures in `report/figures/`, from the output files.
  - `counts.py`: pre-lock coverage and corporate-action checks. No P&L.

Data is read through `agent-data/mdq.py`. Run from the repo root:

```bash
python research/fx-etf-momentum/research/backtest.py
```

```bash
python research/fx-etf-momentum/research/verify.py
```

```bash
python research/fx-etf-momentum/research/posthoc.py
```

```bash
python research/fx-etf-momentum/research/charts.py
```

A second `backtest.py` run exits immediately if `results.json` is already present. Pass `--reason` only for a permitted rerun (the code does not match the locked rules, a crash or a bad read, or a verify mismatch). `verify.py` appends a line to `RUNLOG.md`.
