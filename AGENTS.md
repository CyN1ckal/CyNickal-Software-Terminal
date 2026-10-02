# Agent notes

## Querying market data

The store is `data/market-data.sqlite`. It holds bars, splits, coverage, statements, option chains, portfolios, ledgers, and backtest runs. Read it with `agent-data/mdq.py`, not with hand-written `sqlite3` SQL. mdq applies the terminal's symbol lookup, split adjustment, 09:30-anchored resampling, NYSE calendar, and FIFO ledger matching, and opens the store read-only.

```
python agent-data/mdq.py instruments
python agent-data/mdq.py bars QQQ --tf 5m --last 3 -f csv
python agent-data/mdq.py coverage SPY --tf 1m
```

Read [agent-data/README.md](agent-data/README.md) before any data analysis. Never write to the store, and do not run `ingest` (a paid API) unless asked.

## Researching strategies

Research on a trading strategy follows [.claude/skills/quant-research/SKILL.md](.claude/skills/quant-research/SKILL.md) (`/quant-research <idea>` in Claude Code). Rules are pre-registered and locked before any return is computed, every run is logged, and the verdict comes from acceptance criteria fixed in advance. Every strategy gets one folder, `research/<slug>/`, with the rules, code, and raw output nested in `research/<slug>/research/` and the final report in `research/<slug>/report/`.
