#pragma once

#include "market_data/MboumMap.h"

#include <string>
#include <string_view>
#include <vector>

namespace myapp {

struct MboumV3Page
{
    bool splits{false};
    bool no_data{false};
    std::vector<MboumV3BarRow> bars;
};

[[nodiscard]] MboumV3Page parseMboumV3Historical(std::string_view json);

[[nodiscard]] std::string mboumV3HistoricalUrl(std::string_view ticker, SessionDate session_date);

}  // namespace myapp
