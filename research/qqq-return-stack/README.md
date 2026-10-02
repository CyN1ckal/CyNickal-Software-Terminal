# QQQ core with the studied strategies stacked on top

**Status: rejected.** QQQ held at 1× with the seven in-sample-positive strategies stacked on top, each at 5% in-sample volatility, returned +176.0% out of sample at a Sharpe of 1.70 (max drawdown −21.7%), against QQQ's +55.4% and 1.01 (−22.9%). It passed 5 of 6 pre-registered tests and failed the 2× cost line (OOS Sharpe 0.87 vs 1.01). The secondary K (QQQ + the four paper-trading candidates) passed 6 of 6. It is not promotable: a secondary never replaces a failed primary, and its members were chosen with out-of-sample knowledge.

- [`report/REPORT.md`](report/REPORT.md): the final report. The figures in `report/figures/` show the impact of each strategy: add-one and leave-one-out Sharpe, cumulative and yearly contribution, correlations, crash-day behaviour, cost and budget sweeps, and overnight leverage.
- [`research/`](research/): everything needed to reproduce it.
  - `RULES.md` and `RULES.lock`: rules locked before the first portfolio return was computed.
  - `counts.py`: the pre-lock alignment check (session and blank counts only).
  - `RUNLOG.md`: one cost-formula deviation logged before the run, one run, and one verification.
  - `backtest.py`: hash check, self-test, then the pre-registered run. Writes `results.json` and `daily.csv`.
  - `verify.py`: pure-Python replay of every scale and daily return.
  - `posthoc.py`: best-day dependence, rolling Sharpe, the recent window, overnight exposure, yearly contribution, and cost drag. Writes `posthoc.json`. Does not affect the verdict.
  - `charts.py`: report figures, written to `report/figures/`.

The inputs are the published `daily.csv` and trade files of twelve earlier studies in `research/`. No bars are read from the store. Run everything from the repo root:

```bash
python research/qqq-return-stack/research/backtest.py --reason "reproduce"
```

```bash
python research/qqq-return-stack/research/verify.py
```

```bash
python research/qqq-return-stack/research/posthoc.py
```

```bash
python research/qqq-return-stack/research/charts.py
```
