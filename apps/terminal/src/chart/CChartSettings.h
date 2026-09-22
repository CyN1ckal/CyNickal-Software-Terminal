// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace terminal {

enum class ChartBarPeriod : std::uint8_t
{
    Minute1 = 0,  // kTimeframe1m identity
    Minute5,
    Minute15,
    Hour1,
    Day1  // one RTH session; Bar::timeframe_s = 86400, not a UTC day bucket
};

enum class ChartBarType : std::uint8_t
{
    Candlestick = 0,  // v1 implemented
    Ohlc,             // reserved
    LineOnClose       // reserved
};

enum class ChartDataLimitMode : std::uint8_t
{
    SessionCount = 0,  // v1 implemented ("Days to Load")
    BarCount,          // reserved
    DateRange          // reserved
};

// Sierra Chart Scale Range (Chart >> Chart Settings >> Scale). Linear only in this slice.
enum class ChartScaleRange : std::uint8_t
{
    Automatic = 0,    // fit visible bars + padding
    ConstantRange,    // fixed price range, centered on last visible bar
    UserDefined       // fixed top/bottom
};

inline constexpr int kChartNyseSessionsPerYear = 252;
inline constexpr int kChartMinSessionCount = 1;
inline constexpr int kChartDefaultIntradaySessionCount = 14;  // 2 NYSE weeks
inline constexpr int kChartDefaultHistoricalSessionCount = kChartNyseSessionsPerYear * 5;
inline constexpr int kChartMaxIntradaySessionCount = kChartNyseSessionsPerYear;
inline constexpr int kChartMaxHistoricalSessionCount = kChartNyseSessionsPerYear * 10;
inline constexpr int kChartDefaultSessionCount = kChartDefaultIntradaySessionCount;
inline constexpr int kChartMaxSessionCount = kChartMaxIntradaySessionCount;
inline constexpr int kChartMaxDailySessionCount = kChartMaxHistoricalSessionCount;

[[nodiscard]] inline bool chartPeriodIsHistorical(ChartBarPeriod period) noexcept
{
    return period == ChartBarPeriod::Day1;
}

[[nodiscard]] inline int chartMaxSessionCount(ChartBarPeriod period) noexcept
{
    return chartPeriodIsHistorical(period) ? kChartMaxHistoricalSessionCount
                                           : kChartMaxIntradaySessionCount;
}
inline constexpr float kChartMinBarSpacingPx = 1.0f;
inline constexpr float kChartMaxBarSpacingPx = 128.0f;
inline constexpr float kChartDefaultBarSpacingPx = 8.0f;
inline constexpr float kChartDefaultBarWidthFrac = 0.60f;
inline constexpr float kChartDefaultScalePaddingPct = 4.0f;
inline constexpr int kChartRightFillBars = 2;

struct CChartSettings
{
    std::string symbol;  // ticker; loadChartBars trims/uppercases a local copy. empty = unconfigured
    ChartBarPeriod period{ChartBarPeriod::Minute1};
    ChartBarType bar_type{ChartBarType::Candlestick};
    ChartDataLimitMode limit_mode{ChartDataLimitMode::SessionCount};
    int intraday_session_count{kChartDefaultIntradaySessionCount};
    int historical_session_count{kChartDefaultHistoricalSessionCount};
    int bar_count{kUsRthExpected1m * kChartDefaultIntradaySessionCount};  // reserved
    SessionDate range_from{};  // reserved, YYYYMMDD
    SessionDate range_to{};    // reserved, YYYYMMDD
    ChartScaleRange scale_range{ChartScaleRange::Automatic};
    double constant_range{};  // price units; 0 = derive from first Automatic window
    double user_top{};
    double user_bottom{};
    float bar_spacing_px{kChartDefaultBarSpacingPx};
    float bar_width_frac{kChartDefaultBarWidthFrac};
    float scale_padding_pct{kChartDefaultScalePaddingPct};
};

[[nodiscard]] inline int timeframeSeconds(ChartBarPeriod period) noexcept
{
    switch (period)
    {
    case ChartBarPeriod::Minute1:
        return kTimeframe1m;
    case ChartBarPeriod::Minute5:
        return 300;
    case ChartBarPeriod::Minute15:
        return 900;
    case ChartBarPeriod::Hour1:
        return 3600;
    case ChartBarPeriod::Day1:
        return kTimeframe1d;
    }
    return kTimeframe1m;
}

[[nodiscard]] inline const char* chartPeriodCode(ChartBarPeriod period) noexcept
{
    switch (period)
    {
    case ChartBarPeriod::Minute1:
        return "1m";
    case ChartBarPeriod::Minute5:
        return "5m";
    case ChartBarPeriod::Minute15:
        return "15m";
    case ChartBarPeriod::Hour1:
        return "1h";
    case ChartBarPeriod::Day1:
        return "1d";
    }
    return "1m";
}

[[nodiscard]] inline bool isV1Supported(const CChartSettings& s) noexcept
{
    return s.period == ChartBarPeriod::Minute1 && s.bar_type == ChartBarType::Candlestick &&
           s.limit_mode == ChartDataLimitMode::SessionCount;
}

inline void clampSessionCountField(int& value, int max_sessions) noexcept
{
    if (value < kChartMinSessionCount)
    {
        value = kChartMinSessionCount;
    }
    if (value > max_sessions)
    {
        value = max_sessions;
    }
}

[[nodiscard]] inline int chartSessionCount(const CChartSettings& s) noexcept
{
    return chartPeriodIsHistorical(s.period) ? s.historical_session_count : s.intraday_session_count;
}

inline void clampV1Limits(CChartSettings& s) noexcept
{
    clampSessionCountField(s.intraday_session_count, kChartMaxIntradaySessionCount);
    clampSessionCountField(s.historical_session_count, kChartMaxHistoricalSessionCount);
    if (s.bar_spacing_px < kChartMinBarSpacingPx)
    {
        s.bar_spacing_px = kChartMinBarSpacingPx;
    }
    if (s.bar_spacing_px > kChartMaxBarSpacingPx)
    {
        s.bar_spacing_px = kChartMaxBarSpacingPx;
    }
    if (s.bar_width_frac < 0.10f)
    {
        s.bar_width_frac = 0.10f;
    }
    if (s.bar_width_frac > 1.00f)
    {
        s.bar_width_frac = 1.00f;
    }
    if (s.scale_padding_pct < 0.0f)
    {
        s.scale_padding_pct = 0.0f;
    }
    if (s.scale_padding_pct > 50.0f)
    {
        s.scale_padding_pct = 50.0f;
    }
    if (s.constant_range < 0.0)
    {
        s.constant_range = 0.0;
    }
    if (s.user_top < s.user_bottom)
    {
        const double tmp = s.user_top;
        s.user_top = s.user_bottom;
        s.user_bottom = tmp;
    }
}

[[nodiscard]] inline bool settingsIdentityEqual(const CChartSettings& a,
                                                const CChartSettings& b) noexcept
{
    return a.symbol == b.symbol && a.period == b.period && a.bar_type == b.bar_type &&
           a.limit_mode == b.limit_mode && chartSessionCount(a) == chartSessionCount(b);
}

}  // namespace terminal
