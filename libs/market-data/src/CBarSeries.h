//
// Created by cynickal on 9/19/26.
//

#ifndef MYAPP_CBARSERIES_H
#define MYAPP_CBARSERIES_H

#include "CBarData.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class CBarSeries {
public:
    enum class Adjustment : std::uint8_t {
        None,
        Split,
        SplitAndDividend
    };

    std::vector<CBarData> m_Bars;
    std::string m_Symbol;
    std::chrono::seconds m_Timeframe{};
    Adjustment m_Adjustment{Adjustment::None};
};

#endif //MYAPP_CBARSERIES_H
