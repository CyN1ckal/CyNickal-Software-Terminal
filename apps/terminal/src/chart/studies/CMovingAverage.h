// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace terminal {

// Moving average of one bar field. Length counts chart bars.
// Simple is the window sum divided by length. Exponential seeds with that
// simple average, then uses k = 2 / (length + 1): k * price + (1 - k) * previous.
// Weighted sums weights 1..length from oldest to newest and divides by
// length * (length + 1) / 2. All three are NaN until the window is full.
// The class knows the bar layout and nothing about charts, files, or other studies.
class CMovingAverage
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

    // Chartbook tokens, in stored option order: simple, exponential, weighted.
    enum class Method : std::uint8_t
    {
        Simple = 0,
        Exponential,
        Weighted,
    };

    // source is the enum value and the index of that row.
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

    struct Options
    {
        int length{kDefaultLength};
        Source source{Source::Close};
        Method method{Method::Simple};
    };

    CMovingAverage() noexcept;
    explicit CMovingAverage(Options options) noexcept;

    [[nodiscard]] const Options& options() const noexcept;
    void setOptions(Options options) noexcept;

    // One value per bar. Samples before `length` bars exist are NaN.
    // Empty input is an empty vector. Length past the series is all NaN.
    [[nodiscard]] std::vector<double> process(std::span<const Bar> bars) const;

    [[nodiscard]] std::string label() const;

private:
    static void clamp(Options& options) noexcept;

    Options options_;
};

}  // namespace terminal
