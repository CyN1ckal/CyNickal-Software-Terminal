# Index opening-pop fade

**Status: Rejected** (2026-09-26). This study shorted SPY at 10:00 when its first 30 minutes rose at least 1.0 trailing RMS unit, and covered at the close. Out of sample it returned −11.96% (Sharpe −0.66, 82 trades). It failed 6 of 7 pre-registered tests and lost at zero cost, on QQQ, and on IGV. The gap-inclusive secondary (S1) failed 5 of 7.

- Rules, code, and raw output: [`research/`](research/). Start with [`RULES.md`](research/RULES.md), which is locked by [`RULES.lock`](research/RULES.lock). Every run is in [`RUNLOG.md`](research/RUNLOG.md).
- Report: [`report/REPORT.md`](report/REPORT.md)

## Reproduce

From the repo root:

```bash
python research/index-opening-pop-fade/research/backtest.py --reason "reproduce"
```

```bash
python research/index-opening-pop-fade/research/verify.py
```

```bash
python research/index-opening-pop-fade/research/posthoc.py
```

```bash
python research/index-opening-pop-fade/research/charts.py
```
