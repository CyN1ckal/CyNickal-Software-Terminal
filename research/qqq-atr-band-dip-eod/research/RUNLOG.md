# Run log

Append-only. One entry per store run.

## attempt 1 (crashed, no returns computed)
- self-test 17/17 passed before opening the store.
- crashed in the pre-write data check: hard 390/211 bar-count assertion failed on SPY 2026-03-03 (389 stored bars; one untraded minute, a benign case earlier studies of this store record). No return was computed and no output was written.
## attempt 2 (crashed, no outputs written)
- crashed in the direction-placebo vectorization (numpy broadcast bug) after computing QQQ metrics; the aggregate numbers were printed nowhere and are not results of record. Fix: reshape in the fill assignment.


## 2026-09-30T20:11:52+00:00
- rules_sha256 f48243b68f9e778fcdd89599b7d52ff7271cf62b8777e1037f969ddb2675bd16
- git_head 77e5ebef4d662cf5cf2a3c1cdb81bb29923f914f dirty=yes
- reason: initial pre-registered run
- QQQ full Sharpe -0.462 return -0.1066 | IS Sharpe -0.514 | OOS Sharpe -0.405 return -0.0461 | trades full/IS/OOS 124/68/56 | status Rejected
## attempt 4 (verify.py crash, no outputs changed)
- verify.py crashed assigning to a frozen mdq Bar (session property). Fix: use b.session directly. Rerun of verify only.

##  (verify.py rerun)
- reason: verify.py had an independent seeding bug in its naive ATR (if len(atrs)<14 never left the seed branch), so attempt-4 verify mismatched all trades. Fixed verify.py only; backtest.py and trades.csv unchanged. Result: 124/124 rebuilt trades match on side, entry/exit time and price, reason. No headline numbers changed.

- (addendum: UTC time of this verify rerun was lost to a PowerShell formatting error; appended 2026-09-30T20:15:32+00:00.)*

## 2026-09-30T20:18:06+00:00
- rules_sha256 f48243b68f9e778fcdd89599b7d52ff7271cf62b8777e1037f969ddb2675bd16
- git_head 77e5ebef4d662cf5cf2a3c1cdb81bb29923f914f dirty=yes
- reason: initial pre-registered run
- QQQ full Sharpe -0.462 return -0.1066 | IS Sharpe -0.514 | OOS Sharpe -0.405 return -0.0461 | trades full/IS/OOS 124/68/56 | status Rejected
- (addendum to entry above: this was not the initial run; the initial run is the entry dated 2026-09-30 with full/IS/OOS headline numbers. Reason for THIS rerun: backtest.py did not emit the pre-registered touch-vs-non-touch day-outcome breakdown, so the code did not fully implement RULES.md. Headline numbers identical before/after; results.json compared field-by-field with run_utc/git/breakdown excluded: equal. Recorded 2026-09-30T20:18:29+00:00.)*
- verify.py rerun against the new trades.csv: 124/124 matched (file content unchanged by the rerun). Recorded 2026-09-30T20:18:37+00:00)*
