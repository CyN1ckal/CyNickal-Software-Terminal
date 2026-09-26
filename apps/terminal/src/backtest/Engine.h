// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "backtest/Strategy.h"
#include "market_data/Time.h"
#include "market_data/Types.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace terminal {

// Stored on every backtest_run. Raise it when a change to these rules changes fills.
inline constexpr int kBacktestEngineVersion = 1;

enum class BacktestSizing : std::uint8_t
{
    FixedShares = 0,    // sizing_value shares per unit of target
    FixedNotional,      // sizing_value dollars per unit of target, at the deciding close
    PercentEquity,      // sizing_value percent of equity per unit of target, at the deciding close
};

struct BacktestConfig
{
    double initial_cash{100'000.0};
    BacktestSizing sizing{BacktestSizing::PercentEquity};
    double sizing_value{100.0};
    // Commission is max(per_share * shares, minimum) on every fill, 0 when both are 0.
    double commission_per_share{0.0};
    double commission_minimum{0.0};
    // Every fill moves against the trader by this many basis points.
    double slippage_bps{0.0};
    // Percent from the average entry. Checked on each later bar's range.
    std::optional<double> stop_loss_pct;
    std::optional<double> take_profit_pct;
    bool allow_short{true};
    // Sell or cover at the last bar of each session, at its close.
    bool flatten_at_session_end{false};
    // Close whatever is open at the last bar's close, so every trade is a round trip.
    bool close_at_end{true};
    // Session dates for flatten_at_session_end.
    std::string timezone{"America/New_York"};
};

struct BacktestResult
{
    // instrument_id and kind are set; figi and symbol are left for the caller.
    std::vector<TradeFill> fills;
    // Parallel to the bars: the strategy's target, shares held after the bar, and
    // equity marked at the bar's close.
    std::vector<double> targets;
    std::vector<double> position;
    std::vector<double> equity;
    int stop_exits{};
    int target_exits{};
};

// Fills a target series. A change in target at bar i's close becomes an order at
// bar i + 1's open; nothing fills on the bar that decided it. A position is sized
// when its target changes and is not rebalanced while the target holds.
//
// Stop-loss and take-profit are checked against every bar a position is open,
// including the bar it opened on. A bar that opens beyond a level fills at its open;
// one that trades through it fills at the level. When one bar reaches both, the stop
// fills. After either exit the engine stays flat until the target changes.
//
// Shares are whole and rounded toward zero. Cash can go negative when prices gap;
// there is no margin model.
[[nodiscard]] BacktestResult runTargets(std::span<const Bar> bars,
                                        std::span<const double> targets,
                                        const BacktestConfig& config,
                                        InstrumentId instrument_id,
                                        TradeAssetKind kind);

// strategyTargets, then runTargets.
[[nodiscard]] BacktestResult runBacktest(std::span<const Bar> bars,
                                         const StrategyType& strategy,
                                         std::span<const int> options,
                                         const BacktestConfig& config,
                                         InstrumentId instrument_id,
                                         TradeAssetKind kind);

}  // namespace terminal
