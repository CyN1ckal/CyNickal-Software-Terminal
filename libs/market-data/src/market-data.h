//
// Created by cynickal on 9/19/26.
//

#ifndef MYAPP_MARKET_DATA_H
#define MYAPP_MARKET_DATA_H

#include "CBarSeries.h"

#include <cstddef>
#include <string>

namespace market_data{
    CBarSeries GetBarData(const std::string& Symbol, std::size_t NumBars);
}

#endif //MYAPP_MARKET_DATA_H
