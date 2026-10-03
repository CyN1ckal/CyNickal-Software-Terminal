# QQQ holdings relative strength

**Status: Paper-trading candidate.** Out of sample, 2024-07-01 through 2026-10-02, the long top quintile returned +265.7% after 5 bp (Sharpe 1.52, profit factor 2.41, 520 trades). Full sample 2022-10-03 through 2026-10-02 returned +622.9% (Sharpe 1.53). All 6 pre-registered lines passed. The universe is the 2026-10-02 QQQ constituent file, so this ticker list is not a paper book. The timing placebo p-value is 0.417.

- [`report/REPORT.md`](report/REPORT.md): the verdict, the acceptance lines, and the figures in `report/figures/`.
- [`research/RULES.md`](research/RULES.md): rules locked 2026-10-03 15:26 UTC, sha256 `40e29bd16bef`.
- [`research/RULES.lock`](research/RULES.lock): the hash, the lock time, and the git head.
- [`research/`](research/): `backtest.py`, `verify.py`, `posthoc.py`, `charts.py`, `counts.py`, and the output files.

Run from the repo root:

```bash
python research/qqq-holdings-relative-strength/research/backtest.py
python research/qqq-holdings-relative-strength/research/verify.py
python research/qqq-holdings-relative-strength/research/posthoc.py
python research/qqq-holdings-relative-strength/research/charts.py
python -m research.kit summary research/qqq-holdings-relative-strength/research
python -m research.kit guard research/qqq-holdings-relative-strength/research/verify.py
```

Copyright 2026 CyNickal Software LLC, SPDX PolyForm-Noncommercial-1.0.0.
