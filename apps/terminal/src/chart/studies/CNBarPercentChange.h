// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace terminal {

// Percent change of one bar field over `length` chart bars.
// At bar i the value is 100 * (field[i] - field[i - length]) / field[i - length].
// Samples without a reference bar, and a zero reference, are NaN.
// The class knows the bar layout and nothing about charts, files, or other studies.
class CNBarPercentChange
{
public:
    enum class Source : std::uint8_t
    {
        Close = 0,
        Open,
        High,
        Low,
    };

    // source is the enum value and the index of that row.
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
    };

    static constexpr int kDefaultLength = 1;
    static constexpr int kMinLength = 1;
    static constexpr int kMaxLength = 10000;

    struct Options
    {
        int length{kDefaultLength};
        Source source{Source::Close};
    };

    CNBarPercentChange() noexcept;
    explicit CNBarPercentChange(Options options) noexcept;

    [[nodiscard]] const Options& options() const noexcept;
    void setOptions(Options options) noexcept;

    // One value per bar. The first `length` bars are NaN.
    // Empty input is an empty vector. Length at least the series length is all NaN.
    [[nodiscard]] std::vector<double> process(std::span<const Bar> bars) const;

    [[nodiscard]] std::string label() const;

private:
    static void clamp(Options& options) noexcept;

    Options options_;
};

}  // namespace terminal
