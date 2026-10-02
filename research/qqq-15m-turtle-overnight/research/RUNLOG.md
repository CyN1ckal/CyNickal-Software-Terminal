# Run log

Append-only. One entry per store run.

## 2026-09-26T16:52:02+00:00
- reason: initial pre-registered run
- rules_sha256: d5fb6d5043b0ed6c32089a674fb7cd9461769b47744525b44b089fdcdd0fb334
- git_head: 73b746dbfa4cd11bb81b58f8976236c1af7dbb6e
- git_dirty: true
- full: sharpe 0.4959, return 38.3894%
- IS: sharpe 0.8110, return 37.7197%
- OOS: sharpe 0.0902, return 0.4863%
- status from acceptance table: Rejected

## 2026-09-26T16:55:40+00:00
- reason: verify.py replay (not a backtest rerun). First pass: QQQ 500/500 and SPY 522/522 trades matched; IGV had 15 mismatches, all in the entry or exit *time label* only (side, prices, and reason matched). Cause: verify.py labelled a 15-minute bucket with its first print's minute (e.g. 13:31) when the bucket's first minute was untraded, while mdq labels it with the aligned bucket open (13:30). Fixed in verify.py to label by the aligned open. Second pass: QQQ 500/500, SPY 522/522, IGV 477/477 matched on side, entry time and price, exit time and price, and reason. backtest.py unchanged and not rerun; headlines unchanged.
- rules_sha256: d5fb6d5043b0ed6c32089a674fb7cd9461769b47744525b44b089fdcdd0fb334
- git_head: 73b746dbfa4cd11bb81b58f8976236c1af7dbb6e
- git_dirty: true
- full: sharpe 0.4959, return 38.3894%
- IS: sharpe 0.8110, return 37.7197%
- OOS: sharpe 0.0902, return 0.4863%
