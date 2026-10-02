# QQQ 15-minute Turtle breakout, held overnight

**Status: rejected** (2026-09-26). Failed 3 of 6 pre-registered criteria. Out of sample (2024-07-01 → 2026-09-25) it returned +0.49% after 1 bp per side (Sharpe 0.09, profit factor 1.02), while QQQ rose 55%. The overnight gaps it held lost money on both sides, and SPY lost under identical rules.

- [`report/REPORT.md`](report/REPORT.md): the final report, with figures in `report/figures/`.
- [`research/`](research/): everything needed to reproduce it.
  - `RULES.md` and `RULES.lock`: rules locked before the first run.
  - `RUNLOG.md`: one store run and one verification replay.
  - `backtest.py`: hash check, synthetic self-test, then the pre-registered run. Writes `results.json`, `daily.csv`, `trades.csv`.
  - `verify.py`: independent replay; matched all 500 QQQ, 522 SPY, and 477 IGV trades.
  - `posthoc.py`: rolling Sharpe, concentration, grid selection bias. Does not open the store and does not affect the verdict.
  - `charts.py`: report figures, written to `report/figures/`.
  - `counts.py`: pre-lock bar counts and breakout-onset counts. No P&L.

Data is read through `agent-data/mdq.py`. Run everything from the repo root:

```bash
python research/qqq-15m-turtle-overnight/research/backtest.py --reason "reproduce"
```

```bash
python research/qqq-15m-turtle-overnight/research/verify.py
```

```bash
python research/qqq-15m-turtle-overnight/research/posthoc.py
```

```bash
python research/qqq-15m-turtle-overnight/research/charts.py
```

`python research/qqq-15m-turtle-overnight/research/backtest.py self-test` stops after the synthetic cases and does not open the store. A store run appends an entry to `RUNLOG.md`. Seeds: 20260926 (placebo), 20260927 (bootstrap).
