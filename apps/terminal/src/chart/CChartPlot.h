// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartSettings.h"
#include "chart/CChartView.h"
#include "chart/CStudy.h"
#include "market_data/Types.h"

#include <span>
#include <string_view>

namespace terminal {

void drawCandlesticks(std::span<const Bar> bars,
                      CChartSettings& settings,
                      CChartViewState& view,
                      std::string_view timezone,
                      std::span<const CStudySeries> studies = {});

}  // namespace terminal
