# Weekly quintile reversal on the low-liquidity screen

**Rejected.** Out of sample, 2024-01-02 through 2026-09-25, the book returned −56.6% after 20 bp per side and a 5% borrow (Sharpe −1.13, 521 trades). It failed 5 of 6 pre-registered tests.

- Report: [report/REPORT.md](report/REPORT.md)
- Rules, code, and raw output: [research/](research/)

From the repo root:

```bash
python research/low-liq-high-vol-mean-reversion/research/backtest.py
python research/low-liq-high-vol-mean-reversion/research/verify.py
python research/low-liq-high-vol-mean-reversion/research/posthoc.py
python research/low-liq-high-vol-mean-reversion/research/charts.py
```
