# Trade ledger, trade statistics, and backtesting

| Field | Value |
|---|---|
| Status | Draft. PRs 1–4 implemented 2026-09-25: schema v6 and the Store ledger API, lot matching, equity curve and statistics, the LEDGER panel. |
| Date | 2026-09-25 |
| Audience | First-party C++ in `libs/market-data` and `apps/terminal/` |
| Related | `docs/market-data-store.md` (schema and `Store`), `docs/composite-figi-identity.md` (FIGI identity), `docs/chart-panes.md` (panels, studies), `docs/design.md` (visual tokens) |

The terminal gains three features that share one data shape. A **ledger** is an account made of fills and cash flows. **Statistics** turn a ledger plus daily closes into round trips, an equity curve, and ratios. A **backtest** runs a registered C++ strategy over stored bars and records its result as a ledger. Statistics then treat a real account and a simulated one the same way.

```
manual entry / CSV ──┐
                     ├──► ledger: trade_fill + ledger_cash_flow ──► lot matching ──► round trips, positions ──► stats
backtest engine ─────┘                                                ▲                                        ▲
                                                               corporate_action (splits)           bar (daily closes as marks)
```

---

## Decisions

| # | Decision | Rationale |
|---|---|---|
| D1 | A ledger is its own table, not a `portfolio`. | `portfolio_holding` is hand-edited quantity. A ledger derives positions from fills. Two sources of truth for one book would drift. A later "copy positions to portfolio" action can write `replaceHoldings` from a ledger. |
| D2 | Strategies are native C++, registered like studies. No scripting language. | Same pattern as `StudyRegistry`. Strategies call study `process` functions, so a backtest sees the numbers the chart draws. |
| D3 | Money is `double` (SQLite `REAL`), as in store K18. | One numeric policy in the tree. Tests compare with a tolerance. |
| D4 | Backtests run on equities and ETFs only. The ledger still records option fills. | The store keeps the latest option chain, not option price history. |
| D5 | v1 backtests hold one instrument. | Fills carry an instrument, so multi-symbol runs can come later without a schema change. |
| D6 | Fills and cash flows are stored. Round trips, positions, equity curves, and statistics are derived on read. | Same rule as indicators on `bar`: derive, do not densify. A fix to the math fixes every past run. |
| D7 | A backtest ledger is written once, in one transaction, by `recordBacktestRun`. Its fills cannot be edited. Deleting the ledger deletes the run. | A run is a reproducible artifact: strategy id, parameters, config, range, and engine version are stored beside its fills. |
| D8 | Quantity is signed: positive buys, negative sells. Fees are a separate non-negative amount. | Matches `portfolio_holding.quantity`. |
| D9 | Fills are as-traded. Lot matching applies splits from `corporate_action` to open lots. Backtests run on split-adjusted bars (`adjustBarsForSplits`). Dividends are ignored in v1. | Matches the store: `bar` is as-traded and adjusted in memory. |
| D10 | Starting capital is a cash flow, not a column. A backtest writes its initial cash as the first cash flow of its ledger. | Deposits and withdrawals are needed for return math on a real account anyway. One path serves both. |

---

## Schema v6

`libs/market-data/schema/v6.sql` adds four tables and changes no earlier table. `kSchemaUserVersion` becomes 6.

| From `user_version` | Action |
|---|---|
| 0 | `v4.sql`, `v5.sql`, `v6.sql`, stamp 6 |
| 4 | Check the v4 tables, then `v5.sql`, `v6.sql`, stamp 6 |
| 5 | Check the v4 and v5 tables, then `v6.sql`, stamp 6 |
| 6 | Check every table. Run nothing. |

| Table | Role |
|---|---|
| `ledger` | `id`, `name`, `kind` (`manual` or `backtest`), `created_at`, `updated_at`. Manual names are unique ignoring case. Backtest names repeat. |
| `trade_fill` | One execution. `ledger_id` (CASCADE), `instrument_id` (RESTRICT), `asset_kind` (`equity`, `etf`, `option`), the option identity columns of `portfolio_holding`, `ts` (UTC seconds), signed `quantity`, `price`, `fees`, `note`, `external_id`. |
| `ledger_cash_flow` | Deposit (positive) or withdrawal (negative). `ledger_id` (CASCADE), `ts`, `amount`, `note`, `external_id`. |
| `backtest_run` | `ledger_id` (unique, CASCADE), `strategy_id`, `params_json`, `config_json`, `instrument_id` (RESTRICT), `timeframe_s`, `ts_begin`, `ts_end`, `engine_version`, `created_at`. |

`external_id` is unique within a ledger when present. Appending a row whose `external_id` already exists skips it, so a broker CSV can be imported twice without doubling the account.

Re-ingesting bars, statements, and chains must not delete an instrument a fill or a run still references. `ON DELETE RESTRICT` enforces that, as it does for holdings.

### Store API

```cpp
LedgerId createLedger(std::string_view name);                      // manual
void renameLedger(LedgerId id, std::string_view name);             // manual only
std::vector<Ledger> listLedgers() const;                           // manual first, then backtest; by name, id
std::optional<Ledger> findLedger(LedgerId id) const;
void deleteLedger(LedgerId id);                                    // cascades fills, cash, run

LedgerAppendResult appendFills(LedgerId id, std::span<const TradeFill> fills);          // manual only
void deleteFill(LedgerId id, TradeFillId fill_id);                                      // manual only
std::vector<TradeFill> queryFills(LedgerId id) const;                                   // ts, then id

LedgerAppendResult appendCashFlows(LedgerId id, std::span<const LedgerCashFlow> flows); // manual only
void deleteCashFlow(LedgerId id, LedgerCashFlowId flow_id);                             // manual only
std::vector<LedgerCashFlow> queryCashFlows(LedgerId id) const;                          // ts, then id

RecordedBacktest recordBacktestRun(std::string_view name, const BacktestRun& run,
                                   std::span<const TradeFill> fills,
                                   std::span<const LedgerCashFlow> cash_flows);
std::vector<BacktestRun> listBacktestRuns() const;                 // newest first
std::optional<BacktestRun> findBacktestRun(BacktestRunId id) const;
std::optional<BacktestRun> findBacktestRunForLedger(LedgerId id) const;
```

Fills and runs are written by FIGI and read back with `instrument_id`, the current symbol, and `listing_open`, as holdings are. A batch is validated in full before the transaction opens. One bad row rejects the whole call.

---

## Domain code

Pure C++ with no ImGui and no SQLite. It lives in `terminal_core` and is tested in `terminal_tests`, like `risk/` and `options/`. It moves to a `libs/` target only if a command-line backtest tool needs it.

| File | Role |
|---|---|
| `apps/terminal/src/trading/Ledger.{h,cpp}` | `LotBook` steps through time: `advanceTo(ts)` applies splits, `apply(fill)` matches FIFO. `matchLots(fills, actions, as_of)` runs one to a date. Round trips carry allocated fees and net P&L. Partial closes, a fill that flips long to short, shorts, splits applied to open share lots (not options), a 100 multiplier on options. `fillCashFlow` and `unrealizedPnl` feed the equity curve. |
| `apps/terminal/src/trading/EquityCurve.{h,cpp}` | `equityCurve(fills, cash_flows, actions, marks, points)` replays a `LotBook` to each point: cash, market value, equity, flows, open positions. Returns are time-weighted (flows arrive at the start of a period), so a deposit is not a gain; growth and drawdown come from that index. Shares mark at the latest as-traded close, or at average price before their first mark. Options mark at their last trade in the ledger. `closeMarks` and `markTimes` build marks and session-close points from bars. Callers pass session closes, so intraday ledgers are already grouped before annualizing. |
| `apps/terminal/src/trading/TradeStats.{h,cpp}` | `tradeStats(round_trips)`: count, win rate, gross profit and loss, fees, average trade (expectancy), average win and loss, payoff ratio, profit factor, largest win and loss, streaks, average holding time. `curveStats(curve)`: total return, net P&L, CAGR, volatility, Sharpe, Sortino, Calmar, maximum drawdown and its length, exposure, and VaR/CVaR of period P&L through `valueAtRisk` from `risk/HistoricalRisk.h`. `buyAndHoldReturn` is the benchmark. MAE/MFE is deferred to the backtest PRs, where bars and entries share a split basis. |
| `apps/terminal/src/backtest/Engine.{h,cpp}` | `runBacktest(bars, strategy, options, config)` returns fills, equity, and diagnostics. Deterministic. |
| `apps/terminal/src/backtest/StrategyRegistry.{h,cpp}` | `StrategyType`: id, display name, `StudyOption` inputs, and a `process` callback. Static registration, first id wins. |
| `apps/terminal/src/backtest/strategies/` | MA crossover, Bollinger mean reversion, N-bar momentum. Each calls the matching study's `process`. |

### Engine rules

- A strategy writes a **target position** per bar, in units of the sizing rule. NaN is "no opinion" and keeps the current target.
- A change in target becomes an order that fills at the **next bar's open**. No order fills on the bar that produced it.
- Stop-loss and take-profit are engine settings, checked against each later bar's low and high. A bar that gaps through the level fills at its open. When one bar touches both, the stop fills first.
- Costs: commission per share, a minimum per order, and slippage in basis points against the trader.
- Sizing: fixed shares, fixed notional, or a percent of equity, rounded down to whole shares.
- Optional flatten at the last bar of each session for intraday runs.
- Bars are split-adjusted before the run.

### No look-ahead

A strategy is causal when its target at bar *k* depends only on bars 0..*k*. The registry test runs every registered strategy on `bars[0..k]` for several *k* and checks that the first *k* targets match the full run.

---

## Panels

Each follows the PORTFOLIO pattern: a `PanelKind`, a panel vector in `CChartBook` with add, close, focus, and place, a `ledger:<id>` / `stats:<id>` / `backtest:<id>` window token, a chartbook struct saved by `CChartbookFile`, Ctrl+R through `requestPanelData`, and a received stamp.

| Panel | Contents |
|---|---|
| LEDGER | `ui/LedgerPanel`. Ledger picker and a Ledger menu (new, rename, delete with a confirmation). A summary row: deposits, cash, position value, equity, realized, unrealized after open fees, fees. Tabs **Positions** (open lots marked by `data/LedgerMarks`: the newer of the daily and 1-minute close, or the chain's last print for an option), **Closed** (round trips, newest first), **Fills**, and **Cash**. Manual ledgers get an add-fill row (symbol, shares or option, side, quantity, price, fees, New York time, note; a date alone is the 16:00 close) and an add-cash row; right-click a fill or cash row to delete it. A symbol with no listing is fetched first, as PORTFOLIO does. Backtest ledgers hide both. The chartbook saves the chosen ledger and tab. Ctrl+R fetches marks through `portfolioFetchJobs`. Shared text lives in `ui/TradingFormat`, shared widgets in `ui/TradingWidgets`. |
| STATS | Bound to one ledger, manual or backtest. Statistics table, equity curve (`kAccent`), drawdown (`kDanger` wash), histogram of trade returns, per-symbol table. No new hues. |
| BACKTEST | Strategy, its inputs, symbol with a link group, period, range, costs, sizing. Run. Past runs with **Open in Stats** and **Show on chart**. Runs execute on a `BacktestWorker` thread with its own `Reader` and `Writer` connections. |

The chart pane gains an optional ledger overlay: buy and sell markers at fill times, up and down colors for direction.

---

## PR plan

| # | PR | Tests |
|---|---|---|
| 1 | Schema v6, the Store ledger API, the store doc section. | Migration from 0, 4, and 5. Damaged stamped-6 file refused. Round trips by FIGI. Validation rejects the whole batch. `external_id` skips. Backtest ledgers refuse edits. Cascade on delete. RESTRICT on the instrument. |
| 2 | `trading/Ledger`: lot matching and splits. | FIFO partials, a flip in one fill, shorts, split while held, option multiplier, fees in realized P&L. |
| 3 | `trading/EquityCurve` and `trading/TradeStats`. | Hand-computed figures. Final equity equals cash flows plus realized plus unrealized minus fees. |
| 4 | LEDGER panel and chartbook persistence. | Chartbook round trip. |
| 5 | STATS panel and mark fetch planning. | Fetch planning. |
| 6 | Backtest engine, strategy registry, three strategies. | No look-ahead. Deterministic. Zero-cost buy-and-hold equals the price return. Gap-through stop fills at the open. |
| 7 | `BacktestWorker`, BACKTEST panel, `recordBacktestRun` from the GUI. | Run to ledger to statistics. |
| 8 | Chart ledger overlay. | Marker to bar index mapping. |

Later: broker CSV import, a command-line backtest with parameter sweeps, rule strategies built from two study instances, walk-forward runs, multi-symbol runs, dividends.
