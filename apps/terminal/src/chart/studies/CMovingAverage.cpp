// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/studies/CMovingAverage.h"

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

// k = 2 / (length + 1). The first full window of finite samples is an SMA seed, not Wilder.
// A non-finite sample would poison every later value, so it drops the seed and the next
// length finite samples start again.
void exponentialAverage(std::span<const Bar> bars, double Bar::* field, int length, std::span<double> values)
{
    const auto window = static_cast<std::size_t>(length);
    const double k = 2.0 / static_cast<double>(length + 1);
    double sum = 0.0;
    double ema = 0.0;
    std::size_t finite_run = 0;
    bool seeded = false;
    for (std::size_t index = 0; index < bars.size(); ++index)
    {
        const double price = bars[index].*field;
        if (!std::isfinite(price))
        {
            seeded = false;
            finite_run = 0;
            sum = 0.0;
            continue;
        }
        if (!seeded)
        {
            sum += price;
            ++finite_run;
            if (finite_run < window)
            {
                continue;
            }
            ema = sum / static_cast<double>(length);
            values[index] = ema;
            seeded = true;
            sum = 0.0;
            finite_run = 0;
            continue;
        }
        ema = (k * price) + ((1.0 - k) * ema);
        values[index] = ema;
    }
}

// Weights 1..length from the oldest sample to the newest.
void weightedAverage(std::span<const Bar> bars, double Bar::* field, int length, std::span<double> values)
{
    const auto window = static_cast<std::size_t>(length);
    const double divisor = static_cast<double>(length) * static_cast<double>(length + 1) / 2.0;
    for (std::size_t index = 0; index < bars.size(); ++index)
    {
        if (index + 1 < window)
        {
            continue;
        }
        double weighted = 0.0;
        const std::size_t start = index + 1 - window;
        for (std::size_t offset = 0; offset < window; ++offset)
        {
            const auto weight = static_cast<double>(offset + 1);
            weighted += weight * bars[start + offset].*field;
        }
        values[index] = weighted / divisor;
    }
}

void simpleAverage(std::span<const Bar> bars, double Bar::* field, int length, std::span<double> values)
{
    const auto window = static_cast<std::size_t>(length);
    // The running sum stays exact while bars are finite. One inf or NaN bar would poison every
    // later value, so a window that takes in or drops a non-finite sample re-sums itself.
    double sum = 0.0;
    for (std::size_t index = 0; index < bars.size(); ++index)
    {
        const double sample = bars[index].*field;
        const double leaving = index >= window ? bars[index - window].*field : 0.0;
        if (std::isfinite(sample) && (index < window || std::isfinite(leaving)))
        {
            sum += sample;
            if (index >= window)
            {
                sum -= leaving;
            }
        }
        else
        {
            sum = 0.0;
            const std::size_t first = index + 1 < window ? 0 : index - window + 1;
            for (std::size_t at = first; at <= index; ++at)
            {
                sum += bars[at].*field;
            }
        }
        if (index + 1 >= window)
        {
            values[index] = sum / static_cast<double>(length);
        }
    }
}

}  // namespace

void CMovingAverage::clamp(Options& options) noexcept
{
    options.length = std::clamp(options.length, kMinLength, kMaxLength);
    if (options.method != Method::Simple && options.method != Method::Exponential &&
        options.method != Method::Weighted)
    {
        options.method = Method::Simple;
    }
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

    const auto field = describe(options_.source).field;
    if (options_.method == Method::Exponential)
    {
        exponentialAverage(bars, field, length, values);
    }
    else if (options_.method == Method::Weighted)
    {
        weightedAverage(bars, field, length, values);
    }
    else
    {
        simpleAverage(bars, field, length, values);
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

// Chartbooks store simple, exponential, or weighted. Chart Settings shows the choice.
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
        .choices = kInputChoices,
        .fallback = static_cast<int>(CMovingAverage::Source::Close),
    },
    {
        .key = "method",
        .label = "Method",
        .choices = kMethods,
        .fallback = 0,
        .shown = true,
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

constexpr bool methodChoicesMatchEnum()
{
    return std::size(kMethods) == 3 &&
           static_cast<int>(CMovingAverage::Method::Simple) == 0 &&
           static_cast<int>(CMovingAverage::Method::Exponential) == 1 &&
           static_cast<int>(CMovingAverage::Method::Weighted) == 2 &&
           std::string_view{kMethods[static_cast<std::size_t>(CMovingAverage::Method::Simple)].token} == "simple" &&
           std::string_view{kMethods[static_cast<std::size_t>(CMovingAverage::Method::Exponential)].token} ==
               "exponential" &&
           std::string_view{kMethods[static_cast<std::size_t>(CMovingAverage::Method::Weighted)].token} == "weighted";
}

static_assert(methodChoicesMatchEnum());

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
    if (options.size() > 2)
    {
        const int index = options[2];
        if (index == static_cast<int>(CMovingAverage::Method::Simple))
        {
            parsed.method = CMovingAverage::Method::Simple;
        }
        else if (index == static_cast<int>(CMovingAverage::Method::Exponential))
        {
            parsed.method = CMovingAverage::Method::Exponential;
        }
        else if (index == static_cast<int>(CMovingAverage::Method::Weighted))
        {
            parsed.method = CMovingAverage::Method::Weighted;
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
    .options = kOptions,
    .outputs = kOutputs,
    .process = &processMovingAverage,
    .label = &labelMovingAverage,
    .default_chart_region = 1,
    .value_decimals = 4,
    .graph = StudyGraph::Line,
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
