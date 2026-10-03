# Gold/silver ratio

**Status: Inconclusive.** Out of sample, 2024-07-01 through 2026-10-01, the GLD/SLV pair returned −9.5% after 1 bp per side (Sharpe 0.220, profit factor 0.615, 8 round trips). The pre-registered floor is 15 OOS round trips. OOS Sharpe is 0.220 against a bar of 0.5, and OOS profit factor is 0.615 against a bar of 1.10.

- [`report/REPORT.md`](report/REPORT.md): the verdict, the acceptance lines, and the figures in `report/figures/`.
- [`research/RULES.md`](research/RULES.md): rules locked 2026-10-02 21:15:11 UTC, sha256 `abedf66dd1b3`.
- [`research/results.json`](research/results.json): the stored metrics, grid, placebos, and acceptance lines.
- [`research/`](research/): `backtest.py`, `verify.py`, `posthoc.py`, `charts.py`, `counts.py`, and the output files.

Run from the repo root:

```bash
python research/gold-silver-ratio/research/backtest.py
```

```bash
python research/gold-silver-ratio/research/verify.py
```

```bash
python research/gold-silver-ratio/research/posthoc.py
```

```bash
python research/gold-silver-ratio/research/charts.py
```
