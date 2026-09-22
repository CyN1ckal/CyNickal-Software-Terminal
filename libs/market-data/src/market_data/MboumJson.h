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

}  // namespace terminal
