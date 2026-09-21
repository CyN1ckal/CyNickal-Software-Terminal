// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudyPlot.h"

#include "ui/Theme.h"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace terminal {

void drawStudyRegion(std::span<const CStudySeries> studies,
                     int chart_region,
                     const ChartVisibleWindow& win,
                     int bar_count,
                     std::span<const Bar> bars,
                     float bar_width_frac,
                     bool stems_only)
{
    if (bar_count <= 0)
    {
        return;
    }
    const int first = std::max(0, win.first);
    const int last = std::min(bar_count - 1, win.last);
    if (first > last)
    {
        return;
    }
    // One sample outside the window so a line reaches the clip edge.
    const int draw_first = first > 0 ? first - 1 : first;
    const int draw_last = last + 1 < bar_count ? last + 1 : last;
    const int region = clampStudyChartRegion(chart_region);
    const bool have_bars = bars.size() == static_cast<std::size_t>(bar_count);
    const double half_width = 0.5 * static_cast<double>(bar_width_frac);

    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    ImPlot::PushPlotClipRect();
    for (const CStudySeries& series : studies)
    {
        if (series.kind != StudyKind::Volume || clampStudyChartRegion(series.chart_region) != region ||
            series.values.size() != static_cast<std::size_t>(bar_count))
        {
            continue;
        }
        for (int i = draw_first; i <= draw_last; ++i)
        {
            const auto index = static_cast<std::size_t>(i);
            const double volume = series.values[index];
            if (!std::isfinite(volume) || volume == 0.0)
            {
                continue;
            }
            ImU32 color = series.color;
            if (have_bars)
            {
                const Bar& bar = bars[index];
                color = ImGui::ColorConvertFloat4ToU32(bar.close >= bar.open ? Theme::kUp : Theme::kDown);
            }
            if (stems_only)
            {
                const ImVec2 high = ImPlot::PlotToPixels(static_cast<double>(i), volume, ImAxis_X1, ImAxis_Y1);
                const ImVec2 low = ImPlot::PlotToPixels(static_cast<double>(i), 0.0, ImAxis_X1, ImAxis_Y1);
                draw_list->AddLine(high, low, color);
                continue;
            }
            const ImVec2 left =
                ImPlot::PlotToPixels(static_cast<double>(i) - half_width, 0.0, ImAxis_X1, ImAxis_Y1);
            const ImVec2 right =
                ImPlot::PlotToPixels(static_cast<double>(i) + half_width, volume, ImAxis_X1, ImAxis_Y1);
            const float y0 = std::min(left.y, right.y);
            float y1 = std::max(left.y, right.y);
            if (y1 - y0 < 1.0f)
            {
                y1 = y0 + 1.0f;
            }
            draw_list->AddRectFilled(ImVec2(left.x, y0), ImVec2(right.x, y1), color);
        }
    }
    for (const CStudySeries& series : studies)
    {
        if (series.kind == StudyKind::Volume || clampStudyChartRegion(series.chart_region) != region ||
            series.values.size() != static_cast<std::size_t>(bar_count))
        {
            continue;
        }
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
            const ImVec2 point = ImPlot::PlotToPixels(static_cast<double>(i), y, ImAxis_X1, ImAxis_Y1);
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
