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

// Interactive scale and scale-range items. Constant Range and User Defined stay
// disabled when `ylim_valid` is false; they copy `ylim` into the settings.
void drawChartScaleMenuItems(CChartSettings& settings,
                             CChartViewState& view,
                             const ChartYLimits& ylim,
                             bool ylim_valid);

void drawCandlesticks(std::span<const Bar> bars,
                      CChartSettings& settings,
                      CChartViewState& view,
                      std::string_view timezone,
                      std::span<const CStudySeries> studies = {});

}  // namespace terminal
