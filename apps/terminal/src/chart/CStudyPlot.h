// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartView.h"
#include "chart/CStudy.h"
#include "market_data/Types.h"

#include <span>

namespace terminal {

// Lines and volume histograms for one chart region. Call inside BeginPlot,
// after candles (region 1) or alone (lower regions), and before the crosshair.
// NaN breaks a line. Volume bars grow from zero and use the candle up/down colors.
void drawStudyRegion(std::span<const CStudySeries> studies,
                     int chart_region,
                     const ChartVisibleWindow& win,
                     int bar_count,
                     std::span<const Bar> bars,
                     float bar_width_frac,
                     bool stems_only);

}  // namespace terminal
