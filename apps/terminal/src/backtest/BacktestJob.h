// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "backtest/Engine.h"
#include "chart/CChartSettings.h"
#include "market_data/Store.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// One backtest the terminal can run and record.
struct BacktestRequest
{
    std::string strategy_id;
    std::vector<int> options;
    std::string symbol;
    ChartBarPeriod period{ChartBarPeriod::Day1};
    SessionDate from{};
    SessionDate to{};
    BacktestConfig config{};
};

struct BacktestOutcome
{
    bool ok{false};
    std::string error;
    RecordedBacktest recorded{};
    std::string ledger_name;
    std::size_t bars{};
    std::size_t fills{};
    double final_equity{};
    double total_return{};
};

// "1m", "5m", "15m", "1h", "1d". nullopt for anything else.
[[nodiscard]] std::optional<ChartBarPeriod> backtestPeriodFromCode(std::string_view code) noexcept;

// Every BacktestConfig field as a JSON object. The reader fills missing keys from
// the defaults and returns nullopt for a value of the wrong type or a non-object.
[[nodiscard]] std::string backtestConfigJson(const BacktestConfig& config);
[[nodiscard]] std::optional<BacktestConfig> backtestConfigFromJson(std::string_view json);

// A fill computed on split-adjusted bars, in the as-traded terms the ledger stores:
// every split after the fill's time multiplies its price and divides its quantity.
[[nodiscard]] TradeFill unadjustFill(TradeFill fill, std::span<const CorporateAction> actions);

// The bars a request runs on: split-adjusted, and composited from 1-minute bars
// for an intraday period. Throws when the symbol does not resolve.
[[nodiscard]] std::vector<Bar> backtestBars(const Store& store, const BacktestRequest& request);

// Runs the request and records it with recordBacktestRun: the ledger, one cash flow
// of initial_cash at the first bar, the as-traded fills, and the run row. Failures
// come back in the outcome, never as exceptions.
[[nodiscard]] BacktestOutcome runAndRecordBacktest(Store& writer, const BacktestRequest& request);

}  // namespace terminal
