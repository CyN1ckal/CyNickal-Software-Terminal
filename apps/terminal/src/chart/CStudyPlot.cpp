// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudyPlot.h"

#include "chart/CStudyCompute.h"
#include "ui/Theme.h"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace terminal {
namespace {

constexpr float kStudyLineThickness = 1.5f;

struct DashPattern
{
    float dash{1.0f};
    float gap{1.0f};
};

[[nodiscard]] DashPattern dashPattern(StudyLineStyle style) noexcept
{
    if (style == StudyLineStyle::Dotted)
    {
        // About one stroke wide, then a gap, so the marks read as dots.
        return {.dash = 1.75f, .gap = 3.5f};
    }
    return {.dash = 9.0f, .gap = 5.0f};
}

// phase is pixels into the dash pattern. It carries across bars so a short
// segment does not restart on a dash and paint a solid line.
void strokeStudyLine(ImDrawList* draw,
                     ImVec2 from,
                     ImVec2 to,
                     ImU32 color,
                     StudyLineStyle style,
                     float& phase)
{
    if (style == StudyLineStyle::Value)
    {
        return;
    }
    if (style == StudyLineStyle::Solid)
    {
        draw->AddLine(from, to, color, kStudyLineThickness);
        return;
    }
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::hypot(dx, dy);
    if (!(length > 0.0f))
    {
        return;
    }
    const DashPattern pattern = dashPattern(style);
    const float period = pattern.dash + pattern.gap;
    if (!(period > 0.0f))
    {
        draw->AddLine(from, to, color, kStudyLineThickness);
        return;
    }
    const float ux = dx / length;
    const float uy = dy / length;
    float traveled = 0.0f;
    while (traveled < length)
    {
        if (!(phase >= 0.0f) || phase >= period)
        {
            phase = 0.0f;
        }
        const bool ink = phase < pattern.dash;
        const float room = ink ? pattern.dash - phase : period - phase;
        const float step = std::min(room, length - traveled);
        if (!(step > 0.0f))
        {
            break;
        }
        if (ink)
        {
            const float end_d = traveled + step;
            draw->AddLine(ImVec2(from.x + (ux * traveled), from.y + (uy * traveled)),
                          ImVec2(from.x + (ux * end_d), from.y + (uy * end_d)), color,
                          kStudyLineThickness);
        }
        traveled += step;
        phase += step;
        if (phase >= period)
        {
            phase -= period;
        }
    }
}

// Prefix plus "..." that fits max_width in the current font. Empty when even
// the mark does not fit, so AddText cannot run past the pill.
[[nodiscard]] std::string ellipsizeLabel(std::string_view text, float max_width)
{
    if (!(max_width > 0.0f))
    {
        return {};
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    if (ImGui::CalcTextSize(begin, end, false).x <= max_width)
    {
        return std::string{text};
    }
    const char* dots = "...";
    const float dots_w = ImGui::CalcTextSize(dots, nullptr, false).x;
    if (dots_w > max_width)
    {
        return {};
    }
    const float budget = max_width - dots_w;
    std::size_t lo = 0;
    std::size_t hi = text.size();
    while (lo < hi)
    {
        const std::size_t span = hi - lo + 1U;
        const std::size_t mid = lo + (span / 2U);
        if (ImGui::CalcTextSize(begin, begin + mid, false).x <= budget)
        {
            lo = mid;
        }
        else
        {
            hi = mid - 1U;
        }
    }
    while (lo > 0 && lo < text.size() &&
           (static_cast<unsigned char>(text[lo]) & 0xC0U) == 0x80U)
    {
        --lo;
    }
    std::string out(text.substr(0, lo));
    out += dots;
    return out;
}

void drawStudyValueLabels(ImDrawList* draw_list, std::span<const CStudySeries> studies)
{
    const ImVec2 plot_pos = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();
    constexpr float kInset = 6.0f;
    constexpr float kPadX = 6.0f;
    constexpr float kPadY = 3.0f;
    constexpr float kGap = 3.0f;
    constexpr float kRounding = 2.0f;
    const float max_w = plot_size.x - (2.0f * kInset);
    if (!(max_w > 0.0f) || !(plot_size.y > 0.0f))
    {
        return;
    }
    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    float y = plot_pos.y + kInset;
    const float bottom = plot_pos.y + plot_size.y - kInset;
    for (const CStudySeries& series : studies)
    {
        if (series.line != StudyLineStyle::Value)
        {
            continue;
        }
        const std::string text = studyValueLabelText(series.label, series.values, series.value_decimals);
        if (text.empty() || !(y < bottom))
        {
            continue;
        }
        const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
        const float box_w = std::min(max_w, text_size.x + (2.0f * kPadX));
        const float box_h = text_size.y + (2.0f * kPadY);
        const ImVec2 box_min(plot_pos.x + kInset, y);
        const ImVec2 box_max(box_min.x + box_w, box_min.y + box_h);
        const float inner_w = box_w - (2.0f * kPadX);
        const std::string shown = ellipsizeLabel(text, inner_w);
        const ImU32 fill = ImGui::ColorConvertFloat4ToU32(Theme::kBg1);
        const ImU32 border = ImGui::ColorConvertFloat4ToU32(Theme::kHairline);
        draw_list->AddRectFilled(box_min, box_max, fill, kRounding);
        draw_list->AddRect(box_min, box_max, border, kRounding);
        if (!shown.empty())
        {
            const ImVec2 text_pos(box_min.x + kPadX, box_min.y + kPadY);
            const ImVec4 clip(box_min.x, box_min.y, box_max.x, box_max.y);
            draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_pos, series.color,
                               shown.c_str(), nullptr, 0.0f, &clip);
        }
        y += box_h + kGap;
    }
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
}

}  // namespace

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
        if (series.line == StudyLineStyle::Value || !series.histogram ||
            clampStudyChartRegion(series.chart_region) != region ||
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
            if (series.color_by_bar && have_bars)
            {
                const Bar& bar = bars[index];
                color = studyHistogramColor(series, bar.close >= bar.open);
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
        if (series.line == StudyLineStyle::Value || series.histogram ||
            clampStudyChartRegion(series.chart_region) != region ||
            series.values.size() != static_cast<std::size_t>(bar_count))
        {
            continue;
        }
        bool have_point = false;
        float phase = 0.0f;
        ImVec2 previous{};
        for (int i = draw_first; i <= draw_last; ++i)
        {
            const double y = series.values[static_cast<std::size_t>(i)];
            if (!std::isfinite(y))
            {
                have_point = false;
                phase = 0.0f;
                continue;
            }
            const ImVec2 point = ImPlot::PlotToPixels(static_cast<double>(i), y, ImAxis_X1, ImAxis_Y1);
            if (have_point)
            {
                strokeStudyLine(draw_list, previous, point, series.color, series.line, phase);
            }
            previous = point;
            have_point = true;
        }
    }
    if (region == kStudyMainChartRegion)
    {
        drawStudyValueLabels(draw_list, studies);
    }
    ImPlot::PopPlotClipRect();
}

}  // namespace terminal
