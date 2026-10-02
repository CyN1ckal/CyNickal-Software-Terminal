# Run log

One entry per store run, appended by `backtest.py` (and `verify.py`). Never edited.

## 2026-09-27T00:37:21+00:00

- Reason: initial pre-registered run
- Rules sha256: `e343018e290a959d86a54450af47c91067a8942887327dec4ffc0d1715523ce4`
- Git HEAD: `73b746dbfa4cd11bb81b58f8976236c1af7dbb6e` (dirty: True)
- SPY Sharpe full / IS / OOS: 0.966 / 0.638 / 1.294
- SPY total return full / IS / OOS: 0.531 / 0.145 / 0.338
- Trades full / OOS: 66 / 32; win rate 0.818; trade skew -2.037
- Status: Paper-trading candidate

## 2026-09-27T00:38:14+00:00 (verify.py)

- Reason: independent replay of every SPY and QQQ trade
- Result: all trades match; SPY daily max abs diff 5.00e-11
- SPY Sharpe full / OOS (naive): 0.966 / 1.294
- Timing placebo p with a different placement method: 0.0495

## 2026-09-27T00:40 approx. (posthoc.py, first execution, logged retroactively)

- Reason: post hoc analyses (not part of the verdict). This first execution re-simulated the locked primary from the store to rerun the timing placebo under seeds 1–5 and did not yet log itself; this entry records it. It changed no file used by the verdict.
- Timing placebo p by seed: 1 → 0.0510, 2 → 0.0520, 3 → 0.0460, 4 → 0.0455, 5 → 0.0465, 20260927 → 0.0465

## 2026-09-27T00:41:19+00:00 (posthoc.py)

- Reason: post hoc analyses (not part of the verdict); re-simulates the locked primary to check the timing placebo under other seeds
- Timing placebo p by seed: {'1': 0.050974512743628186, '2': 0.051974012993503245, '3': 0.04597701149425287, '4': 0.04547726136931534, '5': 0.046476761619190406, '20260927': 0.046476761619190406}

## 2026-09-27T00:43:39+00:00 (posthoc.py)

- Reason: post hoc analyses (not part of the verdict); re-simulates the locked primary to check the timing placebo under other seeds
- Timing placebo p by seed: {'1': 0.050974512743628186, '2': 0.051974012993503245, '3': 0.04597701149425287, '4': 0.04547726136931534, '5': 0.046476761619190406, '20260927': 0.046476761619190406}
