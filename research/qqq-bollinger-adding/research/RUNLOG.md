# Run log

Append-only. One entry per store run.

## 2026-09-26T19:15:07+00:00
- reason: initial
- rules_sha256: 48b0332c189f318384cd4325ca13d91678536df5085c74e4136d90dad4e4cda7
- git_head: 73b746dbfa4cd11bb81b58f8976236c1af7dbb6e
- git_dirty: true
- full: sharpe -1.5311, return -35.1415%
- IS: sharpe -1.9848, return -28.0177%
- OOS: sharpe -0.8916, return -9.8966%
- status: Rejected

## 2026-09-26T19:15:24+00:00
- reason: verify.py replayed all 1,254 QQQ sessions independently (own 5-minute bars from 1-minute bars, own band loops, own state machine) and matched all 2,217 campaigns on side, units, leg times and prices, exit time, exit price, and reason. It computes no returns. No strategy code change. Headlines unchanged from the initial run.
- rules_sha256: 48b0332c189f318384cd4325ca13d91678536df5085c74e4136d90dad4e4cda7
- git_head: 73b746dbfa4cd11bb81b58f8976236c1af7dbb6e
- git_dirty: true
- full: sharpe -1.5311, return -35.1415%
- IS: sharpe -1.9848, return -28.0177%
- OOS: sharpe -0.8916, return -9.8966%
