# SPY RSI(2) dip-buy

**Status: Paper-trading candidate** (2026-09-26). Passed all 7 pre-registered tests. The timing placebo passed at p = 0.046 against a 0.05 line, and fails under two of five other seeds. Out of sample: +33.8%, Sharpe 1.29, 32 trades, 93.8% winners, trade skew −3.37. Not for live capital.

The request was a stock-index strategy with a negatively skewed payoff and a high win rate. The rule is Connors' 2-period RSI dip-buy on SPY. At 15:50 ET it buys market-on-close when RSI(2) is below 10, and sells market-on-close at the first close above the 5-day average. It is long only and has no stop.

- **Report:** [report/REPORT.md](report/REPORT.md)
- **Research:** [research/](research/): [RULES.md](research/RULES.md) (locked, sha256 `e343018e290a`), [RUNLOG.md](research/RUNLOG.md), `counts.py`, `backtest.py`, `verify.py`, `posthoc.py`, `charts.py`, and their outputs

## Reproduce

From the repo root:

```bash
python research/spy-rsi2-dip-buy/research/counts.py
```

```bash
python research/spy-rsi2-dip-buy/research/backtest.py --reason "reproduce"
```

```bash
python research/spy-rsi2-dip-buy/research/verify.py
```

```bash
python research/spy-rsi2-dip-buy/research/posthoc.py
```

```bash
python research/spy-rsi2-dip-buy/research/charts.py
```

Every price comes from regular-hours 1-minute bars. The store's daily bars carry extended-hours prices from late 2024 on (see the report, §4), so this study does not read them.
