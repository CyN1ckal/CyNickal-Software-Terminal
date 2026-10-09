# Trade ledger, trade statistics, and backtesting

| Field | Value |
|---|---|
| Status | Implemented 2026-09-25, PRs 1–8: schema v6 and the Store ledger API, lot matching, equity curve and statistics, the LEDGER and STATS panels, the backtest engine and strategies, the BACKTEST panel, and trade markers on charts. |
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
| D9 | Fills are as-traded. Lot matching applies splits from `corporate_action` to open lots. Backtests run on split-adjusted bars (`adjustBarsForSplits`). Intraday chart loads also split-adjust in memory. Cash dividends are credited in the equity curve and in the buy-and-hold benchmark. They are not an external cash flow. | Matches the store: `bar` is as-traded and adjusted in memory. |
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

Pure C++ with no ImGui and no SQLite. It lives in `terminal_logic` and is tested in `terminal_tests`, like `risk/` and `options/`. It moves to a `libs/` target only if a command-line backtest tool needs it.

| File | Role |
|---|---|
| `apps/terminal/src/trading/Ledger.{h,cpp}` | `LotBook` steps through time: `advanceTo(ts)` applies splits, `apply(fill)` matches FIFO. `matchLots(fills, actions, as_of)` runs one to a date. Round trips carry allocated fees and net P&L. Partial closes, a fill that flips long to short, shorts, splits applied to open share lots (not options), a 100 multiplier on options. `fillCashFlow` and `unrealizedPnl` feed the equity curve. |
| `apps/terminal/src/trading/EquityCurve.{h,cpp}` | `equityCurve(fills, cash_flows, actions, marks, points)` replays a `LotBook` to each point: cash, market value, equity, flows, open positions. Returns are time-weighted (flows arrive at the start of a period), so a deposit is not a gain; growth and drawdown come from that index. Shares mark at the latest as-traded close, or at average price before their first mark. Options mark at their last trade in the ledger. `closeMarks` and `markTimes` build marks and session-close points from bars. Callers pass session closes, so intraday ledgers are already grouped before annualizing. |
| `apps/terminal/src/trading/TradeStats.{h,cpp}` | `tradeStats(round_trips)`: count, win rate, gross profit and loss, fees, average trade (expectancy), average win and loss, payoff ratio, profit factor, largest win and loss, streaks, average holding time. `curveStats(curve)`: total return, net P&L, CAGR, volatility, Sharpe, Sortino, Calmar, maximum drawdown and its length, exposure, and VaR/CVaR of period P&L through `valueAtRisk` from `risk/HistoricalRisk.h`. `buyAndHoldReturn` is the benchmark. MAE/MFE is deferred to the backtest PRs, where bars and entries share a split basis. |
| `apps/terminal/src/backtest/Engine.{h,cpp}` | `runBacktest(bars, strategy, options, config)` returns fills, equity, and diagnostics. Deterministic. |
| `apps/terminal/src/backtest/Strategy.{h,cpp}` | `registerStrategy`. `StrategyType`: id, display name, `StudyOption` inputs, and a `process` callback. Static registration, first id wins. There is no `StrategyRegistry.h`. |
| `apps/terminal/src/backtest/strategies/` | MA crossover, Bollinger mean reversion, N-bar momentum. Each calls the matching study's `process`. Intraday shock reversion (`shock_revert`, 1-minute bars) is `apps/terminal/src/backtest/strategies/ShockRevert.cpp` and keeps its own time-of-day volatility profile instead. Its research notes are not a path in this repo. |

### Engine rules

- A strategy writes a **target position** per bar, in units of the sizing rule. NaN is "no opinion" and keeps the current target.
- A change in target becomes an order that fills at the **next bar's open**. No order fills on the bar that produced it.
- Stop-loss and take-profit are engine settings, checked against each later bar's low and high. A bar that gaps through the level fills at its open. When one bar touches both, the stop fills first. After either exit the engine stays flat until the strategy's target changes.
- `close_at_end` (on by default) closes the last position at the last close, so every trade is a round trip.
- Costs: commission per share, a minimum per order, and slippage in basis points against the trader.
- Sizing: fixed shares, fixed notional, or a percent of equity, rounded down to whole shares. A position is sized when its target changes, not rebalanced every bar.
- Optional flatten at the last bar of each session for intraday runs.
- Bars are split-adjusted before the run.

### No look-ahead

A strategy is causal when its target at bar *k* depends only on bars 0..*k*. The registry test runs every registered strategy on `bars[0..k]` for several *k* and checks that the first *k* targets match the full run.

---

## Panels

Each follows the PORTFOLIO pattern: a `PanelKind`, a panel vector in `CChartBook` with add, close, focus, and place, a `ledger:<id>` / `stats:<id>` / `backtest:<id>` window token, a chartbook struct saved by `CChartbookFile`, Ctrl+R through `requestPanelData`, and a received stamp.

| Panel | Contents |
|---|---|
| LEDGER | `ui/LedgerPanel`. Ledger picker and a Ledger menu (new, rename, delete with a confirmation). A summary row: deposits, cash, position value, equity, realized, unrealized after open fees, fees. Tabs **Positions** (open lots marked by `data/LedgerMarks`: the newer of the daily and 1-minute close, or the chain's last print for an option, its mid when it has not printed, and cost when it has neither), **Closed** (round trips, newest first), **Fills**, and **Cash**. Manual ledgers get an add-fill row (symbol, shares or option, side, quantity, price, fees, New York time, note; a date alone is the 16:00 close) and an add-cash row; right-click a fill or cash row to delete it. A symbol with no listing is fetched first, as PORTFOLIO does. Backtest ledgers hide both. The chartbook saves the chosen ledger and tab. Ctrl+R fetches marks through `portfolioFetchJobs`. Shared text lives in `ui/TradingFormat`, shared widgets in `ui/TradingWidgets`. |
| STATS | `ui/StatsPanel`, bound to one ledger, manual or backtest, with a benchmark symbol (SPY by default, empty for none). `data/LedgerAnalysis::analyzeLedger` evaluates the ledger at every stored daily close of the shares it traded, through now or, for a backtest ledger, through its run's `ts_end`; shares with no closes are named and held at cost. A table of returns (net profit, total return, CAGR, buy-and-hold benchmark, exposure), risk (volatility, Sharpe, Sortino, Calmar, maximum drawdown and its length, one-period VaR and CVaR), and trades. Charts: time-weighted return against the benchmark (`kAccent` and `kTextDim`), drawdown (`kDanger` wash), and net profit per closed trade (`kUp` and `kDown`). Opening a ledger fetches missing daily history once; Ctrl+R refetches all of it. The chartbook saves the ledger and benchmark. No new hues. |
| BACKTEST | `ui/BacktestPanel`. Strategy and its inputs, symbol, period (1d, 1h, 15m, 5m, 1m), range (two years of daily or 20 days of intraday by default), starting cash, sizing, commission, slippage, stop, take-profit, session flattening, close at the end. Run downloads the range through `IngestWorker` first unless every finished session in it already has complete coverage (a range that cannot be fetched runs on what is stored), then runs on a `BacktestWorker` thread with its own writer. `backtest/BacktestJob::runAndRecordBacktest` loads split-adjusted bars (1-minute bars composited for 5m–1h), runs the engine, takes session ends for flattening in the instrument's timezone, converts fills back to as-traded shares and prices (`unadjustFill`, undoing only the splits through the last bar that the bars were adjusted for), and records the ledger, starting capital, fills, and run in one transaction. The result offers **Open in Statistics** and **Open Ledger**; the runs table offers those, **Load Inputs**, and **Delete Run**. Closing the window during a run does not wait for it: `CChartBook` keeps the busy worker until the run records. The chartbook saves the inputs. A run's fills show on any chart through **Show Trades**. The symbol link group is later work. |

The chart pane's right-click menu has **Show Trades**, which picks a ledger (manual or backtest) whose fills of the charted instrument are drawn as triangles: a buy points up at its price in `kUp`, a sell down in `kDown`, with a tooltip for side, size, price, time, and note. `chart/CChartTrades::chartTradeMarkers` places each fill on the bar whose span holds it and divides its price by the splits the chart has adjusted for. The pane's settings save the ledger as `trades_ledger`; `ledger.id` is `AUTOINCREMENT`, so a deleted ledger's id never names a later one.

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

Later: broker CSV import, a command-line backtest with parameter sweeps, rule strategies built from two study instances, walk-forward runs, multi-symbol runs.
