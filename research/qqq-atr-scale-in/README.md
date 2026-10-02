# QQQ ATR scale-in

**Status: rejected.** Failed 5 of 6 pre-registered tests. Out of sample, from 1 July 2024 through 25 September 2026, the book returned −8.81% after 1 bp per side (Sharpe −0.57, profit factor 0.78, 343 campaigns). The full-sample win rate was 55.2% and the skewness of per-unit campaign returns was −2.23. The account still lost 25.51% over the full window.

- Rules, code, and raw output: [research/](research/)
- Report: [report/REPORT.md](report/REPORT.md)

From the repo root:

```bash
python research/qqq-atr-scale-in/research/backtest.py
```

```bash
python research/qqq-atr-scale-in/research/verify.py
```

```bash
python research/qqq-atr-scale-in/research/posthoc.py
```

```bash
python research/qqq-atr-scale-in/research/charts.py
```
