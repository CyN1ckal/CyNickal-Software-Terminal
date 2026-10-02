# Run log

Append-only. One entry per store run.

## 2026-09-26T18:06:43+00:00
- reason: rerun after a crash: predictions read SimResult.weeks as week_rows and raised before results.json or RUNLOG were written. Trading rules unchanged.
- rules_sha256: 59b886851c1c86945b986e3dfea4f45148ebecef57eb4b7306e6d9bd5313d0a1
- git_head: 73b746dbfa4cd11bb81b58f8976236c1af7dbb6e
- git_dirty: true
- full: sharpe -0.4382, return -58.5161%
- IS: sharpe 0.0086, return -4.4488%
- OOS: sharpe -1.1290, return -56.5847%
- status from acceptance table: Rejected

## 2026-09-26T18:07:53+00:00
- reason: code fix: a same-side add was rewriting entry_px to a size-weighted average. RULES.md records the entry as the first fill. Dollar P&L does not use entry_px. Previous logged headlines were full Sharpe -0.4382 return -58.5161%, IS 0.0086 / -4.4488%, OOS -1.1290 / -56.5847%.
- rules_sha256: 59b886851c1c86945b986e3dfea4f45148ebecef57eb4b7306e6d9bd5313d0a1
- git_head: 73b746dbfa4cd11bb81b58f8976236c1af7dbb6e
- git_dirty: true
- full: sharpe -0.4382, return -58.5161%
- IS: sharpe 0.0086, return -4.4488%
- OOS: sharpe -1.1290, return -56.5847%
- status from acceptance table: Rejected
