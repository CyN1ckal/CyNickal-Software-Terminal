// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "backtest/Engine.h"
#include "catch_amalgamated.hpp"
#include "trading/Ledger.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

constexpr terminal::InstrumentId kEngineId = 9;
// 2026-06-01 13:30 UTC, the 09:30 open in New York.
constexpr terminal::UnixSeconds kEngineStart = 1'780'320'600;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

terminal::Bar engineBar(int day, double open, double high, double low, double close)
{
    terminal::Bar bar;
    bar.instrument_id = kEngineId;
    bar.timeframe_s = terminal::kTimeframe1d;
    bar.ts = kEngineStart + (static_cast<terminal::UnixSeconds>(day) * 86'400);
    bar.open = open;
    bar.high = high;
    bar.low = low;
    bar.close = close;
    bar.volume = 1'000;
    return bar;
}

// Flat bars at `price`, one per day.
std::vector<terminal::Bar> flatBars(int count, double price)
{
    std::vector<terminal::Bar> bars;
    for (int day = 0; day < count; ++day)
    {
        bars.push_back(engineBar(day, price, price, price, price));
    }
    return bars;
}

terminal::BacktestConfig sharesConfig(double shares)
{
    terminal::BacktestConfig config;
    config.initial_cash = 10'000.0;
    config.sizing = terminal::BacktestSizing::FixedShares;
    config.sizing_value = shares;
    config.close_at_end = false;
    return config;
}

terminal::BacktestResult runEngine(const std::vector<terminal::Bar>& bars, const std::vector<double>& targets,
                                   const terminal::BacktestConfig& config)
{
    return terminal::runTargets(bars, targets, config, kEngineId, terminal::TradeAssetKind::Equity);
}

}  // namespace

TEST_CASE("a target fills at the next bar's open, never on the bar that decided it")
{
    std::vector<terminal::Bar> bars{
        engineBar(0, 100, 101, 99, 100),
        engineBar(1, 102, 103, 101, 102),
        engineBar(2, 104, 105, 103, 104),
    };
    const std::vector<double> targets{1, 1, 1};
    const auto result = runEngine(bars, targets, sharesConfig(10));
    REQUIRE(result.fills.size() == 1);
    CHECK(result.fills[0].ts == bars[1].ts);
    CHECK(result.fills[0].price == 102.0);
    CHECK(result.fills[0].quantity == 10.0);
    CHECK(result.fills[0].instrument_id == kEngineId);
    CHECK(result.fills[0].note == "open");
    CHECK(result.position == std::vector<double>{0, 10, 10});
    CHECK(result.equity[0] == Catch::Approx(10'000.0));
    CHECK(result.equity[2] == Catch::Approx(10'000.0 + (10 * (104 - 102))));
}

TEST_CASE("zero-cost buy and hold returns the price move from the first open")
{
    std::vector<terminal::Bar> bars{
        engineBar(0, 100, 100, 100, 100),
        engineBar(1, 100, 110, 95, 105),
        engineBar(2, 106, 120, 104, 118),
        engineBar(3, 117, 126, 115, 125),
    };
    terminal::BacktestConfig config;
    config.initial_cash = 10'000.0;  // exactly 100 shares at the 100 open
    const std::vector<double> targets{1, 1, 1, 1};
    const auto result = runEngine(bars, targets, config);
    REQUIRE(result.fills.size() == 2);
    CHECK(result.fills[1].note == "end");
    CHECK(result.fills[1].ts == bars[3].ts + terminal::kUsRthDurationS);
    CHECK(result.equity.back() / config.initial_cash == Catch::Approx(125.0 / 100.0));
    CHECK(result.position.back() == 0.0);
}

TEST_CASE("a held target is not rebalanced as equity moves")
{
    std::vector<terminal::Bar> bars;
    for (int day = 0; day < 10; ++day)
    {
        const double price = 100.0 + (day * 3.0);
        bars.push_back(engineBar(day, price, price, price, price));
    }
    terminal::BacktestConfig config;
    config.initial_cash = 10'000.0;
    config.close_at_end = false;
    const std::vector<double> targets(10, 1.0);
    const auto result = runEngine(bars, targets, config);
    REQUIRE(result.fills.size() == 1);
    CHECK(result.fills[0].quantity == 100.0);
    CHECK(result.position.back() == 100.0);
}

TEST_CASE("slippage and commission move every fill against the trader")
{
    auto config = sharesConfig(10);
    config.slippage_bps = 10.0;
    config.commission_per_share = 0.01;
    config.commission_minimum = 1.0;
    const auto bars = flatBars(4, 100.0);
    const std::vector<double> targets{1, 0, kNaN, kNaN};
    const auto result = runEngine(bars, targets, config);
    REQUIRE(result.fills.size() == 2);
    CHECK(result.fills[0].price == Catch::Approx(100.1));
    CHECK(result.fills[0].fees == Catch::Approx(1.0));
    CHECK(result.fills[1].price == Catch::Approx(99.9));
    CHECK(result.fills[1].quantity == -10.0);
    CHECK(result.equity.back() == Catch::Approx(10'000.0 - 2.0 - 2.0));
}

TEST_CASE("a stop fills at the open after a gap and at the level when traded through")
{
    auto config = sharesConfig(10);
    config.stop_loss_pct = 5.0;
    std::vector<terminal::Bar> gap{
        engineBar(0, 100, 100, 100, 100),
        engineBar(1, 100, 101, 99, 100),
        engineBar(2, 90, 92, 88, 91),
    };
    const std::vector<double> held{1, 1, 1};
    const auto gapped = runEngine(gap, held, config);
    REQUIRE(gapped.fills.size() == 2);
    CHECK(gapped.fills[1].note == "stop");
    CHECK(gapped.fills[1].price == 90.0);
    CHECK(gapped.stop_exits == 1);

    std::vector<terminal::Bar> through{
        engineBar(0, 100, 100, 100, 100),
        engineBar(1, 100, 101, 99, 100),
        engineBar(2, 98, 99, 94, 97),
    };
    const auto touched = runEngine(through, held, config);
    REQUIRE(touched.fills.size() == 2);
    CHECK(touched.fills[1].price == Catch::Approx(95.0));
}

TEST_CASE("a bar that reaches both levels takes the stop")
{
    auto config = sharesConfig(10);
    config.stop_loss_pct = 5.0;
    config.take_profit_pct = 5.0;
    std::vector<terminal::Bar> bars{
        engineBar(0, 100, 100, 100, 100),
        engineBar(1, 100, 101, 99, 100),
        engineBar(2, 100, 106, 94, 100),
    };
    const std::vector<double> held{1, 1, 1};
    const auto result = runEngine(bars, held, config);
    REQUIRE(result.fills.size() == 2);
    CHECK(result.fills[1].note == "stop");
    CHECK(result.stop_exits == 1);
    CHECK(result.target_exits == 0);
}

TEST_CASE("after a protective exit the engine waits for the target to change")
{
    auto config = sharesConfig(10);
    config.take_profit_pct = 2.0;
    std::vector<terminal::Bar> bars{
        engineBar(0, 100, 100, 100, 100),
        engineBar(1, 100, 103, 100, 102),
        engineBar(2, 102, 102, 101, 101),
        engineBar(3, 101, 101, 100, 100),
        engineBar(4, 100, 100, 100, 100),
        engineBar(5, 100, 100, 100, 100),
    };
    // Held long through bar 2, then flat on bar 3, then long again.
    const std::vector<double> targets{1, 1, 1, 0, 1, 1};
    const auto result = runEngine(bars, targets, config);
    REQUIRE(result.fills.size() == 3);
    CHECK(result.fills[0].ts == bars[1].ts);
    CHECK(result.fills[1].note == "target");
    CHECK(result.fills[1].price == Catch::Approx(102.0));
    CHECK(result.fills[2].ts == bars[5].ts);
    CHECK(result.target_exits == 1);
}

TEST_CASE("shorts need permission and NaN keeps the last target")
{
    const auto bars = flatBars(5, 50.0);
    const std::vector<double> targets{-1, kNaN, kNaN, kNaN, kNaN};
    auto config = sharesConfig(4);
    config.allow_short = false;
    CHECK(runEngine(bars, targets, config).fills.empty());
    config.allow_short = true;
    const auto result = runEngine(bars, targets, config);
    REQUIRE(result.fills.size() == 1);
    CHECK(result.fills[0].quantity == -4.0);
    CHECK(result.position.back() == -4.0);
}

TEST_CASE("session flattening closes at each session's last bar and reopens the next day")
{
    // Two sessions of two minute bars each.
    const auto minute = [](terminal::UnixSeconds ts, double price) {
        terminal::Bar bar;
        bar.instrument_id = kEngineId;
        bar.timeframe_s = terminal::kTimeframe1m;
        bar.ts = ts;
        bar.open = price;
        bar.high = price;
        bar.low = price;
        bar.close = price;
        bar.volume = 1;
        return bar;
    };
    const std::vector<terminal::Bar> bars{
        minute(kEngineStart, 10.0),
        minute(kEngineStart + 60, 11.0),
        minute(kEngineStart + 86'400, 12.0),
        minute(kEngineStart + 86'460, 13.0),
    };
    auto config = sharesConfig(1);
    config.flatten_at_session_end = true;
    const std::vector<double> targets{1, 1, 1, 1};
    const auto result = runEngine(bars, targets, config);
    REQUIRE(result.fills.size() == 3);
    CHECK(result.fills[0].ts == bars[1].ts);
    CHECK(result.fills[1].note == "session close");
    CHECK(result.fills[1].ts == bars[1].ts + 60);
    CHECK(result.fills[2].ts == bars[2].ts);
    // close_at_end is off, so the last session's position stays open.
    CHECK(result.position.back() == 1.0);
}

TEST_CASE("a run is deterministic and its fills reproduce its equity")
{
    std::vector<terminal::Bar> bars;
    double price = 100.0;
    for (int day = 0; day < 60; ++day)
    {
        const double next = price + ((day % 7) - 3) * 0.8;
        bars.push_back(engineBar(day, price, std::max(price, next) + 1, std::min(price, next) - 1, next));
        price = next;
    }
    std::vector<double> targets;
    for (int day = 0; day < 60; ++day)
    {
        targets.push_back((day / 5) % 3 == 0 ? 1.0 : ((day / 5) % 3 == 1 ? -1.0 : 0.0));
    }
    terminal::BacktestConfig config;
    config.initial_cash = 50'000.0;
    config.sizing_value = 50.0;
    config.commission_per_share = 0.005;
    config.commission_minimum = 1.0;
    config.slippage_bps = 5.0;
    const auto first = runEngine(bars, targets, config);
    const auto second = runEngine(bars, targets, config);
    REQUIRE(first.fills.size() == second.fills.size());
    for (std::size_t index = 0; index < first.fills.size(); ++index)
    {
        CHECK(first.fills[index].ts == second.fills[index].ts);
        CHECK(first.fills[index].quantity == second.fills[index].quantity);
        CHECK(first.fills[index].price == second.fills[index].price);
    }
    CHECK(first.equity == second.equity);

    // Flat at the end: equity is the starting cash plus every fill's cash flow.
    const auto book = terminal::matchLots(first.fills, {}, bars.back().ts + 86'400);
    CHECK(book.positions.empty());
    CHECK(first.equity.back() == Catch::Approx(config.initial_cash + book.trade_cash));
    CHECK(book.realized_pnl == Catch::Approx(book.trade_cash));
}
