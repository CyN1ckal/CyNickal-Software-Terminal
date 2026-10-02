# Run log

Append-only. One entry per run that computes portfolio returns.

## 2026-09-26 (aborted before any return was computed)
- reason: initial pre-registered run
- result: stopped at the data check with "C missing master sessions ['2021-12-31']". No weights, returns, or metrics were computed.
- cause: the RULES.md data check listed only P1's missing 2021-12-31. The channel-trend file lacks the same session, which has no bars in the store. RULES.md's calendar clause says a sleeve with no row on a master session contributes 0.
- fix: the check now allows exactly 2021-12-31 to be missing for P1 and C, and still aborts on any other gap. This is recorded as a deviation from the literal data check. It is not a rule change.

## 2026-09-26T17:12:06+00:00
- reason: initial pre-registered run (after the data-check fix above)
- rules_sha256: d26504d65904cba05494b735e4da665ac8bedcf51dae849f91e9b36bb57c75ab
- git_head: 73b746dbfa4cd11bb81b58f8976236c1af7dbb6e
- git_dirty: true
- A weights: Q 0.2000, P1 1.4200, T 0.3800, C 0.0000 (binding: intraday)
- A full: sharpe 1.2298, return 152.8130%
- A IS: sharpe 1.5306, return 83.6709%
- A OOS: sharpe 0.9075, return 37.6445%
- status from acceptance table: A Rejected; B Rejected

## 2026-09-26 verify.py (not a backtest rerun)
- reason: independent pure-Python replay of the grid search, scaling, and daily returns.
- result: A d = (0.10, 0.71, 0.19, 0.00), w = (0.20, 1.42, 0.38, 0.00); B d = (0.11, 0.89), w = (0.22, 1.78). Both match results.json. The daily returns of A, B, and N match daily.csv on all 1,235 sessions, with a maximum difference of 5.0e-11 (the CSV's 10-decimal rounding). The 2x-cost OOS Sharpe matches (A 0.5259, B 0.6317).
- backtest.py unchanged and not rerun; headlines unchanged.
