// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "backtest/Strategy.h"
#include "backtest/strategies/Direction.h"
#include "chart/studies/CMovingAverage.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace terminal {
namespace {

constexpr StudyOption kOptions[] = {
    {
        .key = "fast",
        .label = "Fast length",
        .min = CMovingAverage::kMinLength,
        .max = CMovingAverage::kMaxLength,
        .fallback = 10,
    },
    {
        .key = "slow",
        .label = "Slow length",
        .min = CMovingAverage::kMinLength,
        .max = CMovingAverage::kMaxLength,
        .fallback = 30,
    },
    {
        .key = "direction",
        .label = "Direction",
        .fallback = 0,
        .choices = kDirectionChoices,
    },
};

// Long while the fast close average is above the slow one. Below it, short or flat.
void processMaCross(std::span<const Bar> bars, std::span<const int> options, std::vector<double>& targets)
{
    CMovingAverage::Options fast_options;
    fast_options.length = options[0];
    CMovingAverage::Options slow_options;
    slow_options.length = options[1];
    const std::vector<double> fast = CMovingAverage{fast_options}.process(bars);
    const std::vector<double> slow = CMovingAverage{slow_options}.process(bars);
    const double below = directionAllowsShort(options[2]) ? -1.0 : 0.0;
    for (std::size_t index = 0; index < bars.size(); ++index)
    {
        if (!std::isfinite(fast[index]) || !std::isfinite(slow[index]))
        {
            targets.push_back(std::numeric_limits<double>::quiet_NaN());
            continue;
        }
        targets.push_back(fast[index] > slow[index] ? 1.0 : below);
    }
}

constexpr StrategyType kType{
    .id = "ma_cross",
    .display_name = "Moving average crossover",
    .note = "Long while the fast average of closes is above the slow one; below it, short or flat.",
    .options = kOptions,
    .process = &processMaCross,
};

struct Registration
{
    Registration() noexcept
    {
        registerStrategy(kType);
    }
};

[[maybe_unused]] const Registration kMaCrossRegistration{};

}  // namespace
}  // namespace terminal
