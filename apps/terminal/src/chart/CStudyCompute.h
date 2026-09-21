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

inline void clampMovingAverageParams(MovingAverageParams& params) noexcept
{
    if (params.length < kStudyMinLength)
    {
        params.length = kStudyMinLength;
    }
    if (params.length > kStudyMaxLength)
    {
        params.length = kStudyMaxLength;
    }
    // v1 compute is SMA only; Exponential and Weighted stay on the enum.
    if (params.method != MovingAverageMethod::Simple)
    {
        params.method = MovingAverageMethod::Simple;
    }
}

[[nodiscard]] inline double barSourceValue(const Bar& bar, StudySource source) noexcept
{
    switch (source)
    {
    case StudySource::Open:
        return bar.open;
    case StudySource::High:
        return bar.high;
    case StudySource::Low:
        return bar.low;
    case StudySource::Close:
        return bar.close;
    case StudySource::Volume:
        return bar.volume;
    }
    return bar.close;
}

[[nodiscard]] bool isStudyInstanceSupported(const CStudyInstance& inst) noexcept;

// Never throws. Disabled and unsupported instances are omitted.
// Enabled + empty bars → one series, values.size()==0, label set.
// Enabled + n bars → values.size()==n (NaN in warmup slots).
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

// Automatic Y for one chart region. Volume series include zero so bars grow
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
