// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudyPlot.h"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace terminal {

void drawStudyOverlays(std::span<const CStudySeries> overlays,
                       const ChartVisibleWindow& win,
                       int bar_count)
{
    if (bar_count <= 0)
    {
        return;
    }

    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    ImPlot::PushPlotClipRect();
    for (const CStudySeries& series : overlays)
    {
        if (series.placement != StudyPlacement::Overlay ||
            series.values.size() != static_cast<std::size_t>(bar_count))
        {
            continue;
        }

        const int first = std::max(0, win.first);
        const int last = std::min(bar_count - 1, win.last);
        if (first > last)
        {
            continue;
        }
        // One sample outside the window so the stroke reaches the clip edge.
        const int draw_first = first > 0 ? first - 1 : first;
        const int draw_last = last + 1 < bar_count ? last + 1 : last;

        bool have_point = false;
        ImVec2 previous{};
        for (int i = draw_first; i <= draw_last; ++i)
        {
            const double y = series.values[static_cast<std::size_t>(i)];
            if (!std::isfinite(y))
            {
                have_point = false;
                continue;
            }
            const ImVec2 point =
                ImPlot::PlotToPixels(static_cast<double>(i), y, ImAxis_X1, ImAxis_Y1);
            if (have_point)
            {
                draw_list->AddLine(previous, point, series.color, 1.5f);
            }
            previous = point;
            have_point = true;
        }
    }
    ImPlot::PopPlotClipRect();
}

}  // namespace terminal
