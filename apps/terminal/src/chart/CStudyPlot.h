// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartView.h"
#include "chart/CStudy.h"
#include "market_data/Types.h"

#include <span>

namespace terminal {

// Lines, volume histograms, and value labels for one chart region. Call inside
// BeginPlot, after candles (region 1) or alone (lower regions), and before the
// crosshair. NaN breaks a line. Line series use their own solid, dotted, or
// dashed stroke. Volume bars grow from zero. Up and down candles use that
// study's two colors. A value style draws a label of the latest sample on the
// price chart instead of a series.
void drawStudyRegion(std::span<const CStudySeries> studies,
                     int chart_region,
                     const ChartVisibleWindow& win,
                     int bar_count,
                     std::span<const Bar> bars,
                     float bar_width_frac,
                     bool stems_only);

}  // namespace terminal
