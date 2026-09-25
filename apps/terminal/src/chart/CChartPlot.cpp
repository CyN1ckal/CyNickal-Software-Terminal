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
#include <string>
#include <string_view>
#include <utility>

namespace terminal {

void drawChartScaleMenuItems(CChartSettings& settings,
                             CChartViewState& view,
                             const ChartYLimits& ylim,
                             bool ylim_valid)
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
    ImGui::TextColored(Theme::kTextDim, "Ctrl swaps Range and Move while dragging.");
    ImGui::Separator();
    if (ImGui::MenuItem("Scale Range: Automatic", nullptr,
                        settings.scale_range == ChartScaleRange::Automatic))
    {
        settings.scale_range = ChartScaleRange::Automatic;
        resetChartScale(view);
    }
    ImGui::BeginDisabled(!ylim_valid);
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
    ImGui::EndDisabled();
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Scale"))
    {
        resetChartScale(view);
    }
}

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
                    ChartVerticalGrid grid,
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

    const std::vector<ChartAxisTick> ticks = buildChartVerticalGridTicks(bars, win, tz, metrics, grid);
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
    if (ImPlot::IsAxisHovered(ImAxis_Y1))
    {
        view.y_axis_hovered = true;
    }
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
        drawChartScaleMenuItems(settings, view, ylim, true);
        ImGui::EndPopup();
    }
    if (region_scale != nullptr && ImPlot::IsAxisHovered(ImAxis_Y1) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("##study_region_scale_menu");
    }
    if (region_scale != nullptr && ImGui::BeginPopup("##study_region_scale_menu"))
    {
        if (ImGui::MenuItem("Reset Scale"))
        {
            region_scale->extra_pad_frac = 0.0;
            region_scale->move_offset = 0.0;
            dragging_y = false;
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

// Same string the time axis paints for this bar. An installed tick wins. Other
// bars follow that grain. With no calendar ticks, formatXTick is the axis text.
void formatTagX(std::span<const Bar> bars,
                const CChartViewState& view,
                std::string_view tz,
                int idx,
                char* buf,
                int size)
{
    if (buf == nullptr || size <= 0)
    {
        return;
    }
    buf[0] = '\0';
    for (std::size_t i = 0; i < view.tick_xs.size() && i < view.tick_labels.size(); ++i)
    {
        if (std::lround(view.tick_xs[i]) == static_cast<long>(idx))
        {
            std::snprintf(buf, static_cast<std::size_t>(size), "%s", view.tick_labels[i].c_str());
            return;
        }
    }
    if (view.tick_labels.empty())
    {
        formatXTick(static_cast<double>(idx), buf, size, &bars);
        return;
    }
    if (idx < 0 || static_cast<std::size_t>(idx) >= bars.size())
    {
        return;
    }
    bool time_grain = false;
    std::size_t widest = 0;
    for (const std::string& label : view.tick_labels)
    {
        if (label.find(':') != std::string::npos)
        {
            time_grain = true;
        }
        widest = std::max(widest, label.size());
    }
    const ChartLocalTime stamp = chartLocalTime(tz, bars[static_cast<std::size_t>(idx)].ts);
    if (time_grain)
    {
        std::snprintf(buf, static_cast<std::size_t>(size), "%02d:%02d", stamp.hour, stamp.minute);
        return;
    }
    if (widest >= 10)
    {
        std::snprintf(buf, static_cast<std::size_t>(size), "%04d-%02d-%02d", stamp.year, stamp.month,
                      stamp.day);
        return;
    }
    if (widest >= 7)
    {
        std::snprintf(buf, static_cast<std::size_t>(size), "%04d-%02d", stamp.year, stamp.month);
        return;
    }
    std::snprintf(buf, static_cast<std::size_t>(size), "%04d", stamp.year);
}

// Pixel geometry for the value marks. Filled while the plot is current, drawn
// after EndPlot so the marks sit on the axis labels.
struct CrosshairMarks
{
    bool y{false};
    bool x{false};
    ImVec2 plot_min;
    ImVec2 plot_max;
    float y_px{};
    float x_px{};
    char y_text[32]{};
    char x_text[32]{};
};

// ImPlot TagX/TagY call OverrideSizeLate. The next frame sizes the axis from
// that tag, the plot moves under the pointer, and the crosshair flashes.
// These marks paint in the gutter that the tick labels already reserved.
constexpr float kAxisMarkPadX = 3.0f;

void shiftMarkIntoWindow(ImVec2& text_pos, const ImVec2& text_size)
{
    const ImVec2 win_pos = ImGui::GetWindowPos();
    const ImVec2 win_size = ImGui::GetWindowSize();
    const float left = win_pos.x;
    const float right = win_pos.x + win_size.x;
    const float top = win_pos.y;
    const float bottom = win_pos.y + win_size.y;
    const float box_left = text_pos.x - kAxisMarkPadX;
    const float box_right = text_pos.x + text_size.x + kAxisMarkPadX;
    const float box_bottom = text_pos.y + text_size.y;
    if (box_right > right)
    {
        text_pos.x -= box_right - right;
    }
    else if (box_left < left)
    {
        text_pos.x += left - box_left;
    }
    if (box_bottom > bottom)
    {
        text_pos.y -= box_bottom - bottom;
    }
    else if (text_pos.y < top)
    {
        text_pos.y += top - text_pos.y;
    }
}

void drawAxisValueMark(ImDrawList* draw, ImVec2 text_pos, const char* text)
{
    if (draw == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    shiftMarkIntoWindow(text_pos, text_size);
    const ImVec2 box_min(text_pos.x - kAxisMarkPadX, text_pos.y);
    const ImVec2 box_max(text_pos.x + text_size.x + kAxisMarkPadX, text_pos.y + text_size.y);
    draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(Theme::kAccent));
    draw->AddText(text_pos, ImGui::GetColorU32(Theme::kBg0), text);
}

void drawCrosshairMarks(const CrosshairMarks& marks)
{
    if (!marks.y && !marks.x)
    {
        return;
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 label_pad = ImPlot::GetStyle().LabelPadding;
    if (marks.y && marks.y_text[0] != '\0')
    {
        const ImVec2 text_size = ImGui::CalcTextSize(marks.y_text);
        float text_y = marks.y_px - (text_size.y * 0.5f);
        const float max_y = marks.plot_max.y - text_size.y;
        if (max_y >= marks.plot_min.y)
        {
            text_y = std::clamp(text_y, marks.plot_min.y, max_y);
        }
        drawAxisValueMark(draw, ImVec2(marks.plot_max.x + label_pad.x, text_y), marks.y_text);
    }
    if (marks.x && marks.x_text[0] != '\0')
    {
        const ImVec2 text_size = ImGui::CalcTextSize(marks.x_text);
        float text_x = marks.x_px - (text_size.x * 0.5f);
        const float max_x = marks.plot_max.x - text_size.x;
        if (max_x >= marks.plot_min.x)
        {
            text_x = std::clamp(text_x, marks.plot_min.x, max_x);
        }
        drawAxisValueMark(draw, ImVec2(text_x, marks.plot_max.y + label_pad.y), marks.x_text);
    }
}

CrosshairMarks drawCrosshair(std::span<const Bar> bars,
                             std::string_view tz,
                             const CChartViewState& view,
                             const ChartYLimits& ylim,
                             std::span<const CStudySeries> studies,
                             int chart_region,
                             int region_count)
{
    CrosshairMarks marks;
    if (!view.crosshair || bars.empty())
    {
        return marks;
    }
    const ImVec2 pos = ImPlot::GetPlotPos();
    const ImVec2 size = ImPlot::GetPlotSize();
    const ImVec2 mouse = ImGui::GetMousePos();
    // Geometric, and only while this pane is the hovered window. A parked pointer
    // keeps the same bar. Another pane, or the right-click menu, does not steal it.
    const bool window_hovered = ImGui::IsWindowHovered();
    const bool x_inside = window_hovered && size.x > 0.0f && mouse.x >= pos.x && mouse.x <= pos.x + size.x;
    const bool y_inside = window_hovered && size.y > 0.0f && mouse.y >= pos.y && mouse.y <= pos.y + size.y;
    const bool inside = x_inside && y_inside;
    // A stack keeps the vertical line in regions that share the pointer's X.
    const bool column = region_count > 1 && x_inside;
    if (!inside && !column)
    {
        return marks;
    }

    const float sample_y = pos.y + (size.y * 0.5f);
    const ImPlotPoint at = ImPlot::PixelsToPlot(mouse.x, sample_y, ImAxis_X1, ImAxis_Y1);
    int idx = static_cast<int>(std::lround(at.x));
    idx = std::clamp(idx, 0, static_cast<int>(bars.size()) - 1);
    const Bar& bar = bars[static_cast<std::size_t>(idx)];

    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(Theme::kAccent);
    ImPlot::PushPlotClipRect();
    const ImVec2 top = ImPlot::PlotToPixels(static_cast<double>(idx), ylim.max);
    const ImVec2 bot = ImPlot::PlotToPixels(static_cast<double>(idx), ylim.min);
    draw_list->AddLine(top, bot, color, 1.0f);
    marks.plot_min = pos;
    marks.plot_max = ImVec2(pos.x + size.x, pos.y + size.y);
    marks.x_px = top.x;
    if (inside)
    {
        const double price = ImPlot::GetPlotMousePos().y;
        const ImVec2 left = ImPlot::PlotToPixels(ImPlot::GetPlotLimits().X.Min, price);
        const ImVec2 right = ImPlot::PlotToPixels(ImPlot::GetPlotLimits().X.Max, price);
        draw_list->AddLine(left, right, color, 1.0f);
        marks.y = true;
        marks.y_px = left.y;
        formatYTick(price, marks.y_text, static_cast<int>(sizeof(marks.y_text)), nullptr);
    }
    ImPlot::PopPlotClipRect();

    const bool bottom = chart_region == region_count;
    if (bottom)
    {
        marks.x = true;
        formatTagX(bars, view, tz, idx, marks.x_text, static_cast<int>(sizeof(marks.x_text)));
    }

    const bool popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup);
    if (!inside || popup || ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        return marks;
    }
    const ChartLocalTime stamp = chartLocalTime(tz, bar.ts);
    char time_buf[32];
    std::snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d", stamp.year, stamp.month,
                  stamp.day, stamp.hour, stamp.minute);
    if (!ImGui::BeginTooltip())
    {
        return marks;
    }
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
        const std::uint32_t tint = studyHistogramColor(series, bar.close >= bar.open);
        const ImVec4 ink = ImGui::ColorConvertU32ToFloat4(tint);
        if (series.value_decimals <= 0)
        {
            ImGui::TextColored(ink, "%s  %.0f", series.label.c_str(), value);
        }
        else
        {
            ImGui::TextColored(ink, "%s  %.*f", series.label.c_str(), series.value_decimals, value);
        }
    }
    ImGui::EndTooltip();
    return marks;
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

// SetupAxisTicks stores custom positions as minor ticks. ImPlot then multiplies
// those grid lines by MinorAlpha and skips them once the pane is short, so a
// calendar line goes faint and a 48-line manual scale disappears.
void drawInstalledGridLines(const std::vector<double>& xs, const std::vector<double>& ys)
{
    if (xs.empty() && ys.empty())
    {
        return;
    }
    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(ImPlot::GetStyle().Colors[ImPlotCol_AxisGrid]);
    const ImVec2 origin = ImPlot::GetPlotPos();
    const ImVec2 size = ImPlot::GetPlotSize();
    const ImVec2 plot_max(origin.x + size.x, origin.y + size.y);
    const ImVec2 grid_px = ImPlot::GetStyle().MajorGridSize;
    ImPlot::PushPlotClipRect();
    for (const double x : xs)
    {
        const float px = ImPlot::PlotToPixels(x, 0.0).x;
        draw_list->AddLine(ImVec2(px, origin.y), ImVec2(px, plot_max.y), color, grid_px.x);
    }
    for (const double y : ys)
    {
        const float py = ImPlot::PlotToPixels(0.0, y).y;
        draw_list->AddLine(ImVec2(origin.x, py), ImVec2(plot_max.x, py), color, grid_px.y);
    }
    ImPlot::PopPlotClipRect();
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
    // Tick nibs and the faint minor lattice, on every region. Grid lines and
    // labels stay. Not the subplot padding push: that zero inset closes the gap
    // between regions and must not move.
    const bool hide_tick_marks = true;
    if (hide_tick_marks)
    {
        ImPlot::PushStyleVar(ImPlotStyleVar_MajorTickLen, ImVec2(0.0f, 0.0f));
        ImPlot::PushStyleVar(ImPlotStyleVar_MinorTickLen, ImVec2(0.0f, 0.0f));
        ImPlot::PushStyleVar(ImPlotStyleVar_MinorAlpha, 0.0f);
    }
    if (!ImPlot::BeginPlot(plot_id, ImVec2(-1.0f, -1.0f), flags))
    {
        if (hide_tick_marks)
        {
            ImPlot::PopStyleVar(3);
        }
        return;
    }

    // Mono for tick measurement and the labels SetupFinish draws. Popped before
    // input so the scale menu keeps the UI face, then pushed again around the
    // readout and the axis marks drawn after EndPlot.
    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }

    buildTimeTicks(bars, win, timezone, settings.bar_spacing_px, settings.vertical_grid, view);

    std::vector<double> y_ticks;
    std::vector<std::string> y_labels;
    std::vector<const char*> y_ptrs;
    ImPlotAxisFlags yflags = ImPlotAxisFlags_Opposite | ImPlotAxisFlags_NoHighlight;
    if (settings.horizontal_grid == ChartHorizontalGrid::Off)
    {
        yflags |= ImPlotAxisFlags_NoGridLines;
    }
    if (price && settings.horizontal_grid == ChartHorizontalGrid::Manual)
    {
        y_ticks = buildHorizontalGridTicks(ylim.min, ylim.max, settings.horizontal_grid_spacing);
        if (y_ticks.empty())
        {
            yflags |= ImPlotAxisFlags_NoGridLines;
        }
        else
        {
            // formatYTick's fixed precision merges distinct manual levels.
            double label_step = settings.horizontal_grid_spacing;
            if (y_ticks.size() >= 2)
            {
                label_step = y_ticks[1] - y_ticks[0];
            }
            const int decimals = horizontalGridDecimals(label_step);
            y_labels.reserve(y_ticks.size());
            for (const double level : y_ticks)
            {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.*f", decimals, level);
                y_labels.emplace_back(buf);
            }
            y_ptrs.reserve(y_labels.size());
            for (const std::string& label : y_labels)
            {
                y_ptrs.push_back(label.c_str());
            }
        }
    }

    ImPlotAxisFlags xflags = ImPlotAxisFlags_NoHighlight;
    if (!bottom)
    {
        xflags |= ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks;
    }
    // Installed ticks are drawn in drawInstalledGridLines. Leaving ImPlot's grid
    // on paints the same positions again as faint minor lines.
    if (!view.tick_xs.empty())
    {
        xflags |= ImPlotAxisFlags_NoGridLines;
    }
    if (!y_ticks.empty())
    {
        yflags |= ImPlotAxisFlags_NoGridLines;
    }
    ImPlot::SetupAxis(ImAxis_X1, nullptr, xflags);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, yflags);
    ImPlot::SetupAxisLimits(ImAxis_X1, win.x_min, win.x_max, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, ylim.min, ylim.max, ImPlotCond_Always);
    ImPlot::SetupAxisFormat(ImAxis_Y1, formatYTick, nullptr);
    if (!view.tick_xs.empty())
    {
        ImPlot::SetupAxisTicks(ImAxis_X1, view.tick_xs.data(), static_cast<int>(view.tick_xs.size()),
                               view.tick_ptrs.data(), false);
    }
    else if (bottom)
    {
        ImPlot::SetupAxisFormat(ImAxis_X1, formatXTick, &bars);
    }
    if (!y_ticks.empty())
    {
        ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks.data(), static_cast<int>(y_ticks.size()), y_ptrs.data(),
                               false);
    }
    ImPlot::SetupFinish();
    drawInstalledGridLines(view.tick_xs, y_ticks);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }

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
        view.price_ylim = ylim;
        view.price_ylim_valid = true;
    }

    handlePlotInput(bars, settings, view, win, ylim, x_handled, region_scale, clamp_scroll);

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
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    const CrosshairMarks marks =
        drawCrosshair(bars, timezone, view, ylim, studies, chart_region, region_count);
    ImPlot::EndPlot();
    drawCrosshairMarks(marks);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
    if (hide_tick_marks)
    {
        ImPlot::PopStyleVar(3);
    }
}

}  // namespace

void drawCandlesticks(std::span<const Bar> bars,
                      CChartSettings& settings,
                      CChartViewState& view,
                      std::string_view timezone,
                      std::span<const CStudySeries> studies)
{
    view.price_ylim_valid = false;
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

    // Subplot cells split PlotPadding in half and insert it between regions.
    // Zero closes that gap and the frame inset. One pixel keeps price text off the border.
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0.0f, 0.0f));
    ImPlot::PushStyleVar(ImPlotStyleVar_LabelPadding, ImVec2(1.0f, 0.0f));
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotMinSize, ImVec2(0.0f, 0.0f));

    if (region_count <= 1)
    {
        drawChartRegion(bars, settings, view, timezone, studies, win, kStudyMainChartRegion, 1, nullptr,
                        nullptr, true);
    }
    else
    {
        ensureRegionRatios(view, region_count);
        const ImPlotSubplotFlags subplot_flags = ImPlotSubplotFlags_NoTitle | ImPlotSubplotFlags_NoMenus;
        if (ImPlot::BeginSubplots("##chart_regions", region_count, 1, ImVec2(-1.0f, -1.0f), subplot_flags,
                                  view.region_ratios.data(), nullptr))
        {
            bool x_handled = false;
            for (int region = kStudyMainChartRegion; region <= region_count; ++region)
            {
                StudyRegionScale* scale = nullptr;
                if (region != kStudyMainChartRegion)
                {
                    scale = &studyRegionScale(view, region);
                }
                drawChartRegion(bars, settings, view, timezone, studies, win, region, region_count,
                                &x_handled, scale, false);
            }
            const ChartVisibleWindow clamped =
                computeVisibleWindow(bar_count, view.last_plot_w, settings.bar_spacing_px,
                                     view.scroll_from_end, kChartRightFillBars);
            view.scroll_from_end = clamped.scroll;
            clampV1Limits(settings);
            ImPlot::EndSubplots();
        }
    }

    ImPlot::PopStyleVar(3);
}

}  // namespace terminal
