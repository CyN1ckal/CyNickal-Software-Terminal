// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "backtest/Strategy.h"
#include "catch_amalgamated.hpp"
#include "market_data/Time.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace {

// A deterministic random walk with some trend changes, one bar per day.
std::vector<terminal::Bar> walkBars(int count)
{
    std::vector<terminal::Bar> bars;
    std::uint32_t state = 12'345;
    double price = 100.0;
    for (int day = 0; day < count; ++day)
    {
        state = (state * 1'664'525u) + 1'013'904'223u;
        const double noise = (static_cast<double>(state >> 8) / static_cast<double>(1u << 24)) - 0.5;
        const double drift = ((day / 40) % 2 == 0) ? 0.4 : -0.4;
        const double close = std::max(1.0, price + drift + (noise * 4.0));
        terminal::Bar bar;
        bar.instrument_id = 1;
        bar.timeframe_s = terminal::kTimeframe1d;
        bar.ts = 1'780'320'600 + (static_cast<terminal::UnixSeconds>(day) * 86'400);
        bar.open = price;
        bar.close = close;
        bar.high = std::max(price, close) + 0.5;
        bar.low = std::min(price, close) - 0.5;
        bar.volume = 1'000;
        bars.push_back(bar);
        price = close;
    }
    return bars;
}

// Regular-hours one-minute sessions on consecutive weekdays from 2026-03-02, across
// the March 8 DST change. Closes zigzag 0.005% around a level, so five-minute moves
// are tiny; shape(session, closes) then adds shocks with shiftFrom.
std::vector<terminal::Bar> minuteSessions(int sessions, const std::function<void(int, std::vector<double>&)>& shape)
{
    using namespace std::chrono;
    std::vector<terminal::Bar> bars;
    sys_days day{year{2026} / March / 2};
    for (int session = 0; session < sessions; ++session)
    {
        while (weekday{day} == Saturday || weekday{day} == Sunday)
        {
            day += days{1};
        }
        std::vector<double> closes(390);
        for (std::size_t slot = 0; slot < closes.size(); ++slot)
        {
            closes[slot] = 100.0 * (1.0 + (slot % 2 == 0 ? -0.00005 : 0.00005));
        }
        shape(session, closes);
        const terminal::SessionDate date = terminal::toSessionDate(year_month_day{day});
        const terminal::UnixSeconds open = terminal::usRthUtcWindow("America/New_York", date).start;
        for (std::size_t slot = 0; slot < closes.size(); ++slot)
        {
            terminal::Bar bar;
            bar.instrument_id = 1;
            bar.timeframe_s = terminal::kTimeframe1m;
            bar.ts = open + static_cast<terminal::UnixSeconds>(slot * 60);
            bar.open = slot == 0 ? closes[0] : closes[slot - 1];
            bar.close = closes[slot];
            bar.high = std::max(bar.open, bar.close);
            bar.low = std::min(bar.open, bar.close);
            bar.volume = 1'000;
            bars.push_back(bar);
        }
        day += days{1};
    }
    return bars;
}

// Moves every close from `slot` on by `fraction`: a one-minute shock that stays.
void shiftFrom(std::vector<double>& closes, std::size_t slot, double fraction)
{
    for (std::size_t at = slot; at < closes.size(); ++at)
    {
        closes[at] *= 1.0 + fraction;
    }
}

// A five-session profile, so the sixth session (index 5) is the first that can trade.
std::vector<int> shockOptions(int hold, int direction)
{
    return {5, 40, 5, 30, 50, 100, hold, direction};
}

bool sameTarget(double left, double right)
{
    return (std::isnan(left) && std::isnan(right)) || left == right;
}

const terminal::StrategyType& requireStrategy(std::string_view id)
{
    const terminal::StrategyType* type = terminal::findStrategy(id);
    REQUIRE(type != nullptr);
    return *type;
}

}  // namespace

TEST_CASE("the built-in strategies are registered")
{
    CHECK(terminal::findStrategy("ma_cross") != nullptr);
    CHECK(terminal::findStrategy("bollinger_revert") != nullptr);
    CHECK(terminal::findStrategy("momentum") != nullptr);
    CHECK(terminal::findStrategy("shock_revert") != nullptr);
    CHECK(terminal::findStrategy("nope") == nullptr);
    CHECK(terminal::strategyTypes().size() >= 4);
}

TEST_CASE("no registered strategy looks ahead")
{
    const std::vector<terminal::Bar> bars = walkBars(240);
    for (const terminal::StrategyType* type : terminal::strategyTypes())
    {
        CAPTURE(type->id);
        // Both directions, where the strategy has a direction input.
        for (int direction = 0; direction < 2; ++direction)
        {
            std::vector<int> options = terminal::clampStrategyOptions(*type, {});
            for (std::size_t index = 0; index < type->options.size(); ++index)
            {
                if (std::string_view{type->options[index].key} == "direction")
                {
                    options[index] = direction;
                }
            }
            const std::vector<double> full = terminal::strategyTargets(*type, bars, options);
            REQUIRE(full.size() == bars.size());
            for (const std::size_t cut : {std::size_t{1}, std::size_t{25}, std::size_t{61}, std::size_t{150}})
            {
                const std::span<const terminal::Bar> prefix(bars.data(), cut);
                const std::vector<double> partial = terminal::strategyTargets(*type, prefix, options);
                REQUIRE(partial.size() == cut);
                for (std::size_t index = 0; index < cut; ++index)
                {
                    CAPTURE(cut, index);
                    CHECK(sameTarget(partial[index], full[index]));
                }
            }
        }
    }
}

TEST_CASE("strategy options clamp and round trip through JSON")
{
    const auto& type = requireStrategy("ma_cross");
    const std::vector<int> defaults = terminal::clampStrategyOptions(type, {});
    CHECK(defaults == std::vector<int>{10, 30, 0});
    const std::vector<int> wild{0, 99'999, 7};
    CHECK(terminal::clampStrategyOptions(type, wild) == std::vector<int>{1, 10'000, 0});

    const std::vector<int> options{5, 50, 1};
    const std::string json = terminal::strategyParamsJson(type, options);
    CHECK(json == R"({"fast":5,"slow":50,"direction":"long_short"})");
    const auto back = terminal::strategyOptionsFromJson(type, json);
    REQUIRE(back.has_value());
    CHECK(*back == options);

    const auto partial = terminal::strategyOptionsFromJson(type, R"({"slow":40,"extra":1})");
    REQUIRE(partial.has_value());
    CHECK(*partial == std::vector<int>{10, 40, 0});
    CHECK_FALSE(terminal::strategyOptionsFromJson(type, R"({"direction":"sideways"})").has_value());
    CHECK_FALSE(terminal::strategyOptionsFromJson(type, "[1,2]").has_value());
    CHECK_FALSE(terminal::strategyOptionsFromJson(type, "{nope").has_value());

    // An integer past the stored type's range saturates into the option instead of
    // narrowing to a negative, which would have clamped to the option's minimum.
    const auto huge = terminal::strategyOptionsFromJson(type, R"({"fast":18446744073709551615})");
    REQUIRE(huge.has_value());
    CHECK(*huge == std::vector<int>{10'000, 30, 0});
}

TEST_CASE("a moving average crossover follows the trend")
{
    const auto& type = requireStrategy("ma_cross");
    std::vector<terminal::Bar> bars;
    for (int day = 0; day < 40; ++day)
    {
        terminal::Bar bar;
        bar.instrument_id = 1;
        bar.timeframe_s = terminal::kTimeframe1d;
        bar.ts = 1'780'320'600 + (static_cast<terminal::UnixSeconds>(day) * 86'400);
        bar.close = day < 20 ? 100.0 + day : 120.0 - (2.0 * (day - 20));
        bar.open = bar.close;
        bar.high = bar.close;
        bar.low = bar.close;
        bars.push_back(bar);
    }
    const std::vector<int> long_only{3, 6, 0};
    const auto targets = terminal::strategyTargets(type, bars, long_only);
    CHECK(std::isnan(targets[4]));  // the slow average needs six bars
    CHECK(targets[10] == 1.0);
    CHECK(targets[39] == 0.0);
    const std::vector<int> both{3, 6, 1};
    CHECK(terminal::strategyTargets(type, bars, both)[39] == -1.0);
}

TEST_CASE("bollinger reversion buys below the band and exits at the middle")
{
    const auto& type = requireStrategy("bollinger_revert");
    std::vector<terminal::Bar> bars;
    const std::vector<double> closes{100, 101, 100, 101, 100, 101, 100, 90, 95, 101, 100};
    for (std::size_t day = 0; day < closes.size(); ++day)
    {
        terminal::Bar bar;
        bar.instrument_id = 1;
        bar.timeframe_s = terminal::kTimeframe1d;
        bar.ts = 1'780'320'600 + (static_cast<terminal::UnixSeconds>(day) * 86'400);
        bar.close = closes[day];
        bar.open = bar.close;
        bar.high = bar.close;
        bar.low = bar.close;
        bars.push_back(bar);
    }
    // One deviation: in a five-bar window one close can reach at most 1.79 deviations.
    const std::vector<int> options{5, 1, 0};
    const auto targets = terminal::strategyTargets(type, bars, options);
    CHECK(targets[6] == 0.0);
    CHECK(targets[7] == 1.0);   // 90 closes under the lower band
    CHECK(targets[8] == 1.0);   // 95 is still under the middle
    CHECK(targets[9] == 0.0);   // 101 reaches the middle
}

TEST_CASE("momentum goes long above the threshold and holds inside it")
{
    const auto& type = requireStrategy("momentum");
    std::vector<terminal::Bar> bars;
    const std::vector<double> closes{100, 100, 110, 111, 104, 90};
    for (std::size_t day = 0; day < closes.size(); ++day)
    {
        terminal::Bar bar;
        bar.instrument_id = 1;
        bar.timeframe_s = terminal::kTimeframe1d;
        bar.ts = 1'780'320'600 + (static_cast<terminal::UnixSeconds>(day) * 86'400);
        bar.close = closes[day];
        bar.open = bar.close;
        bar.high = bar.close;
        bar.low = bar.close;
        bars.push_back(bar);
    }
    // One-bar change with a 5% threshold: +10% long, +0.9% no opinion, -6.3% flat, -13.5% flat.
    const std::vector<int> options{1, 50, 0};
    const auto targets = terminal::strategyTargets(type, bars, options);
    CHECK(std::isnan(targets[0]));
    CHECK(std::isnan(targets[1]));
    CHECK(targets[2] == 1.0);
    CHECK(std::isnan(targets[3]));
    CHECK(targets[4] == 0.0);
    CHECK(targets[5] == 0.0);
}

TEST_CASE("shock reversion fades a down-shock and takes profit on the retrace")
{
    const auto& type = requireStrategy("shock_revert");
    const auto bars = minuteSessions(6, [](int session, std::vector<double>& closes) {
        if (session == 5)
        {
            shiftFrom(closes, 91, -0.003);  // 11:01, about 13 sigmas
            shiftFrom(closes, 100, 0.002);  // retraces two thirds of it
        }
    });
    const auto targets = terminal::strategyTargets(type, bars, shockOptions(30, 0));
    REQUIRE(targets.size() == bars.size());
    const std::size_t day = 5 * 390;
    for (std::size_t index = 0; index < day; ++index)
    {
        CAPTURE(index);
        REQUIRE(targets[index] == 0.0);  // the profile is still warming up
    }
    CHECK(targets[day + 90] == 0.0);
    CHECK(targets[day + 91] == 1.0);
    CHECK(targets[day + 99] == 1.0);
    CHECK(targets[day + 100] == 0.0);
    CHECK(targets[day + 389] == 0.0);
}

TEST_CASE("shock reversion stops out and stays out on that side for the session")
{
    const auto& type = requireStrategy("shock_revert");
    const auto bars = minuteSessions(6, [](int session, std::vector<double>& closes) {
        if (session == 5)
        {
            shiftFrom(closes, 91, -0.003);
            shiftFrom(closes, 93, -0.004);   // extends past the whole shock again
            shiftFrom(closes, 200, -0.003);  // a fresh down-shock: the long side is blocked
            shiftFrom(closes, 300, 0.003);   // an up-shock: short when shorts are on
        }
    });
    const std::size_t day = 5 * 390;
    const auto both = terminal::strategyTargets(type, bars, shockOptions(30, 1));
    CHECK(both[day + 92] == 1.0);
    CHECK(both[day + 93] == 0.0);
    CHECK(both[day + 200] == 0.0);
    CHECK(both[day + 300] == -1.0);
    const auto long_only = terminal::strategyTargets(type, bars, shockOptions(30, 0));
    CHECK(long_only[day + 300] == 0.0);
}

TEST_CASE("shock reversion trades only 09:45 to 15:30 and is flat by 15:58")
{
    const auto& type = requireStrategy("shock_revert");
    const auto bars = minuteSessions(7, [](int session, std::vector<double>& closes) {
        if (session == 5)
        {
            shiftFrom(closes, 10, -0.003);   // 09:40
            shiftFrom(closes, 120, -0.003);  // held to the time stop
            shiftFrom(closes, 355, -0.003);  // held to the end of the day
        }
        if (session == 6)
        {
            shiftFrom(closes, 370, -0.003);  // 15:40
        }
    });
    const auto targets = terminal::strategyTargets(type, bars, shockOptions(60, 1));
    const std::size_t day = 5 * 390;
    for (std::size_t slot = 0; slot < 20; ++slot)
    {
        CAPTURE(slot);
        CHECK(targets[day + slot] == 0.0);
    }
    CHECK(targets[day + 120] == 1.0);
    CHECK(targets[day + 179] == 1.0);
    CHECK(targets[day + 180] == 0.0);
    CHECK(targets[day + 355] == 1.0);
    CHECK(targets[day + 387] == 1.0);
    CHECK(targets[day + 388] == 0.0);
    CHECK(targets[day + 389] == 0.0);
    CHECK(targets[day + 390 + 370] == 0.0);
}

TEST_CASE("shock reversion does not look ahead across minute sessions")
{
    const auto& type = requireStrategy("shock_revert");
    const auto bars = minuteSessions(7, [](int session, std::vector<double>& closes) {
        if (session >= 5)
        {
            shiftFrom(closes, 91, -0.003);
        }
        if (session == 6)
        {
            shiftFrom(closes, 240, 0.003);
        }
    });
    const auto options = shockOptions(30, 1);
    const auto full = terminal::strategyTargets(type, bars, options);
    for (const std::size_t cut : {std::size_t{17}, std::size_t{(5 * 390) + 91}, std::size_t{(5 * 390) + 92},
                                  std::size_t{6 * 390}, std::size_t{(6 * 390) + 241}})
    {
        const std::span<const terminal::Bar> prefix(bars.data(), cut);
        const auto partial = terminal::strategyTargets(type, prefix, options);
        for (std::size_t index = 0; index < cut; ++index)
        {
            CAPTURE(cut, index);
            REQUIRE(sameTarget(partial[index], full[index]));
        }
    }
    CHECK(full[(5 * 390) + 91] == 1.0);
    // Yesterday's 11:01 shock is now part of what 11:01 usually does, so the same move is not a shock.
    CHECK(full[(6 * 390) + 91] == 0.0);
    CHECK(full[(6 * 390) + 240] == -1.0);
}

TEST_CASE("shock reversion has no opinion on bars other than one minute")
{
    const auto& type = requireStrategy("shock_revert");
    const auto targets = terminal::strategyTargets(type, walkBars(60), terminal::clampStrategyOptions(type, {}));
    for (const double target : targets)
    {
        CHECK(std::isnan(target));
    }
}
