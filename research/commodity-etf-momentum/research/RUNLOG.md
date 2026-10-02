# Run log

Append-only. One entry per store run.

## 2026-10-02T20:03:09+00:00
- rules_sha256 0a269fcb4d443675fdf2f5d6b96c75bfdccc93742152739ee67d5c0c9535e7e2
- git_head 8068750d3850669df5441a076ec108499b4acd44 dirty=yes
- reason: initial pre-registered run
- full Sharpe 0.309640 return 0.951933 | IS Sharpe 0.246983 | OOS Sharpe 0.574078 return 0.414511 | round trips full/IS/OOS 135/114/21 | status Inconclusive

## 2026-10-02T20:05:31+00:00
- rules_sha256 0a269fcb4d443675fdf2f5d6b96c75bfdccc93742152739ee67d5c0c9535e7e2
- git_head 8068750d3850669df5441a076ec108499b4acd44 dirty=yes
- reason: verification, independent rebuild of every trade
- verify matched 135/135 trades on symbol, side, entry and exit date and price, and exit reason. Seed 20261024 drew 40 entry dates covering 53 of those trades; the pass condition is the full list. OOS entries 21.
