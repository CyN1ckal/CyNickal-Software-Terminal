// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartPlot.h"

#include "market_data/Time.h"
#include "ui/Theme.h"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <exception>
#include <string>
#include <string_view>

namespace terminal {
namespace {

struct LocalStamp
{
    int year{};
    int month{};
    int day{};
    int hour{};
    int minute{};
};

[[nodiscard]] LocalStamp utcToLocalStamp(std::string_view tz, UnixSeconds ts)
{
    LocalStamp out{};
    try
    {
        const std::string key(tz.empty() ? "UTC" : tz);
        const std::chrono::time_zone* zone = std::chrono::locate_zone(key);
        const std::chrono::sys_seconds tp{std::chrono::seconds{ts}};
        const auto local = zone->to_local(tp);
        const auto day = std::chrono::floor<std::chrono::days>(local);
        const std::chrono::year_month_day ymd{day};
        const std::chrono::hh_mm_ss hms{local - day};
        out.year = static_cast<int>(ymd.year());
        out.month = static_cast<int>(static_cast<unsigned>(ymd.month()));
        out.day = static_cast<int>(static_cast<unsigned>(ymd.day()));
        out.hour = static_cast<int>(hms.hours().count());
        out.minute = static_cast<int>(hms.minutes().count());
        return out;
    }
    catch (const std::exception&)
    {
        const auto t = static_cast<std::time_t>(ts);
        std::tm utc{};
        if (gmtime_r(&t, &utc) != nullptr)
        {
            out.year = utc.tm_year + 1900;
            out.month = utc.tm_mon + 1;
            out.day = utc.tm_mday;
            out.hour = utc.tm_hour;
            out.minute = utc.tm_min;
        }
        return out;
    }
}

int formatXTick(double value, char* buf, int size, void* data)
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
    if (gmtime_r(&t, &utc) == nullptr)
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
    if (bars.empty() || win.first > win.last)
    {
        return;
    }

    const int min_step =
        std::max(1, static_cast<int>(std::lround(80.0 / static_cast<double>(spacing_px))));
    int last_tick = win.first - min_step;
    SessionDate prev_date = 0;
    bool have_date = false;

    for (int i = win.first; i <= win.last; ++i)
    {
        SessionDate date = 0;
        try
        {
            date = utcToSessionDate(tz.empty() ? "UTC" : tz, bars[static_cast<std::size_t>(i)].ts);
        }
        catch (const std::exception&)
        {
            date = 0;
        }
        const bool session_start = !have_date || date != prev_date;
        prev_date = date;
        have_date = true;
        if (!session_start && (i - last_tick) < min_step)
        {
            continue;
        }

        const LocalStamp stamp = utcToLocalStamp(tz, bars[static_cast<std::size_t>(i)].ts);
        char buf[32];
        if (session_start)
        {
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", stamp.year, stamp.month, stamp.day);
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "%02d:%02d", stamp.hour, stamp.minute);
        }
        view.tick_xs.push_back(static_cast<double>(i));
        view.tick_labels.emplace_back(buf);
        last_tick = i;
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
                     const ChartYLimits& ylim)
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

    const float plot_h = std::max(1.0f, view.last_plot_h);
    const float spacing = settings.bar_spacing_px;

    if (ImPlot::IsPlotHovered() && io.MouseWheel != 0.0f)
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
            const double new_x_min = idx - rel * static_cast<double>(after.slot_count);
            const double new_x_max = new_x_min + static_cast<double>(after.slot_count);
            view.scroll_from_end = static_cast<int>(std::lround(
                static_cast<double>(static_cast<int>(bars.size()) - 1 + kChartRightFillBars) + 0.5 -
                new_x_max));
        }
    }

    if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        view.dragging_plot = true;
    }
    if (view.dragging_plot)
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

    if (ImPlot::IsAxisHovered(ImAxis_X1) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        view.dragging_x = true;
    }
    if (view.dragging_x)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            settings.bar_spacing_px = std::clamp(settings.bar_spacing_px - io.MouseDelta.x * 0.05f,
                                                 kChartMinBarSpacingPx, kChartMaxBarSpacingPx);
        }
        else
        {
            view.dragging_x = false;
        }
    }

    if (ImPlot::IsAxisHovered(ImAxis_Y1) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        view.dragging_y = true;
    }
    if (ImPlot::IsAxisHovered(ImAxis_Y1) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        resetChartScale(view);
        view.dragging_y = false;
    }
    if (view.dragging_y)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && mode != ChartInteractiveScale::Locked)
        {
            if (mode == ChartInteractiveScale::Move)
            {
                const double price_per_px = (ylim.max - ylim.min) / static_cast<double>(plot_h);
                view.move_offset += static_cast<double>(io.MouseDelta.y) * price_per_px;
            }
            else
            {
                const auto delta = static_cast<double>(io.MouseDelta.y / plot_h);
                if (settings.scale_range == ChartScaleRange::ConstantRange)
                {
                    double range = view.working_range > 0.0 ? view.working_range : (ylim.max - ylim.min);
                    range *= 1.0 + delta;
                    view.working_range = std::max(range, 1e-6);
                }
                else
                {
                    view.extra_pad_frac += 0.5 * delta;
                    view.extra_pad_frac = std::clamp(view.extra_pad_frac, -0.5, 8.0);
                }
            }
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            view.dragging_y = false;
        }
    }

    if (ImPlot::IsAxisHovered(ImAxis_Y1) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("##chart_scale_menu");
    }
    if (ImGui::BeginPopup("##chart_scale_menu"))
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

    const ChartVisibleWindow clamped = computeVisibleWindow(
        static_cast<int>(bars.size()), view.last_plot_w, settings.bar_spacing_px, view.scroll_from_end,
        kChartRightFillBars);
    view.scroll_from_end = clamped.scroll;
    clampV1Limits(settings);
}

void drawCrosshair(std::span<const Bar> bars, std::string_view tz, const ChartYLimits& ylim)
{
    if (!ImPlot::IsPlotHovered() || bars.empty())
    {
        return;
    }
    int idx = static_cast<int>(std::lround(ImPlot::GetPlotMousePos().x));
    idx = std::clamp(idx, 0, static_cast<int>(bars.size()) - 1);
    const Bar& bar = bars[static_cast<std::size_t>(idx)];
    const double price = ImPlot::GetPlotMousePos().y;

    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(Theme::kHairline);
    ImPlot::PushPlotClipRect();
    const ImVec2 top = ImPlot::PlotToPixels(static_cast<double>(idx), ylim.max);
    const ImVec2 bot = ImPlot::PlotToPixels(static_cast<double>(idx), ylim.min);
    const ImVec2 left = ImPlot::PlotToPixels(ImPlot::GetPlotLimits().X.Min, price);
    const ImVec2 right = ImPlot::PlotToPixels(ImPlot::GetPlotLimits().X.Max, price);
    draw_list->AddLine(top, bot, color);
    draw_list->AddLine(left, right, color);
    ImPlot::PopPlotClipRect();

    const LocalStamp stamp = utcToLocalStamp(tz, bar.ts);
    char time_buf[32];
    std::snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d", stamp.year, stamp.month,
                  stamp.day, stamp.hour, stamp.minute);
    ImPlot::TagX(static_cast<double>(idx), Theme::kAmber, "%s", time_buf);
    ImPlot::TagY(price, Theme::kAmber, "%.4f", price);

    ImGui::BeginTooltip();
    ImGui::Text("%s  O  %.4f  H  %.4f  L  %.4f  C  %.4f  V  %.0f", time_buf, bar.open, bar.high,
                bar.low, bar.close, bar.volume);
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

}  // namespace

void drawCandlesticks(std::span<const Bar> bars,
                      CChartSettings& settings,
                      CChartViewState& view,
                      std::string_view timezone)
{
    if (bars.empty())
    {
        return;
    }

    const int n = static_cast<int>(bars.size());
    ChartVisibleWindow win = computeVisibleWindow(n, view.last_plot_w, settings.bar_spacing_px,
                                                  view.scroll_from_end, kChartRightFillBars);
    view.scroll_from_end = win.scroll;
    ChartYLimits ylim = computeYLimits(bars, win, settings, view);

    const ImPlotFlags flags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                              ImPlotFlags_NoBoxSelect | ImPlotFlags_NoInputs | ImPlotFlags_NoMouseText;
    if (!ImPlot::BeginPlot("##candles", ImVec2(-1.0f, -1.0f), flags))
    {
        return;
    }

    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoHighlight);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_Opposite | ImPlotAxisFlags_NoHighlight);
    ImPlot::SetupAxisLimits(ImAxis_X1, win.x_min, win.x_max, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, ylim.min, ylim.max, ImPlotCond_Always);
    ImPlot::SetupAxisFormat(ImAxis_Y1, formatYTick, nullptr);
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
    ImPlot::SetupFinish();

    view.last_plot_w = ImPlot::GetPlotSize().x;
    view.last_plot_h = ImPlot::GetPlotSize().y;
    win = computeVisibleWindow(n, view.last_plot_w, settings.bar_spacing_px, view.scroll_from_end,
                               kChartRightFillBars);
    ylim = computeYLimits(bars, win, settings, view);

    handlePlotInput(bars, settings, view, win, ylim);

    drawSessionGuides(bars, win, timezone);

    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    ImPlot::PushPlotClipRect();
    const ImU32 up = ImGui::ColorConvertFloat4ToU32(Theme::kUp);
    const ImU32 down = ImGui::ColorConvertFloat4ToU32(Theme::kDown);
    const bool stems_only = settings.bar_spacing_px < 2.0f;
    const double half_width = 0.5 * static_cast<double>(settings.bar_width_frac);

    const int draw_first = std::max(0, win.first);
    const int draw_last = std::min(n - 1, win.last);
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

    drawCrosshair(bars, timezone, ylim);
    ImPlot::EndPlot();
}

}  // namespace terminal
