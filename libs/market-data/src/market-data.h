//
// Created by cynickal on 9/19/26.
//

#ifndef MYAPP_MARKET_DATA_H
#define MYAPP_MARKET_DATA_H

#include "CBarData.h"

#include <cstddef>
#include <string>
#include <vector>

namespace market_data{
    const std::vector<CBarData> GetBarData(const std::string& Symbol, std::size_t NumBars);
}

#endif //MYAPP_MARKET_DATA_H
