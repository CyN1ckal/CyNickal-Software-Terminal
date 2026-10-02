# Opening-gap fade on the small-cap screen

**Paper-trading candidate.** Out of sample, 2024-01-02 through 2026-09-25, shorting a gap of at least 5% at the open and covering at the close multiplied a unit of capital by 22.0 after 20 bp per side (Sharpe 2.60, 165 trades, max drawdown −45%). It passed all 6 pre-registered tests. Not for live capital. The full-sample multiple, 1 to 39,156, is a fully invested single-name compound and is the wrong number to underwrite.

- Report: [report/REPORT.md](report/REPORT.md)
- Rules, code, and raw output: [research/](research/)

From the repo root:

```bash
python research/small-cap-gap-up-fade/research/backtest.py
python research/small-cap-gap-up-fade/research/verify.py
python research/small-cap-gap-up-fade/research/posthoc.py
python research/small-cap-gap-up-fade/research/charts.py
```
