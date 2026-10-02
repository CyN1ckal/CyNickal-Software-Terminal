# IGV small-account minute fade

**Status: rejected.** Failed 4 of 6 pre-registered tests. Out of sample the account returned −2.96% (Sharpe −3.82) after 2 bp per side.

- [`report/REPORT.md`](report/REPORT.md): the final report, with figures in `report/figures/`.
- [`research/`](research/): everything needed to reproduce it.
  - `RULES.md` and `RULES.lock`: rules locked before the first run.
  - `backtest.py`: the self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`, and appends `RUNLOG.md`.
  - `verify.py`: separate replay of `trades.csv`.
  - `posthoc.py`: rolling Sharpe and concentration. Does not open the store and does not affect the verdict.
  - `charts.py`: report figures, written to `report/figures/`.
  - `counts.py`: pre-lock dollar-volume percentiles and signal counts. No P&L.

Data is read through `agent-data/mdq.py`. Run everything from the repo root:

```bash
python research/igv-small-account-fade/research/backtest.py
```

```bash
python research/igv-small-account-fade/research/verify.py
```

```bash
python research/igv-small-account-fade/research/posthoc.py
```

```bash
python research/igv-small-account-fade/research/charts.py
```

`python research/igv-small-account-fade/research/backtest.py self-test` stops after the synthetic cases and does not open the store. A store run appends a line to `RUNLOG.md`.
