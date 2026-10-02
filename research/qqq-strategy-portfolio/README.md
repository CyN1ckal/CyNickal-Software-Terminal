# QQQ plus the studied strategies: max-Sharpe portfolio

**Status: rejected.** Weights fitted to maximize in-sample Sharpe (QQQ 0.20×, P1 1.42×, Turtle 0.38×) returned +37.6% out of sample at a Sharpe of 0.91, against QQQ's 1.01. The portfolio failed 3 of 5 pre-registered tests. The QQQ + P1 secondary reached 1.05 and failed 2 of 5. **(post hoc)** Holding QQQ at 1× with P1 overlaid intraday at 1× reached 1.22 out of sample. It is a benchmark, so it is proposed as a new study, not promoted.

- [`report/REPORT.md`](report/REPORT.md): the final report, with figures in `report/figures/`.
- [`research/`](research/): everything needed to reproduce it.
  - `RULES.md` and `RULES.lock`: rules locked before the first portfolio return was computed.
  - `RUNLOG.md`: one aborted attempt (a data check, before any return was computed), one run, and one verification.
  - `backtest.py`: hash check, self-test, then the pre-registered run. Writes `results.json` and `daily.csv`.
  - `verify.py`: pure-Python replay of the weights and daily returns.
  - `posthoc.py`: overlay frontier, hindsight weights, and the recent window. Writes `posthoc.json`. Does not affect the verdict.
  - `charts.py`: report figures, written to `report/figures/`.

The inputs are the published `daily.csv` and trade files of the five earlier studies in `research/`. No bars are read from the store. Run everything from the repo root:

```bash
python research/qqq-strategy-portfolio/research/backtest.py --reason "reproduce"
```

```bash
python research/qqq-strategy-portfolio/research/verify.py
```

```bash
python research/qqq-strategy-portfolio/research/posthoc.py
```

```bash
python research/qqq-strategy-portfolio/research/charts.py
```
