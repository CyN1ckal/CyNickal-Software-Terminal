# qqq-atr-band-dip-eod

**Status: Rejected** (2026-09-30). Buy QQQ at the session open minus 1× prior-day Wilder ATR(14) (limit touch), hold to the close. Failed 4 of 6 pre-registered acceptance tests; negative gross of costs (full Sharpe −0.46, OOS −0.40).

- Report: [report/REPORT.md](report/REPORT.md)
- Rules (locked before the first run): [research/RULES.md](research/RULES.md) · [RULES.lock](research/RULES.lock) · [RUNLOG](research/RUNLOG.md)

Reproduce from the repo root:

```bash
python research/qqq-atr-band-dip-eod/research/backtest.py   # results.json, daily.csv, trades.csv, RUNLOG entry
python research/qqq-atr-band-dip-eod/research/verify.py     # independent replay, all sessions
python research/qqq-atr-band-dip-eod/research/posthoc.py    # posthoc.json (no store access)
python research/qqq-atr-band-dip-eod/research/charts.py     # report/figures/*.svg
```

Seeds: 20260930 (direction placebo), 20260931 (timing), 20260932 (bootstrap). `counts.py`/`_lock.py` were pre-lock feasibility and locking helpers.
