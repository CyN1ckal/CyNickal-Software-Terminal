// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartView.h"
#include "chart/CStudy.h"

#include <span>

namespace terminal {

// Price-axis polylines. Subgraph series are skipped. Call inside BeginPlot,
// after candles and before the crosshair. NaN breaks the stroke.
void drawStudyOverlays(std::span<const CStudySeries> overlays,
                       const ChartVisibleWindow& win,
                       int bar_count);

}  // namespace terminal
