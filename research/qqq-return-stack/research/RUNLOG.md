# Run log: qqq-return-stack

Append-only. One entry per run that computes portfolio returns.

## 2026-09-27 01:15 UTC — deviation recorded before the first run (no returns computed)

While writing `backtest.py`, after the lock, I read the unit of the `cost` columns in two trade files. Only unit columns were printed, never a return statistic.

- **GAP** (`small-cap-gap-up-fade/trades.csv`): `cost` is in account-equity units, not a fraction of notional. On 2026-09-21 it is 152.36 on `equity_at_entry` 38,495.8. The locked formula `weight × cost` would charge a 1× cost of 152 per session, which is impossible. The only defensible reading is `weight × (gross_ret − net_ret)`, which equals `cost ÷ equity_at_entry`. That is what the code uses.
- **REV** (`low-liq-high-vol-mean-reversion/trades.csv`): `cost` and `borrow` are also in account-equity units (`gross_pnl = gross_ret × entry_notional`, with `entry_notional = |weight| × equity_at_entry`). The locked formula `(cost + borrow) × equity_at_entry ÷ equity_{t−1}` counts `equity_at_entry` once too often. The code uses `(cost + borrow) ÷ equity_{t−1}` as the primary reading, the one I would have chosen had I read the units first. The literal formula is also computed, and line 5 is reported under both readings.

Both formulas feed only the cost sweep and acceptance line 5. The 1× returns, the scales, and lines 1–4 and 6 do not use them.

## 2026-09-27 01:11 UTC — run

- Reason: first pre-registered run
- Rules sha256 `38dae5d16f5a`, git `73b746d` (dirty)
- S Sharpe full/IS/OOS 1.31 / 1.00 / 1.70; return +444.5% / +97.3% / +176.0%
- K OOS Sharpe 2.26, ALL OOS Sharpe 1.06, Q OOS Sharpe 1.01
- Status S: Rejected (5/6); K: Paper-trading candidate (6/6)

## 2026-09-27 01:25 UTC — verification and log notes (no new run)

- `verify.py` (pure Python, no shared code) reproduced all 12 weights, all 3,705 daily returns of S, K, and ALL (max abs diff 5.0e-13), and the 2× cost OOS Sharpes of S (0.873437) and K (1.625098). No mismatch, so no rerun.
- **Timestamp correction.** The deviation entry above is headed 01:15 UTC. It was written between the lock (01:08:15 UTC) and the first run (01:11 UTC); 01:15 was a mistyped estimate. The entry's content is unchanged.
- **Encoding.** The run entry's em dash was written in the Windows code page and showed as an invalid byte. The byte was re-encoded as UTF-8, and `backtest.py` now opens RUNLOG.md as UTF-8. No number changed.
