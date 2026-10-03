# QQQ-holdings 150/250 moving-average bounce

**Status: Paper-trading candidate** (2026-10-03). Passed all 6 pre-registered tests. Out of sample: total return 0.6545142383828009, Sharpe 1.0171774262432778, profit factor 1.4422280396580611, 476 trades. Full sample: total return 1.4550571919689181, Sharpe 1.1060682655093068, 734 trades. Prediction 1 (time-stop trades worse than trend-break trades) is not consistent. The timing placebo p is 0.7245508982035929. Not for live capital.

Long a current QQQ equity holding when the low tags the band between the 150-session and 250-session averages and the close reclaims the faster average, with the close above the slower average and the faster average above the slower one. Enter at the next open. Exit at the next open on a close under the 250-session average, or after 20 evaluated sessions. Equal weight. 5 bp per side. The universe is the 101 equity holdings of Invesco QQQ, CUSIP 46090E103, business date 2026-10-02. Membership is look-ahead.

- **Report:** [report/REPORT.md](report/REPORT.md)
- **Research:** [research/](research/): [RULES.md](research/RULES.md) (locked, sha256 `a3303e9e97f3`), [RUNLOG.md](research/RUNLOG.md), `counts.py`, `backtest.py`, `verify.py`, `posthoc.py`, `charts.py`, and their outputs

## Reproduce

From the repo root. One store run is already logged. Running `backtest.py` again appends another `RUNLOG.md` entry.

```bash
python research/qqq-holdings-ma-bounce/research/counts.py
```

```bash
python -m research.kit lock research/qqq-holdings-ma-bounce/research
```

```bash
python research/qqq-holdings-ma-bounce/research/backtest.py
```

```bash
python -m research.kit guard research/qqq-holdings-ma-bounce/research/verify.py
```

```bash
python research/qqq-holdings-ma-bounce/research/verify.py
```

```bash
python research/qqq-holdings-ma-bounce/research/posthoc.py
```

```bash
python research/qqq-holdings-ma-bounce/research/charts.py
```

```bash
python -m research.kit summary research/qqq-holdings-ma-bounce/research
```

`research/README.md` was left unchanged on instruction.
