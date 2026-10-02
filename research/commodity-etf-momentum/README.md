# Commodity ETF cross-sectional momentum

**Status: Inconclusive.** Out of sample, 2024-07-01 through 2026-10-01, the book returned +41.5% after costs (Sharpe 0.574, 21 round trips). The pre-registered floor is 24 OOS round trips. The in-sample formation grid is 2 of 5 cells with Sharpe above 0.

- [`report/REPORT.md`](report/REPORT.md): the verdict, the acceptance lines, and the figures in `report/figures/`.
- [`research/RULES.md`](research/RULES.md): rules locked 2026-10-02 19:50:06 UTC, sha256 `0a269fcb4d44`.
- [`research/`](research/): `backtest.py`, `verify.py`, `posthoc.py`, `charts.py`, `counts.py`, and the output files.

Run from the repo root:

```bash
python research/commodity-etf-momentum/research/backtest.py
```
