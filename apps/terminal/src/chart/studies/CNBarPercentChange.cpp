// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/studies/CNBarPercentChange.h"

#include "chart/studies/StudyRegistry.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] const CNBarPercentChange::SourceDesc& describe(CNBarPercentChange::Source source) noexcept
{
    for (const CNBarPercentChange::SourceDesc& desc : CNBarPercentChange::kSources)
    {
        if (desc.source == source)
        {
            return desc;
        }
    }
    return CNBarPercentChange::kSources[0];
}

constexpr bool sourceRowsMatchEnum()
{
    for (std::size_t index = 0; index < std::size(CNBarPercentChange::kSources); ++index)
    {
        if (static_cast<std::size_t>(CNBarPercentChange::kSources[index].source) != index)
        {
            return false;
        }
    }
    return std::size(CNBarPercentChange::kSources) == 4;
}

static_assert(sourceRowsMatchEnum());

}  // namespace

void CNBarPercentChange::clamp(Options& options) noexcept
{
    options.length = std::clamp(options.length, kMinLength, kMaxLength);
}

CNBarPercentChange::CNBarPercentChange() noexcept
    : CNBarPercentChange{Options{}}
{
}

CNBarPercentChange::CNBarPercentChange(Options options) noexcept
    : options_{options}
{
    clamp(options_);
}

const CNBarPercentChange::Options& CNBarPercentChange::options() const noexcept
{
    return options_;
}

void CNBarPercentChange::setOptions(Options options) noexcept
{
    clamp(options);
    options_ = options;
}

std::vector<double> CNBarPercentChange::process(std::span<const Bar> bars) const
{
    const auto count = bars.size();
    std::vector<double> values(count, std::numeric_limits<double>::quiet_NaN());
    const int length{options_.length};
    // The reference bar is `length` steps back, so a series of that length has none.
    if (count == 0 || std::cmp_greater_equal(length, count))
    {
        return values;
    }

    const SourceDesc& desc = describe(options_.source);
    const auto lookback = static_cast<std::size_t>(length);
    for (std::size_t index = lookback; index < count; ++index)
    {
        const double base = bars[index - lookback].*desc.field;
        if (base == 0.0)
        {
            continue;
        }
        const double current = bars[index].*desc.field;
        values[index] = ((current - base) / base) * 100.0;
    }
    return values;
}

std::string CNBarPercentChange::label() const
{
    std::string text = "n% ";
    text += std::to_string(options_.length);
    text += ' ';
    text += describe(options_.source).code;
    return text;
}

namespace {

constexpr StudyChoice kInputChoices[] = {
    {.token = CNBarPercentChange::kSources[0].token, .label = CNBarPercentChange::kSources[0].label},
    {.token = CNBarPercentChange::kSources[1].token, .label = CNBarPercentChange::kSources[1].label},
    {.token = CNBarPercentChange::kSources[2].token, .label = CNBarPercentChange::kSources[2].label},
    {.token = CNBarPercentChange::kSources[3].token, .label = CNBarPercentChange::kSources[3].label},
};

constexpr StudyOption kOptions[] = {
    {
        .key = "length",
        .label = "Length",
        .min = CNBarPercentChange::kMinLength,
        .max = CNBarPercentChange::kMaxLength,
        .fallback = CNBarPercentChange::kDefaultLength,
    },
    {
        .key = "source",
        .label = "Input Data",
        .fallback = static_cast<int>(CNBarPercentChange::Source::Close),
        .choices = kInputChoices,
    },
};

constexpr bool inputChoicesMatchSources()
{
    if (std::size(kInputChoices) != std::size(CNBarPercentChange::kSources))
    {
        return false;
    }
    for (std::size_t index = 0; index < std::size(kInputChoices); ++index)
    {
        if (std::string_view{kInputChoices[index].token} != CNBarPercentChange::kSources[index].token ||
            std::string_view{kInputChoices[index].label} != CNBarPercentChange::kSources[index].label ||
            kInputChoices[index].leave_price_scale)
        {
            return false;
        }
    }
    return true;
}

static_assert(inputChoicesMatchSources());

[[nodiscard]] CNBarPercentChange::Options optionsFrom(std::span<const int> options)
{
    CNBarPercentChange::Options parsed;
    if (!options.empty())
    {
        parsed.length = options[0];
    }
    if (options.size() > 1)
    {
        const int index = options[1];
        if (index >= 0 && static_cast<std::size_t>(index) < std::size(CNBarPercentChange::kSources))
        {
            parsed.source = CNBarPercentChange::kSources[static_cast<std::size_t>(index)].source;
        }
    }
    return parsed;
}

void labelPercentChange(std::span<const int> options, std::string& label)
{
    label = CNBarPercentChange{optionsFrom(options)}.label();
}

void processPercentChange(std::span<const Bar> bars,
                          std::span<const int> options,
                          std::string& label,
                          std::vector<StudyTrace>& traces)
{
    const CNBarPercentChange study{optionsFrom(options)};
    label = study.label();
    traces.push_back(StudyTrace{.label = label, .values = study.process(bars)});
}

constexpr StudyOutput kOutputs[] = {
    {.key = "change", .label = "% Change", .line = StudyLineStyle::Value},
};

static_assert(std::string_view{kOutputs[0].key} == "change");

constexpr StudyType kType{
    .id = "n_bar_percent_change",
    .display_name = "n bar % change",
    .default_chart_region = 2,
    .graph = StudyGraph::Line,
    .anchor_zero = true,
    .value_decimals = 2,
    .note = "Percent change from the bar n bars ago to this bar. Shown as a label of the latest value.",
    .options = kOptions,
    .outputs = kOutputs,
    .process = &processPercentChange,
    .label = &labelPercentChange,
};

struct Registration
{
    Registration() noexcept
    {
        registerStudy(kType);
    }
};

[[maybe_unused]] const Registration kPercentChangeRegistration{};

}  // namespace

}  // namespace terminal
