# Run log

One entry per store run, appended by backtest.py. Never edited.

## 2026-10-15T04:51:21+00:00

- Reason: initial pre-registered run
- Rules sha256: `33e478d8cfd481b9826afd9285f265ed54c5f82b945f76ff0754b5728f4c908c`
- Git HEAD: `1b4fd133391e71e42b84c4bdb713b595eef97f3b` (dirty: True)
- Sharpe full / IS / OOS: -0.915 / -0.781 / -1.360
- Total return full / IS / OOS: -1.000 / -0.995 / -0.941
- Trades full / OOS: 4131 / 1362
- Status: Rejected

## 2026-10-15T04:57:29+00:00

- Reason: code did not do what RULES.md says: a missing-bar exit that found a second session with no bar left sell_index unchanged, so the book could stay open and later sessions earned 0. The open-sell loop now waits for the next session that has a bar, or flattens at the last marked close if the sample ends. The primary hold exits the same session and was not on that path. Before this rerun the primary Sharpe full / IS / OOS was -0.915 / -0.781 / -1.360 and total return was -1.000 / -0.995 / -0.941, with 4131 / 1362 trades.
- Rules sha256: `33e478d8cfd481b9826afd9285f265ed54c5f82b945f76ff0754b5728f4c908c`
- Git HEAD: `1b4fd133391e71e42b84c4bdb713b595eef97f3b` (dirty: True)
- Sharpe full / IS / OOS: -0.915 / -0.781 / -1.360
- Total return full / IS / OOS: -1.000 / -0.995 / -0.941
- Trades full / OOS: 4131 / 1362
- Status: Rejected

## 2026-10-15T05:51:53+00:00

- Reason: post hoc hold sweep for holds 1 through 31, requested after the verdict. The locked primary stays hold 1. results.json was not rewritten. Appended by hold_sweep.py.
- Rules sha256: `33e478d8cfd481b9826afd9285f265ed54c5f82b945f76ff0754b5728f4c908c`
- Output: `hold_sweep.json` and `hold_sweep.csv`. Label: post hoc.
- Locked grid holds 1, 2, 3, and 5 match results.json: True
