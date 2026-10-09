// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "backtest/Strategy.h"
#include "backtest/strategies/Direction.h"
#include "chart/studies/CBollinger.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace terminal {
namespace {

constexpr StudyOption kOptions[] = {
    {
        .key = "length",
        .label = "Length",
        .min = CBollinger::kMinLength,
        .max = CBollinger::kMaxLength,
        .fallback = CBollinger::kDefaultLength,
    },
    {
        .key = "deviations",
        .label = "Deviations",
        .min = CBollinger::kMinDeviations,
        .max = CBollinger::kMaxDeviations,
        .fallback = CBollinger::kDefaultDeviations,
    },
    {
        .key = "direction",
        .label = "Direction",
        .choices = kDirectionChoices,
        .fallback = 0,
    },
};

// Buys a close under the lower band and sells it back at the middle band. With
// shorts on, sells a close over the upper band and covers at the middle band.
void processBollingerRevert(std::span<const Bar> bars, std::span<const int> options, std::vector<double>& targets)
{
    CBollinger::Options bands_options;
    bands_options.length = options[0];
    bands_options.deviations = options[1];
    const CBollinger::Bands bands = CBollinger{bands_options}.process(bars);
    const bool shorts = directionAllowsShort(options[2]);
    double held = 0.0;
    for (std::size_t index = 0; index < bars.size(); ++index)
    {
        const double close = bars[index].close;
        const double lower = bands.lower[index];
        const double middle = bands.middle[index];
        const double upper = bands.upper[index];
        if (!std::isfinite(lower) || !std::isfinite(middle) || !std::isfinite(upper))
        {
            targets.push_back(held);
            continue;
        }
        const bool back_to_middle = (held > 0.0 && close >= middle) || (held < 0.0 && close <= middle);
        if (back_to_middle)
        {
            held = 0.0;
        }
        if (held == 0.0)
        {
            if (close < lower)
            {
                held = 1.0;
            }
            else if (shorts && close > upper)
            {
                held = -1.0;
            }
        }
        targets.push_back(held);
    }
}

constexpr StrategyType kType{
    .id = "bollinger_revert",
    .display_name = "Bollinger band reversion",
    .note = "Buys a close under the lower band and exits at the middle band; with shorts, the mirror at the upper "
            "band.",
    .options = kOptions,
    .process = &processBollingerRevert,
};

struct Registration
{
    Registration() noexcept
    {
        registerStrategy(kType);
    }
};

[[maybe_unused]] const Registration kBollingerRevertRegistration{};

}  // namespace
}  // namespace terminal
