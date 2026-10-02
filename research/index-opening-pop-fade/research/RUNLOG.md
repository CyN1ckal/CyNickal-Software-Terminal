# Run log

Append-only. One entry per run that computes returns from the store.

## 2026-09-27T00:58:45+00:00

- Reason: initial run
- Rules sha256: `a72202a2dac027196bf83498ab227760bdd6fcb67ccfa0134526fbab5780d1b8`
- git HEAD: `73b746dbfa4cd11bb81b58f8976236c1af7dbb6e` (dirty: True)
- Primary SPY: full Sharpe -0.61, return -20.17%; IS Sharpe -0.57, return -9.33%; OOS Sharpe -0.66, return -11.96%; OOS trades 82. Status: Rejected (failed lines [1, 2, 3, 4, 5, 6])
- S1 SPY: full Sharpe 0.33, IS 0.29, OOS 0.38, OOS return 4.49%. Status: Rejected (failed lines [1, 2, 3, 5, 6])

## 2026-09-27T00:59:25+00:00

- Reason: verification replay (`verify.py`), required by the protocol. Not a backtest rerun; no headline number changed.
- Rules sha256: `a72202a2dac027196bf83498ab227760bdd6fcb67ccfa0134526fbab5780d1b8`
- Result: primary SPY 178, primary QQQ 189, S1 SPY 165, S1 QQQ 188 trades replayed from the store by an independent implementation; 0 mismatches on session, side, entry time and price, exit time and price, and net return.

## 2026-09-27T01:03:36+00:00

- Reason: post hoc analyses (`posthoc.py`) and figures (`charts.py`). Both read only daily.csv, trades.csv, results.json; neither opens the store. Logged for transparency. No headline number changed.
