// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartPlot.h"

#include "chart/CChartAxis.h"
#include "chart/CStudyCompute.h"
#include "chart/CStudyPlot.h"
#include "market_data/Time.h"
#include "ui/Theme.h"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

int formatXTick(double value, char* buf, int size, void* data) // NOLINT(misc-const-correctness)
{
    const auto* bars = static_cast<const std::span<const Bar>*>(data);
    if (bars == nullptr || bars->empty() || buf == nullptr || size <= 0)
    {
        return 0;
    }
    int idx = static_cast<int>(std::lround(value));
    idx = std::clamp(idx, 0, static_cast<int>(bars->size()) - 1);
    const UnixSeconds ts = (*bars)[static_cast<std::size_t>(idx)].ts;
    const auto t = static_cast<std::time_t>(ts);
    std::tm utc{};
    if (!tryUtcTm(t, utc))
    {
        buf[0] = '\0';
        return 0;
    }
    return std::snprintf(buf, static_cast<std::size_t>(size), "%02d-%02d %02d:%02d", utc.tm_mon + 1,
                         utc.tm_mday, utc.tm_hour, utc.tm_min);
}

int formatYTick(double value, char* buf, int size, void* /*data*/)
{
    const double mag = std::abs(value);
    if (mag >= 1000.0)
    {
        return std::snprintf(buf, static_cast<std::size_t>(size), "%.0f", value);
    }
    if (mag >= 100.0)
    {
        return std::snprintf(buf, static_cast<std::size_t>(size), "%.2f", value);
    }
    return std::snprintf(buf, static_cast<std::size_t>(size), "%.4f", value);
}

void buildTimeTicks(std::span<const Bar> bars,
                    const ChartVisibleWindow& win,
                    std::string_view tz,
                    float spacing_px,
                    CChartViewState& view)
{
    view.tick_xs.clear();
    view.tick_labels.clear();
    view.tick_ptrs.clear();

    ChartTickMetrics metrics;
    metrics.spacing_px = spacing_px;
    metrics.date_px = ImGui::CalcTextSize("0000-00-00").x;
    metrics.time_px = ImGui::CalcTextSize("00:00").x;
    metrics.month_px = ImGui::CalcTextSize("0000-00").x;
    metrics.year_px = ImGui::CalcTextSize("0000").x;
    metrics.gap_px = std::max(8.0f, ImGui::GetFontSize() * 0.5f);

    const std::vector<ChartAxisTick> ticks = buildChartTimeTicks(bars, win, tz, metrics);
    view.tick_xs.reserve(ticks.size());
    view.tick_labels.reserve(ticks.size());
    for (const ChartAxisTick& tick : ticks)
    {
        view.tick_xs.push_back(tick.x);
        view.tick_labels.push_back(tick.label);
    }
    view.tick_ptrs.reserve(view.tick_labels.size());
    for (const std::string& label : view.tick_labels)
    {
        view.tick_ptrs.push_back(label.c_str());
    }
}

void handlePlotInput(std::span<const Bar> bars,
                     CChartSettings& settings,
                     CChartViewState& view,
                     const ChartVisibleWindow& win,
                     const ChartYLimits& ylim,
                     bool* x_handled,
                     StudyRegionScale* region_scale,
                     bool clamp_scroll)
{
    const ImGuiIO& io = ImGui::GetIO();
    const bool ctrl = io.KeyCtrl;
    ChartInteractiveScale mode = view.interactive;
    if (ctrl)
    {
        if (mode == ChartInteractiveScale::Range)
        {
            mode = ChartInteractiveScale::Move;
        }
        else if (mode == ChartInteractiveScale::Move)
        {
            mode = ChartInteractiveScale::Range;
        }
    }

    const float plot_h = std::max(1.0f, ImPlot::GetPlotSize().y);
    const float spacing = settings.bar_spacing_px;
    const bool hovered = ImPlot::IsPlotHovered();
    const bool x_axis = ImPlot::IsAxisHovered(ImAxis_X1);
    const bool shared_x = x_handled != nullptr;
    const bool x_free = !shared_x || !*x_handled;
    // Solo (x_handled == nullptr) always owns time. In a stack, only the hovered
    // region, or the one already dragging, applies wheel, pan, and X-axis zoom.
    const bool take_x =
        x_free && (!shared_x || hovered || x_axis || view.dragging_plot || view.dragging_x);

    if (take_x && hovered && io.MouseWheel != 0.0f)
    {
        const double idx = ImPlot::GetPlotMousePos().x;
        const double span = win.x_max - win.x_min;
        const double rel = span > 0.0 ? (idx - win.x_min) / span : 1.0;
        settings.bar_spacing_px =
            std::clamp(spacing + io.MouseWheel, kChartMinBarSpacingPx, kChartMaxBarSpacingPx);
        if (io.KeyShift)
        {
            const ChartVisibleWindow after =
                computeVisibleWindow(static_cast<int>(bars.size()), view.last_plot_w,
                                     settings.bar_spacing_px, 0, kChartRightFillBars);
            const double new_x_min = idx - (rel * static_cast<double>(after.slot_count));
            const double new_x_max = new_x_min + static_cast<double>(after.slot_count);
            view.scroll_from_end = static_cast<int>(std::lround(
                static_cast<double>(static_cast<int>(bars.size()) - 1 + kChartRightFillBars) + 0.5 -
                new_x_max));
        }
    }

    if (take_x && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        view.dragging_plot = true;
    }
    if (take_x && view.dragging_plot)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            view.scroll_from_end += static_cast<int>(std::lround(-io.MouseDelta.x / spacing));
        }
        else
        {
            view.dragging_plot = false;
        }
    }

    if (take_x && x_axis && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        view.dragging_x = true;
    }
    if (take_x && view.dragging_x)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            settings.bar_spacing_px = std::clamp(settings.bar_spacing_px - (io.MouseDelta.x * 0.05f),
                                                 kChartMinBarSpacingPx, kChartMaxBarSpacingPx);
        }
        else
        {
            view.dragging_x = false;
        }
    }
    if (take_x && x_handled != nullptr)
    {
        *x_handled = true;
    }

    double& extra_pad = region_scale != nullptr ? region_scale->extra_pad_frac : view.extra_pad_frac;
    double& move_offset = region_scale != nullptr ? region_scale->move_offset : view.move_offset;
    bool& dragging_y = region_scale != nullptr ? region_scale->dragging_y : view.dragging_y;

    if (ImPlot::IsAxisHovered(ImAxis_Y1) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        dragging_y = true;
    }
    if (ImPlot::IsAxisHovered(ImAxis_Y1) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        if (region_scale != nullptr)
        {
            region_scale->extra_pad_frac = 0.0;
            region_scale->move_offset = 0.0;
        }
        else
        {
            resetChartScale(view);
        }
        dragging_y = false;
    }
    if (dragging_y)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && mode != ChartInteractiveScale::Locked)
        {
            if (mode == ChartInteractiveScale::Move)
            {
                const double price_per_px = (ylim.max - ylim.min) / static_cast<double>(plot_h);
                move_offset += static_cast<double>(io.MouseDelta.y) * price_per_px;
            }
            else
            {
                const auto delta = static_cast<double>(io.MouseDelta.y / plot_h);
                if (region_scale == nullptr && settings.scale_range == ChartScaleRange::ConstantRange)
                {
                    double range = view.working_range > 0.0 ? view.working_range : (ylim.max - ylim.min);
                    range *= 1.0 + delta;
                    view.working_range = std::max(range, 1e-6);
                }
                else
                {
                    extra_pad += 0.5 * delta;
                    extra_pad = std::clamp(extra_pad, -0.5, 8.0);
                }
            }
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            dragging_y = false;
        }
    }

    if (region_scale == nullptr && ImPlot::IsAxisHovered(ImAxis_Y1) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("##chart_scale_menu");
    }
    if (region_scale == nullptr && ImGui::BeginPopup("##chart_scale_menu"))
    {
        if (ImGui::MenuItem("Interactive Scale Range", nullptr,
                            view.interactive == ChartInteractiveScale::Range))
        {
            view.interactive = ChartInteractiveScale::Range;
        }
        if (ImGui::MenuItem("Interactive Scale Move", nullptr,
                            view.interactive == ChartInteractiveScale::Move))
        {
            view.interactive = ChartInteractiveScale::Move;
        }
        if (ImGui::MenuItem("Interactive Scale Locked", nullptr,
                            view.interactive == ChartInteractiveScale::Locked))
        {
            view.interactive = ChartInteractiveScale::Locked;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Scale Range: Automatic", nullptr,
                            settings.scale_range == ChartScaleRange::Automatic))
        {
            settings.scale_range = ChartScaleRange::Automatic;
            resetChartScale(view);
        }
        if (ImGui::MenuItem("Scale Range: Constant Range", nullptr,
                            settings.scale_range == ChartScaleRange::ConstantRange))
        {
            settings.scale_range = ChartScaleRange::ConstantRange;
            view.working_range = ylim.max - ylim.min;
            settings.constant_range = view.working_range;
            view.extra_pad_frac = 0.0;
        }
        if (ImGui::MenuItem("Scale Range: User Defined", nullptr,
                            settings.scale_range == ChartScaleRange::UserDefined))
        {
            settings.scale_range = ChartScaleRange::UserDefined;
            settings.user_bottom = ylim.min - view.move_offset;
            settings.user_top = ylim.max - view.move_offset;
            view.extra_pad_frac = 0.0;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Scale"))
        {
            resetChartScale(view);
        }
        ImGui::EndPopup();
    }

    if (clamp_scroll)
    {
        const ChartVisibleWindow clamped = computeVisibleWindow(
            static_cast<int>(bars.size()), view.last_plot_w, settings.bar_spacing_px,
            view.scroll_from_end, kChartRightFillBars);
        view.scroll_from_end = clamped.scroll;
        clampV1Limits(settings);
    }
}

void drawCrosshair(std::span<const Bar> bars,
                   std::string_view tz,
                   const ChartYLimits& ylim,
                   std::span<const CStudySeries> studies,
                   int chart_region,
                   int region_count)
{
    if (bars.empty())
    {
        return;
    }
    const ImVec2 pos = ImPlot::GetPlotPos();
    const ImVec2 size = ImPlot::GetPlotSize();
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool hovered = ImPlot::IsPlotHovered();
    const bool x_inside = size.x > 0.0f && mouse.x >= pos.x && mouse.x <= pos.x + size.x;
    // One region keeps the old hover gate. A stack draws the vertical line in
    // every region while the pointer is in the shared X column.
    if (!hovered && (!x_inside || region_count <= 1))
    {
        return;
    }

    const float sample_y = pos.y + (size.y * 0.5f);
    const ImPlotPoint at = ImPlot::PixelsToPlot(mouse.x, sample_y, ImAxis_X1, ImAxis_Y1);
    int idx = static_cast<int>(std::lround(at.x));
    idx = std::clamp(idx, 0, static_cast<int>(bars.size()) - 1);
    const Bar& bar = bars[static_cast<std::size_t>(idx)];

    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(Theme::kHairline);
    ImPlot::PushPlotClipRect();
    const ImVec2 top = ImPlot::PlotToPixels(static_cast<double>(idx), ylim.max);
    const ImVec2 bot = ImPlot::PlotToPixels(static_cast<double>(idx), ylim.min);
    draw_list->AddLine(top, bot, color);
    if (hovered)
    {
        const double price = ImPlot::GetPlotMousePos().y;
        const ImVec2 left = ImPlot::PlotToPixels(ImPlot::GetPlotLimits().X.Min, price);
        const ImVec2 right = ImPlot::PlotToPixels(ImPlot::GetPlotLimits().X.Max, price);
        draw_list->AddLine(left, right, color);
        ImPlot::PopPlotClipRect();
        if (chart_region == kStudyMainChartRegion)
        {
            ImPlot::TagY(price, Theme::kAccent, "%.4f", price);
        }
        else if (std::abs(price) >= 1000.0)
        {
            ImPlot::TagY(price, Theme::kAccent, "%.0f", price);
        }
        else
        {
            ImPlot::TagY(price, Theme::kAccent, "%.2f", price);
        }
    }
    else
    {
        ImPlot::PopPlotClipRect();
    }

    const bool bottom = chart_region == region_count;
    if (bottom && (hovered || region_count > 1))
    {
        const ChartLocalTime stamp = chartLocalTime(tz, bar.ts);
        char time_buf[32];
        std::snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d", stamp.year, stamp.month,
                      stamp.day, stamp.hour, stamp.minute);
        ImPlot::TagX(static_cast<double>(idx), Theme::kAccent, "%s", time_buf);
    }

    if (!hovered)
    {
        return;
    }
    const ChartLocalTime stamp = chartLocalTime(tz, bar.ts);
    char time_buf[32];
    std::snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d", stamp.year, stamp.month,
                  stamp.day, stamp.hour, stamp.minute);
    ImGui::BeginTooltip();
    ImGui::Text("%s  O  %.4f  H  %.4f  L  %.4f  C  %.4f  V  %.0f", time_buf, bar.open, bar.high,
                bar.low, bar.close, bar.volume);
    const auto bar_count = static_cast<int>(bars.size());
    for (const CStudySeries& series : studies)
    {
        if (series.values.size() != static_cast<std::size_t>(bar_count))
        {
            continue;
        }
        const double value = series.values[static_cast<std::size_t>(idx)];
        if (!std::isfinite(value))
        {
            continue;
        }
        if (series.kind == StudyKind::Volume)
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(series.color), "%s  %.0f",
                               series.label.c_str(), value);
        }
        else
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(series.color), "%s  %.4f",
                               series.label.c_str(), value);
        }
    }
    ImGui::EndTooltip();
}

void drawSessionGuides(std::span<const Bar> bars, const ChartVisibleWindow& win, std::string_view tz)
{
    if (bars.empty())
    {
        return;
    }
    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(Theme::WithAlpha(Theme::kHairline, 0.45f));
    SessionDate prev = 0;
    bool have = false;
    ImPlot::PushPlotClipRect();
    const ImPlotRect limits = ImPlot::GetPlotLimits();
    for (int i = win.first; i <= win.last; ++i)
    {
        SessionDate date = 0;
        try
        {
            date = utcToSessionDate(tz.empty() ? "UTC" : tz, bars[static_cast<std::size_t>(i)].ts);
        }
        catch (const std::exception&)
        {
            continue;
        }
        if (have && date == prev)
        {
            continue;
        }
        if (have)
        {
            const ImVec2 a = ImPlot::PlotToPixels(static_cast<double>(i) - 0.5, limits.Y.Min);
            const ImVec2 b = ImPlot::PlotToPixels(static_cast<double>(i) - 0.5, limits.Y.Max);
            draw_list->AddLine(a, b, color);
        }
        prev = date;
        have = true;
    }
    ImPlot::PopPlotClipRect();
}

StudyRegionScale& studyRegionScale(CChartViewState& view, int chart_region)
{
    const auto index = static_cast<std::size_t>(clampStudyChartRegion(chart_region));
    if (view.study_region_scale.size() <= index)
    {
        view.study_region_scale.resize(index + 1);
    }
    return view.study_region_scale[index];
}

void ensureRegionRatios(CChartViewState& view, int regions)
{
    regions = std::max(regions, 1);
    if (std::cmp_equal(view.region_ratios.size(), regions))
    {
        return;
    }
    view.region_ratios.assign(static_cast<std::size_t>(regions), 1.0f);
    if (regions >= 2)
    {
        view.region_ratios[0] = 0.72f;
        const float each = 0.28f / static_cast<float>(regions - 1);
        for (int i = 1; i < regions; ++i)
        {
            view.region_ratios[static_cast<std::size_t>(i)] = each;
        }
    }
}

void drawChartRegion(std::span<const Bar> bars,
                     CChartSettings& settings,
                     CChartViewState& view,
                     std::string_view timezone,
                     std::span<const CStudySeries> studies,
                     ChartVisibleWindow win,
                     int chart_region,
                     int region_count,
                     bool* x_handled,
                     StudyRegionScale* region_scale,
                     bool clamp_scroll)
{
    const int bar_count = static_cast<int>(bars.size());
    const bool price = chart_region == kStudyMainChartRegion;
    const bool bottom = chart_region == region_count;
    const bool solo = region_count <= 1;

    ChartYLimits ylim;
    if (price)
    {
        const OverlayYExtent overlay = overlayYExtent(studies, win, bar_count);
        ylim = computeYLimits(bars, win, settings, view, overlay);
    }
    else
    {
        const double extra = region_scale != nullptr ? region_scale->extra_pad_frac : 0.0;
        const double move = region_scale != nullptr ? region_scale->move_offset : 0.0;
        ylim = computeStudyRegionYLimits(studies, chart_region, win, bar_count, settings.scale_padding_pct,
                                         extra, move);
    }

    char plot_id[32];
    if (price)
    {
        std::snprintf(plot_id, sizeof(plot_id), "##candles");
    }
    else
    {
        std::snprintf(plot_id, sizeof(plot_id), "##region_%d", chart_region);
    }

    const ImPlotFlags flags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                              ImPlotFlags_NoBoxSelect | ImPlotFlags_NoInputs | ImPlotFlags_NoMouseText;
    if (!ImPlot::BeginPlot(plot_id, ImVec2(-1.0f, -1.0f), flags))
    {
        return;
    }

    ImPlotAxisFlags xflags = ImPlotAxisFlags_NoHighlight;
    if (!bottom)
    {
        xflags |= ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks;
    }
    ImPlot::SetupAxis(ImAxis_X1, nullptr, xflags);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_Opposite | ImPlotAxisFlags_NoHighlight);
    ImPlot::SetupAxisLimits(ImAxis_X1, win.x_min, win.x_max, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, ylim.min, ylim.max, ImPlotCond_Always);
    ImPlot::SetupAxisFormat(ImAxis_Y1, formatYTick, nullptr);
    if (bottom)
    {
        buildTimeTicks(bars, win, timezone, settings.bar_spacing_px, view);
        if (!view.tick_xs.empty())
        {
            ImPlot::SetupAxisTicks(ImAxis_X1, view.tick_xs.data(), static_cast<int>(view.tick_xs.size()),
                                   view.tick_ptrs.data(), false);
        }
        else
        {
            ImPlot::SetupAxisFormat(ImAxis_X1, formatXTick, &bars);
        }
    }
    ImPlot::SetupFinish();

    if (price)
    {
        view.last_plot_w = ImPlot::GetPlotSize().x;
        view.last_plot_h = ImPlot::GetPlotSize().y;
        if (solo)
        {
            win = computeVisibleWindow(bar_count, view.last_plot_w, settings.bar_spacing_px,
                                       view.scroll_from_end, kChartRightFillBars);
            const OverlayYExtent overlay = overlayYExtent(studies, win, bar_count);
            ylim = computeYLimits(bars, win, settings, view, overlay);
        }
    }

    handlePlotInput(bars, settings, view, win, ylim, x_handled, region_scale, clamp_scroll);
    drawSessionGuides(bars, win, timezone);

    const bool stems_only = settings.bar_spacing_px < 2.0f;
    if (price)
    {
        ImDrawList* draw_list = ImPlot::GetPlotDrawList();
        ImPlot::PushPlotClipRect();
        const ImU32 up = ImGui::ColorConvertFloat4ToU32(Theme::kUp);
        const ImU32 down = ImGui::ColorConvertFloat4ToU32(Theme::kDown);
        const double half_width = 0.5 * static_cast<double>(settings.bar_width_frac);
        const int draw_first = std::max(0, win.first);
        const int draw_last = std::min(bar_count - 1, win.last);
        for (int i = draw_first; i <= draw_last; ++i)
        {
            const Bar& bar = bars[static_cast<std::size_t>(i)];
            const ImU32 color = bar.close >= bar.open ? up : down;
            const ImVec2 high = ImPlot::PlotToPixels(static_cast<double>(i), bar.high);
            const ImVec2 low = ImPlot::PlotToPixels(static_cast<double>(i), bar.low);
            draw_list->AddLine(high, low, color);
            if (stems_only)
            {
                continue;
            }
            const ImVec2 open_pos = ImPlot::PlotToPixels(static_cast<double>(i) - half_width, bar.open);
            const ImVec2 close_pos = ImPlot::PlotToPixels(static_cast<double>(i) + half_width, bar.close);
            const float y0 = std::min(open_pos.y, close_pos.y);
            float y1 = std::max(open_pos.y, close_pos.y);
            if (y1 - y0 < 1.0f)
            {
                y1 = y0 + 1.0f;
            }
            draw_list->AddRectFilled(ImVec2(open_pos.x, y0), ImVec2(close_pos.x, y1), color);
        }
        ImPlot::PopPlotClipRect();
    }

    drawStudyRegion(studies, chart_region, win, bar_count, bars, settings.bar_width_frac, stems_only);
    drawCrosshair(bars, timezone, ylim, studies, chart_region, region_count);
    ImPlot::EndPlot();
}

}  // namespace

void drawCandlesticks(std::span<const Bar> bars,
                      CChartSettings& settings,
                      CChartViewState& view,
                      std::string_view timezone,
                      std::span<const CStudySeries> studies)
{
    if (bars.empty())
    {
        return;
    }

    const int bar_count = static_cast<int>(bars.size());
    const int region_count = studyChartRegionCount(studies);
    const ChartVisibleWindow win =
        computeVisibleWindow(bar_count, view.last_plot_w, settings.bar_spacing_px, view.scroll_from_end,
                             kChartRightFillBars);
    view.scroll_from_end = win.scroll;

    if (region_count <= 1)
    {
        drawChartRegion(bars, settings, view, timezone, studies, win, kStudyMainChartRegion, 1, nullptr,
                        nullptr, true);
        return;
    }

    ensureRegionRatios(view, region_count);
    const ImPlotSubplotFlags subplot_flags = ImPlotSubplotFlags_NoTitle | ImPlotSubplotFlags_NoMenus;
    if (!ImPlot::BeginSubplots("##chart_regions", region_count, 1, ImVec2(-1.0f, -1.0f), subplot_flags,
                               view.region_ratios.data(), nullptr))
    {
        return;
    }
    bool x_handled = false;
    for (int region = kStudyMainChartRegion; region <= region_count; ++region)
    {
        StudyRegionScale* scale = nullptr;
        if (region != kStudyMainChartRegion)
        {
            scale = &studyRegionScale(view, region);
        }
        drawChartRegion(bars, settings, view, timezone, studies, win, region, region_count, &x_handled,
                        scale, false);
    }
    const ChartVisibleWindow clamped =
        computeVisibleWindow(bar_count, view.last_plot_w, settings.bar_spacing_px, view.scroll_from_end,
                             kChartRightFillBars);
    view.scroll_from_end = clamped.scroll;
    clampV1Limits(settings);
    ImPlot::EndSubplots();
}

}  // namespace terminal
