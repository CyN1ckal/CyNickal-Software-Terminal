// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartSettings.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace terminal {

// Keystrokes typed on a focused chart. Enter submits the buffer.
// A supported period (1m, 5m, 15m, 1h, 1d) changes the bar period.
// Anything else is a symbol, including a name that has not been downloaded.
enum class ChartCommandKind : std::uint8_t
{
    Empty,
    Symbol,
    Period,
    Rejected
};

struct ChartCommand
{
    ChartCommandKind kind{ChartCommandKind::Empty};
    std::string symbol;
    ChartBarPeriod period{ChartBarPeriod::Minute1};
    std::string message;
};

// `current` supplies the unit for a bare number (15 on a minute chart is 15m).
// A leading '/' forces a symbol. Blank input is Empty.
[[nodiscard]] ChartCommand parseChartCommand(std::string_view text, ChartBarPeriod current);

// Calendar days to request so Days to Load can fill. Intraday is at least 21 days.
// Daily walks NYSE sessions from `today` and is at least the shared five-year preset.
[[nodiscard]] int chartDownloadLookbackDays(const CChartSettings& settings, SessionDate today) noexcept;

// Symbol, source timeframe, and [from, to] for one ingest job. Does not read the store.
// Intraday periods request 1-minute bars. Day1 requests daily bars.
struct ChartDownloadRequest
{
    std::string symbol;
    SessionDate from{};
    SessionDate to{};
    int timeframe_s{kTimeframe1m};
};

[[nodiscard]] ChartDownloadRequest chartDownloadWindow(const CChartSettings& settings,
                                                       SessionDate today);

}  // namespace terminal
