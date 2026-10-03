# Country betting against beta

**Inconclusive.** Out of sample the low-minus-high beta book of twelve country ETFs returned −37.9% after 5 bp per side (Sharpe −0.770, profit factor 2.060 on 14 round trips). The pre-registered floor is 24 out-of-sample round trips, so the verdict is Inconclusive. Lines 1, 2, and 4 also failed.

- [report/REPORT.md](report/REPORT.md): verdict, figures, and acceptance table.
- [research/RULES.md](research/RULES.md): rules locked before the first return was computed.
- [research/](research/): code, lock, run log, and raw output.

Reproduce from the repo root:

```bash
python research/country-bab/research/backtest.py
```

```bash
python research/country-bab/research/verify.py
```

```bash
python research/country-bab/research/posthoc.py
```

```bash
python research/country-bab/research/charts.py
```
