// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/studies/CBollinger.h"

#include "chart/studies/StudyRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] const CBollinger::SourceDesc& describe(CBollinger::Source source) noexcept
{
    for (const CBollinger::SourceDesc& desc : CBollinger::kSources)
    {
        if (desc.source == source)
        {
            return desc;
        }
    }
    return CBollinger::kSources[0];
}

constexpr bool sourceRowsMatchEnum()
{
    for (std::size_t index = 0; index < std::size(CBollinger::kSources); ++index)
    {
        if (static_cast<std::size_t>(CBollinger::kSources[index].source) != index)
        {
            return false;
        }
    }
    return std::size(CBollinger::kSources) == 5;
}

static_assert(sourceRowsMatchEnum());

}  // namespace

void CBollinger::clamp(Options& options) noexcept
{
    options.length = std::clamp(options.length, kMinLength, kMaxLength);
    options.deviations = std::clamp(options.deviations, kMinDeviations, kMaxDeviations);
}

CBollinger::CBollinger() noexcept
    : CBollinger{Options{}}
{
}

CBollinger::CBollinger(Options options) noexcept
    : options_{options}
{
    clamp(options_);
}

const CBollinger::Options& CBollinger::options() const noexcept
{
    return options_;
}

void CBollinger::setOptions(Options options) noexcept
{
    clamp(options);
    options_ = options;
}

CBollinger::Bands CBollinger::process(std::span<const Bar> bars) const
{
    const auto count = bars.size();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Bands bands;
    bands.upper.assign(count, nan);
    bands.middle.assign(count, nan);
    bands.lower.assign(count, nan);
    const int length{options_.length};
    if (count == 0 || std::cmp_greater(length, count))
    {
        return bands;
    }

    const SourceDesc& desc = describe(options_.source);
    const auto window = static_cast<std::size_t>(length);
    const auto samples = static_cast<double>(length);
    const auto width = static_cast<double>(options_.deviations);
    double sum = 0.0;
    double sum_sq = 0.0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const double sample = bars[index].*desc.field;
        sum += sample;
        sum_sq += sample * sample;
        if (index >= window)
        {
            const double leaving = bars[index - window].*desc.field;
            sum -= leaving;
            sum_sq -= leaving * leaving;
        }
        if (index + 1 < window)
        {
            continue;
        }
        const double mean = sum / samples;
        const double variance = std::max((sum_sq / samples) - (mean * mean), 0.0);
        const double offset = std::sqrt(variance) * width;
        bands.middle[index] = mean;
        bands.upper[index] = mean + offset;
        bands.lower[index] = mean - offset;
    }
    return bands;
}

std::string CBollinger::label() const
{
    std::string text = "BB ";
    text += std::to_string(options_.length);
    text += ' ';
    text += std::to_string(options_.deviations);
    text += ' ';
    text += describe(options_.source).code;
    return text;
}

namespace {

constexpr StudyChoice kInputChoices[] = {
    {.token = CBollinger::kSources[0].token, .label = CBollinger::kSources[0].label},
    {.token = CBollinger::kSources[1].token, .label = CBollinger::kSources[1].label},
    {.token = CBollinger::kSources[2].token, .label = CBollinger::kSources[2].label},
    {.token = CBollinger::kSources[3].token, .label = CBollinger::kSources[3].label},
    {.token = CBollinger::kSources[4].token,
     .label = CBollinger::kSources[4].label,
     .leave_price_scale = true,},
};

constexpr StudyOption kOptions[] = {
    {
        .key = "length",
        .label = "Length",
        .min = CBollinger::kMinLength,
        .max = CBollinger::kMaxLength,
        .fallback = CBollinger::kDefaultLength,
    },
    {
        .key = "source",
        .label = "Input Data",
        .fallback = static_cast<int>(CBollinger::Source::Close),
        .choices = kInputChoices,
    },
    {
        .key = "deviations",
        .label = "Deviations",
        .min = CBollinger::kMinDeviations,
        .max = CBollinger::kMaxDeviations,
        .fallback = CBollinger::kDefaultDeviations,
    },
};

constexpr bool inputChoicesMatchSources()
{
    if (std::size(kInputChoices) != std::size(CBollinger::kSources))
    {
        return false;
    }
    for (std::size_t index = 0; index < std::size(kInputChoices); ++index)
    {
        if (std::string_view{kInputChoices[index].token} != CBollinger::kSources[index].token ||
            std::string_view{kInputChoices[index].label} != CBollinger::kSources[index].label)
        {
            return false;
        }
    }
    return kInputChoices[static_cast<std::size_t>(CBollinger::Source::Volume)].leave_price_scale &&
           !kInputChoices[static_cast<std::size_t>(CBollinger::Source::Close)].leave_price_scale;
}

static_assert(inputChoicesMatchSources());

[[nodiscard]] CBollinger::Options optionsFrom(std::span<const int> options)
{
    CBollinger::Options parsed;
    if (!options.empty())
    {
        parsed.length = options[0];
    }
    if (options.size() > 1)
    {
        const int index = options[1];
        if (index >= 0 && static_cast<std::size_t>(index) < std::size(CBollinger::kSources))
        {
            parsed.source = CBollinger::kSources[static_cast<std::size_t>(index)].source;
        }
    }
    if (options.size() > 2)
    {
        parsed.deviations = options[2];
    }
    return parsed;
}

void labelBollinger(std::span<const int> options, std::string& label)
{
    label = CBollinger{optionsFrom(options)}.label();
}

void processBollinger(std::span<const Bar> bars,
                      std::span<const int> options,
                      std::string& label,
                      std::vector<StudyTrace>& traces)
{
    const CBollinger study{optionsFrom(options)};
    label = study.label();
    CBollinger::Bands bands = study.process(bars);
    // Order matches kOutputs: upper, middle, lower.
    traces.push_back(StudyTrace{.label = label + " U", .values = std::move(bands.upper)});
    traces.push_back(StudyTrace{.label = label + " M", .values = std::move(bands.middle)});
    traces.push_back(StudyTrace{.label = label + " L", .values = std::move(bands.lower)});
}

constexpr StudyOutput kOutputs[] = {
    {.key = "upper", .label = "Upper Band"},
    {.key = "middle", .label = "Middle Band"},
    {.key = "lower", .label = "Lower Band"},
};

static_assert(std::string_view{kOutputs[0].key} == "upper");
static_assert(std::string_view{kOutputs[1].key} == "middle");
static_assert(std::string_view{kOutputs[2].key} == "lower");

constexpr StudyType kType{
    .id = "bollinger",
    .display_name = "Bollinger Bands",
    .default_chart_region = 1,
    .graph = StudyGraph::Line,
    .value_decimals = 4,
    .options = kOptions,
    .outputs = kOutputs,
    .process = &processBollinger,
    .label = &labelBollinger,
};

struct Registration
{
    Registration() noexcept
    {
        registerStudy(kType);
    }
};

[[maybe_unused]] const Registration kBollingerRegistration{};

}  // namespace

}  // namespace terminal
