# Run log

One entry per run that computes returns. Append-only.

## 2026-09-26T16:29:23+00:00

- Reason: first run of the locked rules
- Rules sha256: `af095ca58156cd11bd677b8d60791574b23c8628421e4b4c88127c686735eb96`
- Git HEAD: `73b746dbfa4cd11bb81b58f8976236c1af7dbb6e` (dirty: True)
- Primary Sharpe full / IS / OOS: -0.326 / -0.459 / -0.192
- Primary total return full / IS / OOS: -0.2145 / -0.1484 / -0.0776
- Status from acceptance table: Rejected

## 2026-09-26T16:31:04+00:00 (post hoc)

- Reason: post hoc diagnostics (posthoc.py) after the verdict; re-runs the primary unchanged
- Rules sha256: `af095ca58156cd11bd677b8d60791574b23c8628421e4b4c88127c686735eb96`
- Git HEAD: `73b746dbfa4cd11bb81b58f8976236c1af7dbb6e`
- Primary full-sample return reproduced: -0.2145; verdict unchanged

## 2026-09-26T16:30Z (verify)

- `verify.py` (independent re-implementation) against the 16:29:23 run: 155/155 trades match (market, side, entry date and price, exit date and price, reason); 1,253 sessions, worst daily P&L difference $0.000001; final equity $78,545.62 in both. No fix needed; no rerun.
- Notes on the code before the first run (no store run was affected): the self-test caught a NumPy boolean subtraction in `sgn` and it was fixed; the "last session of the month" roll-cost date was changed to exclude the data's final partial month (2026-09) so it matches RULES.md. Both changes were made before the first run above.
- Readings of the rules applied literally where RULES.md was silent: trades are assigned to IS/OOS by entry date; the roll cost uses the position held through the session (after any fill at that session's open); the "valid-bar count equals dq.py" data check is implemented as the 1,260-bar and no-duplicate-date checks plus the 253-bar warm-up check.

## 2026-09-26T16:33:22+00:00 (post hoc)

- Reason: post hoc diagnostics (posthoc.py) after the verdict; re-runs the primary unchanged
- Rules sha256: `af095ca58156cd11bd677b8d60791574b23c8628421e4b4c88127c686735eb96`
- Git HEAD: `73b746dbfa4cd11bb81b58f8976236c1af7dbb6e`
- Primary full-sample return reproduced: -0.2145; verdict unchanged
