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
        const char* token;
        const char* label;
        double Bar::* field;
        Source source;
        char code;
    };

    static constexpr SourceDesc kSources[] = {
        {.token = "close", .label = "Close", .field = &Bar::close, .source = Source::Close, .code = 'C'},
        {.token = "open", .label = "Open", .field = &Bar::open, .source = Source::Open, .code = 'O'},
        {.token = "high", .label = "High", .field = &Bar::high, .source = Source::High, .code = 'H'},
        {.token = "low", .label = "Low", .field = &Bar::low, .source = Source::Low, .code = 'L'},
        {.token = "volume", .label = "Volume", .field = &Bar::volume, .source = Source::Volume, .code = 'V'},
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
