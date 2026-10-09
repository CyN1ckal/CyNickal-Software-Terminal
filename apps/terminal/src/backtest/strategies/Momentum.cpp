// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "backtest/Strategy.h"
#include "backtest/strategies/Direction.h"
#include "chart/studies/CNBarPercentChange.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace terminal {
namespace {

constexpr StudyOption kOptions[] = {
    {
        .key = "length",
        .label = "Lookback bars",
        .min = CNBarPercentChange::kMinLength,
        .max = CNBarPercentChange::kMaxLength,
        .fallback = 20,
    },
    {
        .key = "threshold",
        .label = "Threshold (0.1%)",
        .min = 0,
        .max = 1000,
        .fallback = 0,
    },
    {
        .key = "direction",
        .label = "Direction",
        .choices = kDirectionChoices,
        .fallback = 0,
    },
};

// Long when the close is up more than the threshold over the lookback. Down more
// than the threshold is short, or flat without shorts. In between keeps the last target.
void processMomentum(std::span<const Bar> bars, std::span<const int> options, std::vector<double>& targets)
{
    CNBarPercentChange::Options change_options;
    change_options.length = options[0];
    const std::vector<double> change = CNBarPercentChange{change_options}.process(bars);
    const double threshold = static_cast<double>(options[1]) / 10.0;
    const double down = directionAllowsShort(options[2]) ? -1.0 : 0.0;
    for (std::size_t index = 0; index < bars.size(); ++index)
    {
        const double percent = change[index];
        if (!std::isfinite(percent))
        {
            targets.push_back(std::numeric_limits<double>::quiet_NaN());
        }
        else if (percent > threshold)
        {
            targets.push_back(1.0);
        }
        else if (percent < -threshold)
        {
            targets.push_back(down);
        }
        else
        {
            targets.push_back(std::numeric_limits<double>::quiet_NaN());
        }
    }
}

constexpr StrategyType kType{
    .id = "momentum",
    .display_name = "N-bar momentum",
    .note = "Long after the close rises more than the threshold over the lookback; after a fall, short or flat.",
    .options = kOptions,
    .process = &processMomentum,
};

struct Registration
{
    Registration() noexcept
    {
        registerStrategy(kType);
    }
};

[[maybe_unused]] const Registration kMomentumRegistration{};

}  // namespace
}  // namespace terminal
