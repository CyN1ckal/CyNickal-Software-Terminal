# Opening-gap fade on a Finviz sample

**Inconclusive.** The same opening-gap short as `small-cap-gap-up-fade`, on 43 new small-cap names drawn from Finviz, reached equity 0 on 30 July 2020. Out of sample, 2024-01-02 through 2026-09-25, the book had 0 trades and an undefined Sharpe. The pre-lock count was 476 out-of-sample signals. Three of the six pre-registered tests passed. The account that failed is not a paper-trading candidate.

- Report: [report/REPORT.md](report/REPORT.md)
- Rules, code, and raw output: [research/](research/)

From the repo root:

```bash
python research/finviz-gap-up-fade/research/backtest.py
python research/finviz-gap-up-fade/research/verify.py
python research/finviz-gap-up-fade/research/posthoc.py
python research/finviz-gap-up-fade/research/charts.py
```

`fetch_bars.py` calls the paid ingest. That download already ran for the sampled names.
