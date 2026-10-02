# QQQ ATR martingale to breakeven

**Status: paper-trading candidate.** Passed all six pre-registered tests. Not for live capital. Out of sample, from 1 July 2024 through 25 September 2026, the account added +1.05 per 1 of starting capital (Sharpe 1.02, daily profit factor 1.75). Full-sample equity went from 1 to 3.05 (Sharpe 1.15, drawdown −12.6%). All 2,266 closed campaigns netted at least zero. The pre-registered ruin predictions failed: the drawdown was not −50%, and the booked gains were larger than the hole. A neighboring parameter cell reached equity −414.

- Rules, code, and raw output: [research/](research/)
- Report: [report/REPORT.md](report/REPORT.md)

From the repo root:

```bash
python research/qqq-atr-martingale/research/backtest.py
```

```bash
python research/qqq-atr-martingale/research/verify.py
```

```bash
python research/qqq-atr-martingale/research/posthoc.py
```

```bash
python research/qqq-atr-martingale/research/charts.py
```
