# Treasury ETF time-series momentum

**Status: Inconclusive.** Out of sample, 2024-07-01 through 2026-10-01, the book returned −10.52% (Sharpe −0.65, profit factor 0.55, 16 round trips). Line 6 required 24 OOS round trips. Lines 1 and 2 also failed.

- [`report/REPORT.md`](report/REPORT.md): the final report, with figures in `report/figures/`.
- [`research/`](research/): everything needed to reproduce it.
  - `RULES.md` and `RULES.lock`: rules locked before the first run.
  - `backtest.py`: the self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay of `trades.csv`.
  - `posthoc.py`: rolling Sharpe, concentration, and the OOS side split. Does not open the store and does not affect the verdict.
  - `charts.py`: report figures, written to `report/figures/`.
  - `counts.py`: pre-lock coverage counts. No P&L.

Data is read through `agent-data/mdq.py`. Run everything from the repo root:

```bash
python research/treasury-etf-trend/research/backtest.py
```

```bash
python research/treasury-etf-trend/research/verify.py
```

```bash
python research/treasury-etf-trend/research/posthoc.py
```

```bash
python research/treasury-etf-trend/research/charts.py
```

A store run appends a line to `RUNLOG.md`. `research/README.md` was not updated; the request forbade that edit.
