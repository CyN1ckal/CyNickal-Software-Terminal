// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace terminal {

// Middle band is a simple average. Upper and lower sit `deviations` population
// standard deviations away (the sum of squares is divided by length, not length-1).
class CBollinger
{
public:
    enum class Source : std::uint8_t
    {
        Close = 0,
        Open,
        High,
        Low,
        Volume,
    };

    struct SourceDesc
    {
        Source source;
        const char* token;
        const char* label;
        char code;
        double Bar::* field;
    };

    static constexpr SourceDesc kSources[] = {
        {.source = Source::Close, .token = "close", .label = "Close", .code = 'C', .field = &Bar::close},
        {.source = Source::Open, .token = "open", .label = "Open", .code = 'O', .field = &Bar::open},
        {.source = Source::High, .token = "high", .label = "High", .code = 'H', .field = &Bar::high},
        {.source = Source::Low, .token = "low", .label = "Low", .code = 'L', .field = &Bar::low},
        {.source = Source::Volume, .token = "volume", .label = "Volume", .code = 'V', .field = &Bar::volume},
    };

    static constexpr int kDefaultLength = 20;
    static constexpr int kMinLength = 1;
    static constexpr int kMaxLength = 10000;
    static constexpr int kDefaultDeviations = 2;
    static constexpr int kMinDeviations = 1;
    static constexpr int kMaxDeviations = 10;

    struct Options
    {
        int length{kDefaultLength};
        Source source{Source::Close};
        int deviations{kDefaultDeviations};
    };

    struct Bands
    {
        std::vector<double> upper;
        std::vector<double> middle;
        std::vector<double> lower;
    };

    CBollinger() noexcept;
    explicit CBollinger(Options options) noexcept;

    [[nodiscard]] const Options& options() const noexcept;
    void setOptions(Options options) noexcept;

    // Three samples per bar, each the same length as bars. Warmup slots are NaN.
    // Empty input yields three empty vectors. Length past the series is all NaN.
    [[nodiscard]] Bands process(std::span<const Bar> bars) const;

    [[nodiscard]] std::string label() const;

private:
    static void clamp(Options& options) noexcept;

    Options options_;
};

}  // namespace terminal
