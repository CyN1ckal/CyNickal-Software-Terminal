# Run log

Append-only. One entry per store run.

## 2026-10-02T19:57:51+00:00
- rules_sha256 2e570c264cb3732130a73c7a3e927760d701a705db568b293e42bbb111565eb4
- git_head 8068750d3850669df5441a076ec108499b4acd44 dirty=yes
- reason: initial pre-registered run
- full Sharpe 0.11739161802128001 return 0.1033295201434492 | IS Sharpe 0.21581258588301078 | OOS Sharpe -0.650837390695698 return -0.10522411268282605 | trades full/IS/OOS 44/28/16 | status Inconclusive
## 2026-10-02T19:59:17+00:00
- rules_sha256 2e570c264cb3732130a73c7a3e927760d701a705db568b293e42bbb111565eb4
- git_head 8068750d3850669df5441a076ec108499b4acd44 dirty=yes
- reason: bug fix: benchmark exposure was reported as 0 because the no-trade helper ignored the position flag. Share path, trades, and headline Sharpe and return are unchanged from the 2026-10-02T19:57:51Z entry (full Sharpe 0.11739161802128001 return 0.1033295201434492, IS Sharpe 0.21581258588301078, OOS Sharpe -0.650837390695698 return -0.10522411268282605).
- full Sharpe 0.11739161802128001 return 0.1033295201434492 | IS Sharpe 0.21581258588301078 | OOS Sharpe -0.650837390695698 return -0.10522411268282605 | trades full/IS/OOS 44/28/16 | status Inconclusive
## 2026-10-02T20:02:46+00:00
- rules_sha256 2e570c264cb3732130a73c7a3e927760d701a705db568b293e42bbb111565eb4
- git_head 8068750d3850669df5441a076ec108499b4acd44
- reason: verify.py independent replay
- matched 44/44 round trips on side, entry date and price, exit date and price, and exit reason. Seed 20261054 checked 40 rebalance dates; all of 178 eligible dates were replayed.
## 2026-10-02T20:04:45+00:00
- rules_sha256 2e570c264cb3732130a73c7a3e927760d701a705db568b293e42bbb111565eb4
- git_head 8068750d3850669df5441a076ec108499b4acd44 dirty=yes
- reason: saved the direction and timing placebo draws for the figure. Headline Sharpe and return are unchanged from the 2026-10-02T19:59:17Z entry.
- full Sharpe 0.11739161802128001 return 0.1033295201434492 | IS Sharpe 0.21581258588301078 | OOS Sharpe -0.650837390695698 return -0.10522411268282605 | trades full/IS/OOS 44/28/16 | status Inconclusive

## note (not a store run)
- The first verify.py invocation, before the 2026-10-02T20:02:46Z entry, raised KeyError on trades.csv column `reason`. The column name is `exit_reason`. No trade was compared, and that process did not append a log entry. The 20:02:46Z entry is the rerun after the comparison used `exit_reason`. It matched 44/44.
