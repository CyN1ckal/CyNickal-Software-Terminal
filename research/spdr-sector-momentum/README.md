# SPDR sector momentum

**Rejected.** Out of sample the 12-1 long/short book of SPDR sector ETFs returned +6.2% after 1 bp per side (Sharpe 0.253, profit factor 0.935). It failed the pre-registered Sharpe, profit-factor, and direction-placebo tests.

- [report/REPORT.md](report/REPORT.md): verdict, figures, and acceptance table.
- [research/RULES.md](research/RULES.md): rules locked before the first return was computed.
- [research/](research/): code, lock, run log, and raw output.

Reproduce from the repo root:

```bash
python research/spdr-sector-momentum/research/backtest.py
```

```bash
python research/spdr-sector-momentum/research/verify.py
```

```bash
python research/spdr-sector-momentum/research/posthoc.py
```

```bash
python research/spdr-sector-momentum/research/charts.py
```
