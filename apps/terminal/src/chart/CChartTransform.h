// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartSettings.h"
#include "market_data/Types.h"

#include <string_view>
#include <vector>

namespace terminal {

// Higher-timeframe charts are built at load time from 1-minute Store rows.
// Canonical persisted grains are kTimeframe1m and kTimeframe1d. 5m/15m/1h
// remain load-time composites and are not written back to SQLite.
//
// Alignment origin is the US RTH open (09:30 local), not the Unix epoch.
// Buckets never span session dates. Day1 is one RTH session, labeled with
// Bar::timeframe_s = 86400 (not a UTC midnight–midnight bucket).

[[nodiscard]] inline bool isChartPeriodSupported(ChartBarPeriod period) noexcept
{
    switch (period)
    {
    case ChartBarPeriod::Minute1:
    case ChartBarPeriod::Minute5:
    case ChartBarPeriod::Minute15:
    case ChartBarPeriod::Hour1:
    case ChartBarPeriod::Day1:
        return true;
    }
    return false;
}

[[nodiscard]] inline bool isChartSettingsSupported(const CChartSettings& settings) noexcept
{
    return isChartPeriodSupported(settings.period) &&
           settings.bar_type == ChartBarType::Candlestick &&
           settings.limit_mode == ChartDataLimitMode::SessionCount;
}

[[nodiscard]] inline bool chartNeedsBarTransform(ChartBarPeriod period) noexcept
{
    return period != ChartBarPeriod::Minute1 && isChartPeriodSupported(period);
}

[[nodiscard]] inline bool chartNeedsBarTransform(const CChartSettings& settings) noexcept
{
    return chartNeedsBarTransform(settings.period);
}

// Composite 1-minute bars into period. Minute1 is identity.
// Source bars are assumed sorted by ts (queryBars order). Non-1m and
// non-RTH bars are skipped. Empty buckets emit nothing. A partial bucket
// (missing minutes, or a short last hour) still emits a bar from the
// minutes that exist. Open/high/low/close/volume are first/max/min/last/sum.
// Throws if timezone is empty or missing from tzdb (same as Time helpers).
[[nodiscard]] std::vector<Bar> transformChartBars(const std::vector<Bar>& bars_1m,
                                                  ChartBarPeriod period,
                                                  std::string_view timezone);

}  // namespace terminal
