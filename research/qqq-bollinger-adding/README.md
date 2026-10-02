# QQQ intraday Bollinger reversion with adding

**Status: rejected.** Failed 5 of 6 pre-registered tests. Fading a 5-minute QQQ close outside a session-local 20-bar, 2-SD Bollinger band, adding up to two more units at each further band SD, exiting at the middle band, and flattening at the close lost 9.90% out of sample, 1 July 2024 through 25 September 2026, after 1 bp per side (Sharpe −0.89, profit factor 0.81, 970 campaigns). Over the full window it lost 35.14% (Sharpe −1.53). At zero cost the full-sample Sharpe was still −0.75. Added legs did worse than first legs, and the same rule lost on SPY and IGV.

- Rules, code, and raw output: [research/](research/)
- Report: [report/REPORT.md](report/REPORT.md)

From the repo root:

```bash
python research/qqq-bollinger-adding/research/backtest.py
```

```bash
python research/qqq-bollinger-adding/research/verify.py
```

```bash
python research/qqq-bollinger-adding/research/posthoc.py
```

```bash
python research/qqq-bollinger-adding/research/charts.py
```
