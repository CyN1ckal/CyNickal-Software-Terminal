# Micro-futures trend following ($100k account)

**Status: Rejected** (2026-09-26). It failed all 7 performance lines of the pre-registered acceptance table. Out of sample (2024-10 → 2026-09) the book returned −7.8% at a Sharpe of −0.19; over the full evaluation window (2022-10 → 2026-09) it returned −21.5% at −0.33. It lost money before costs, with whole or fractional contracts.

- Report: [report/REPORT.md](report/REPORT.md)
- Rules (locked before any return was computed): [research/RULES.md](research/RULES.md), [research/RULES.lock](research/RULES.lock)
- Run log: [research/RUNLOG.md](research/RUNLOG.md)

## Data

The store cannot hold futures, so `research/fetch.py` downloads MBoum continuous front-month daily bars into `research/data/raw/`. That uses the paid API, about one request per symbol, and reads the key from `secrets.json`. The raw files used for the report are already there. `fetch.py` skips any symbol whose file exists, so rerunning it costs nothing unless a file is deleted. Nothing is written to `data/`.

## Reproduce

Run these from the repo root, in this order:

```bash
python research/micro-futures-trend/research/dq.py
```

```bash
python research/micro-futures-trend/research/rollcheck.py
```

```bash
python research/micro-futures-trend/research/backtest.py --reason "reproduce"
```

```bash
python research/micro-futures-trend/research/verify.py
```

```bash
python research/micro-futures-trend/research/posthoc.py
```

```bash
python research/micro-futures-trend/research/charts.py
```

- `dq.py` and `rollcheck.py` are the pre-lock data-quality checks. They compute counts only.
- `backtest.py` checks the rules hash, runs the self-test, and writes `results.json`, `daily.csv`, `trades.csv`, and `placebo_null.npy`. Each run appends to `RUNLOG.md`.
- `verify.py` is an independent re-implementation that has to match every trade and every daily P&L.
- `posthoc.py` writes `posthoc.json`. Everything in it is labelled post hoc.
- `charts.py` writes `report/figures/*.svg`.

The seed is 20260926. The scripts need Python 3.10+, NumPy, and matplotlib.
