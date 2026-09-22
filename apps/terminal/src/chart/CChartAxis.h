// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartView.h"
#include "market_data/Types.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// Local civil time in an IANA zone. Empty or unknown zones fall back to UTC.
struct ChartLocalTime
{
    int year{};
    int month{};
    int day{};
    int hour{};
    int minute{};
};

[[nodiscard]] ChartLocalTime chartLocalTime(std::string_view timezone, UnixSeconds ts);

// Pixel width of each label form, plus the clear gap between neighbors.
// spacing_px is pixels per bar (index X, not wall-clock X).
struct ChartTickMetrics
{
    float spacing_px{8.0f};
    float gap_px{8.0f};
    float time_px{40.0f};   // "HH:MM"
    float date_px{72.0f};   // "YYYY-MM-DD"
    float month_px{56.0f};  // "YYYY-MM"
    float year_px{32.0f};   // "YYYY"
};

struct ChartAxisTick
{
    double x{};
    std::string label;
};

// Finest clock or calendar grain whose labels clear one another.
// Intraday: HH:MM on a round step from the session open, YYYY-MM-DD on that
// open when the wider text still fits. Otherwise session, week, month,
// quarter, then year. A label that would run into the price scale is omitted.
[[nodiscard]] std::vector<ChartAxisTick> buildChartTimeTicks(std::span<const Bar> bars,
                                                             const ChartVisibleWindow& win,
                                                             std::string_view timezone,
                                                             const ChartTickMetrics& metrics);

}  // namespace terminal
