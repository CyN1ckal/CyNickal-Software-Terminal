# Run log

One entry per store run, appended by backtest.py. Never edited.

## 2026-10-03T15:36:52+00:00

- Reason: Rerun after the first execution computed the book and then crashed with NameError on write_results before results.json, daily.csv, trades.csv, or RUNLOG.md were written. The missing import does not change the book. No headline numbers were saved from the crash.
- Rules sha256: `40e29bd16bef992aca504d1f9e1dbda8f5e4a6ebbcd8f9a222c77f305c7a9c27`
- Git HEAD: `738d3e7ec4340e05458317114dfe247d55a1ef16` (dirty: True)
- Sharpe full / IS / OOS: 1.525 / 1.753 / 1.516
- Total return full / IS / OOS: 6.229 / 0.977 / 2.657
- Trades full / OOS: 900 / 520
- Status: Paper-trading candidate
