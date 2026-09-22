// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartLoad.h"
#include "chart/CChartView.h"
#include "chart/CStudy.h"
#include "market_data/Types.h"

#include <span>
#include <string>
#include <vector>

namespace terminal {

[[nodiscard]] bool isStudyInstanceSupported(const CStudyInstance& inst) noexcept;

// Pull integer options into the study's declared range. Unknown studies are left alone.
void clampStudyOptions(CStudyInstance& inst) noexcept;

// Resize outputs to the study's declared traces. An empty list copies color and a
// solid line onto every output so an older chartbook keeps one color.
void normalizeStudyOutputs(CStudyInstance& inst);

// Palette colors for a study that was just added. slot is its place in the list
// and is used when the study does not pin a palette index.
void assignStudyOutputDefaults(CStudyInstance& inst, int slot);

// Never throws. Disabled and unsupported instances are omitted.
// Each output trace becomes one series, in declared output order.
// Color and line style come from CStudyInstance::outputs. A missing output uses
// the study color and a solid line.
// Empty bars leave values empty and labels set.
// Enabled + n bars → each trace has values.size()==n (NaN in warmup slots).
[[nodiscard]] std::vector<CStudySeries> computeStudies(std::span<const Bar> bars,
                                                       std::span<const CStudyInstance> studies);

// Ready + non-empty bars → computeStudies; every other load result → {}.
[[nodiscard]] std::vector<CStudySeries> studiesForLoad(const ChartLoadResult& loaded,
                                                       std::span<const CStudyInstance> studies);

// Skip NaN and any series whose values.size() != bar_count or placement != Overlay.
// bar_count is the plot X domain (bars.size()), not win.slot_count.
[[nodiscard]] OverlayYExtent overlayYExtent(std::span<const CStudySeries> series,
                                            const ChartVisibleWindow& win,
                                            int bar_count) noexcept;

// How many stacked regions to draw: 1 through the highest chart region in use.
// An empty list is the price graph alone.
[[nodiscard]] int studyChartRegionCount(std::span<const CStudySeries> series) noexcept;

// Automatic Y for one chart region. anchor_zero series include zero so bars grow
// from the baseline. No finite samples → 0..1. Does not throw.
[[nodiscard]] ChartYLimits computeStudyRegionYLimits(std::span<const CStudySeries> series,
                                                     int chart_region,
                                                     const ChartVisibleWindow& win,
                                                     int bar_count,
                                                     float padding_pct,
                                                     double extra_pad_frac,
                                                     double move_offset) noexcept;

[[nodiscard]] std::string studyShortLabel(const CStudyInstance& inst);

}  // namespace terminal
