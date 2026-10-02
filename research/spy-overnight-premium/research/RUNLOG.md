# Run log

Append-only. One entry per store run.
## 2026-10-02T19:55:51+00:00
- rules_sha256 14f65d00f766db9b441c279b85830b9c8e3dd7ec95594904aa0220cbc7afc796
- git_head 8068750d3850669df5441a076ec108499b4acd44 dirty=yes
- reason: initial pre-registered run
- SPY full Sharpe 0.1326 return 0.1403 | IS Sharpe 0.1425 | OOS Sharpe 0.0527 return 0.0027 | trades full/IS/OOS 3958/3392/566 | status Rejected

## 2026-10-02T19:55:57+00:00
- rules_sha256 14f65d00f766db9b441c279b85830b9c8e3dd7ec95594904aa0220cbc7afc796
- git_head 8068750d3850669df5441a076ec108499b4acd44 dirty=yes
- reason: verification replay
- matched 3958/3958 SPY trades on side, entry time, entry price, exit time, exit price, gross, net, and reason; strategy_net matched on all 3958 evaluation sessions; explicit sample of 40 (seed 20261043) matched (0 bad)

## 2026-10-02T19:59:40+00:00
- rules_sha256 14f65d00f766db9b441c279b85830b9c8e3dd7ec95594904aa0220cbc7afc796
- git_head 8068750d3850669df5441a076ec108499b4acd44 dirty=yes
- reason: post hoc daily-versus-1-minute price discrepancy count; no strategy return; verdict unchanged
- SPY compared 1254 close>20bp 111 open>20bp 224; QQQ compared 1254 close>20bp 169 open>20bp 281
