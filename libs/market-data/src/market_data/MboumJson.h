// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/MboumMap.h"

#include <string>
#include <string_view>
#include <vector>

namespace terminal {

struct MboumV3Page
{
    bool splits{false};
    bool no_data{false};
    std::vector<MboumV3BarRow> bars;
};

struct MboumV3DailyPage
{
    bool splits{false};
    bool no_data{false};
    std::vector<MboumV3DailyRow> bars;
};

[[nodiscard]] MboumV3Page parseMboumV3Historical(std::string_view json);
[[nodiscard]] MboumV3DailyPage parseMboumV3Daily(std::string_view json);

[[nodiscard]] std::string mboumV3HistoricalUrl(std::string_view ticker, SessionDate session_date);
[[nodiscard]] std::string mboumV3DailyUrl(std::string_view ticker,
                                          SessionDate from,
                                          SessionDate to,
                                          int limit = kMboumDailyPageLimit);

// One split from GET /v1/markets/stock/history?interval=1mo&diffandsplits=true.
// 1mo is the documented 10-year window. 1d is 5 years, short of a 2520-session chart.
// Price bars in that payload are already adjusted and are not represented here.
struct MboumV1SplitEvent
{
    UnixSeconds ex_ts{};
    double split_ratio{};
};

[[nodiscard]] std::vector<MboumV1SplitEvent> parseMboumV1SplitEvents(std::string_view json);
[[nodiscard]] std::string mboumV1SplitsUrl(std::string_view ticker);

}  // namespace terminal
