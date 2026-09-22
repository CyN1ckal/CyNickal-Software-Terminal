// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/studies/CMovingAverage.h"

#include "chart/studies/StudyRegistry.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] const CMovingAverage::SourceDesc& describe(CMovingAverage::Source source) noexcept
{
    for (const CMovingAverage::SourceDesc& desc : CMovingAverage::kSources)
    {
        if (desc.source == source)
        {
            return desc;
        }
    }
    return CMovingAverage::kSources[0];
}

constexpr bool sourceRowsMatchEnum()
{
    for (std::size_t index = 0; index < std::size(CMovingAverage::kSources); ++index)
    {
        if (static_cast<std::size_t>(CMovingAverage::kSources[index].source) != index)
        {
            return false;
        }
    }
    return std::size(CMovingAverage::kSources) == 5;
}

static_assert(sourceRowsMatchEnum());

}  // namespace

void CMovingAverage::clamp(Options& options) noexcept
{
    options.length = std::clamp(options.length, kMinLength, kMaxLength);
}

CMovingAverage::CMovingAverage() noexcept
    : CMovingAverage{Options{}}
{
}

CMovingAverage::CMovingAverage(Options options) noexcept
    : options_{options}
{
    clamp(options_);
}

const CMovingAverage::Options& CMovingAverage::options() const noexcept
{
    return options_;
}

void CMovingAverage::setOptions(Options options) noexcept
{
    clamp(options);
    options_ = options;
}

std::vector<double> CMovingAverage::process(std::span<const Bar> bars) const
{
    const auto count = bars.size();
    std::vector<double> values(count, std::numeric_limits<double>::quiet_NaN());
    const int length{options_.length};
    if (count == 0 || std::cmp_greater(length, count))
    {
        return values;
    }

    const SourceDesc& desc = describe(options_.source);
    const auto window = static_cast<std::size_t>(length);
    double sum = 0.0;
    for (std::size_t index = 0; index < count; ++index)
    {
        sum += bars[index].*desc.field;
        if (index >= window)
        {
            sum -= bars[index - window].*desc.field;
        }
        if (index + 1 >= window)
        {
            values[index] = sum / static_cast<double>(length);
        }
    }
    return values;
}

std::string CMovingAverage::label() const
{
    std::string text = "MA ";
    text += std::to_string(options_.length);
    text += ' ';
    text += describe(options_.source).code;
    return text;
}

namespace {

constexpr StudyChoice kInputChoices[] = {
    {.token = CMovingAverage::kSources[0].token, .label = CMovingAverage::kSources[0].label},
    {.token = CMovingAverage::kSources[1].token, .label = CMovingAverage::kSources[1].label},
    {.token = CMovingAverage::kSources[2].token, .label = CMovingAverage::kSources[2].label},
    {.token = CMovingAverage::kSources[3].token, .label = CMovingAverage::kSources[3].label},
    {.token = CMovingAverage::kSources[4].token,
     .label = CMovingAverage::kSources[4].label,
     .leave_price_scale = true,},
};

// Chartbooks already store a method. The average is always simple, so this choice is kept and not shown.
constexpr StudyChoice kMethods[] = {
    {.token = "simple", .label = "Simple"},
    {.token = "exponential", .label = "Exponential"},
    {.token = "weighted", .label = "Weighted"},
};

constexpr StudyOption kOptions[] = {
    {
        .key = "length",
        .label = "Length",
        .min = CMovingAverage::kMinLength,
        .max = CMovingAverage::kMaxLength,
        .fallback = CMovingAverage::kDefaultLength,
    },
    {
        .key = "source",
        .label = "Input Data",
        .fallback = static_cast<int>(CMovingAverage::Source::Close),
        .choices = kInputChoices,
    },
    {
        .key = "method",
        .label = "Method",
        .fallback = 0,
        .choices = kMethods,
        .shown = false,
    },
};

constexpr bool inputChoicesMatchSources()
{
    if (std::size(kInputChoices) != std::size(CMovingAverage::kSources))
    {
        return false;
    }
    for (std::size_t index = 0; index < std::size(kInputChoices); ++index)
    {
        if (std::string_view{kInputChoices[index].token} != CMovingAverage::kSources[index].token ||
            std::string_view{kInputChoices[index].label} != CMovingAverage::kSources[index].label)
        {
            return false;
        }
    }
    return kInputChoices[static_cast<std::size_t>(CMovingAverage::Source::Volume)].leave_price_scale &&
           !kInputChoices[static_cast<std::size_t>(CMovingAverage::Source::Close)].leave_price_scale;
}

static_assert(inputChoicesMatchSources());

[[nodiscard]] CMovingAverage::Options optionsFrom(std::span<const int> options)
{
    CMovingAverage::Options parsed;
    if (!options.empty())
    {
        parsed.length = options[0];
    }
    if (options.size() > 1)
    {
        const int index = options[1];
        if (index >= 0 && static_cast<std::size_t>(index) < std::size(CMovingAverage::kSources))
        {
            parsed.source = CMovingAverage::kSources[static_cast<std::size_t>(index)].source;
        }
    }
    return parsed;
}

void labelMovingAverage(std::span<const int> options, std::string& label)
{
    label = CMovingAverage{optionsFrom(options)}.label();
}

void processMovingAverage(std::span<const Bar> bars,
                          std::span<const int> options,
                          std::string& label,
                          std::vector<StudyTrace>& traces)
{
    const CMovingAverage average{optionsFrom(options)};
    label = average.label();
    traces.push_back(StudyTrace{.label = label, .values = average.process(bars)});
}

constexpr StudyOutput kOutputs[] = {
    {.key = "average", .label = "Moving Average"},
};

constexpr StudyType kType{
    .id = "moving_average",
    .display_name = "Moving Average",
    .default_chart_region = 1,
    .graph = StudyGraph::Line,
    .value_decimals = 4,
    .options = kOptions,
    .outputs = kOutputs,
    .process = &processMovingAverage,
    .label = &labelMovingAverage,
};

struct Registration
{
    Registration() noexcept
    {
        registerStudy(kType);
    }
};

[[maybe_unused]] const Registration kMovingAverageRegistration{};

}  // namespace

}  // namespace terminal
