# Run log

Append-only. One entry per store run.

## 2026-10-02T21:24:39+00:00
- rules_sha256 32ca8212e02921e86251a708c78acfb3f44495726987d855c52eaa91cc137957
- git_head 738d3e7ec4340e05458317114dfe247d55a1ef16 dirty=yes
- reason: bug fix: Spearman rank crashed when a grid Sharpe was undefined. The crashed process wrote no results.json, daily.csv, trades.csv, or RUNLOG entry, so there is no before headline.
- full Sharpe -0.0032235091182425107 return -4.191046540796297 | IS Sharpe -0.0034833348778102327 | OOS Sharpe None return 0.0 | trades full/IS/OOS 29/29/0 | status Inconclusive

## 2026-10-02T21:34:33+00:00
- rules_sha256 32ca8212e02921e86251a708c78acfb3f44495726987d855c52eaa91cc137957
- git_head 738d3e7ec4340e05458317114dfe247d55a1ef16 dirty=yes
- reason: verify.py independent replay
- matched 29/29 VIXY holdings on side, entry time and price, exit time and price, exit reason, hold, gross, and net. Seed 20261074 redrew 40 indexes from a population of 29. UVXY OOS Sharpe None matches results.json. VIXY ruin 2013-06-10. Unadjusted jumps: VIXY 2, UVXY 6.
