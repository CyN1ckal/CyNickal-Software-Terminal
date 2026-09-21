#pragma once

#include "chart/CChartSettings.h"
#include "market_data/Types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace terminal {

// Sierra Chart Interactive Scaling Mode (right-click the Values Scale).
enum class ChartInteractiveScale : std::uint8_t
{
    Range = 0,  // drag Y: expand/compress
    Move,       // drag Y: pan
    Locked
};

struct ChartVisibleWindow
{
    int first{};       // first data index in the window (inclusive)
    int last{};        // last data index in the window (inclusive)
    int slot_count{};  // number of bar slots across the plot
    int scroll{};      // clamped scroll_from_end
    double x_min{};
    double x_max{};
};

struct ChartYLimits
{
    double min{};
    double max{};
};

// Per-pane interaction state. Not part of load identity; not persisted.
struct CChartViewState
{
    int scroll_from_end{};
    double extra_pad_frac{};
    double move_offset{};
    double working_range{};
    ChartInteractiveScale interactive{ChartInteractiveScale::Move};
    float last_plot_w{800.0f};
    float last_plot_h{400.0f};
    bool dragging_y{};
    bool dragging_x{};
    bool dragging_plot{};
    std::vector<double> tick_xs;
    std::vector<std::string> tick_labels;
    std::vector<const char*> tick_ptrs;
};

[[nodiscard]] inline ChartVisibleWindow computeVisibleWindow(int bar_count,
                                                             float plot_w,
                                                             float spacing_px,
                                                             int scroll_from_end,
                                                             int right_fill) noexcept
{
    ChartVisibleWindow w;
    if (bar_count <= 0)
    {
        return w;
    }
    const float spacing = std::clamp(spacing_px, kChartMinBarSpacingPx, kChartMaxBarSpacingPx);
    const float width = plot_w > 1.0f ? plot_w : 1.0f;
    w.slot_count = std::max(1, static_cast<int>(std::floor(width / spacing)));
    const int max_scroll = std::max(0, bar_count + right_fill - w.slot_count);
    w.scroll = std::clamp(scroll_from_end, 0, max_scroll);
    w.x_max = static_cast<double>(bar_count - 1 + right_fill - w.scroll) + 0.5;
    w.x_min = w.x_max - static_cast<double>(w.slot_count);
    w.first = std::max(0, static_cast<int>(std::ceil(w.x_min)));
    w.last = std::min(bar_count - 1, static_cast<int>(std::floor(w.x_max)));
    if (w.first > w.last)
    {
        w.first = bar_count - 1;
        w.last = bar_count - 1;
    }
    return w;
}

[[nodiscard]] inline ChartYLimits computeYLimits(std::span<const Bar> bars,
                                                 const ChartVisibleWindow& win,
                                                 const CChartSettings& settings,
                                                 const CChartViewState& view) noexcept
{
    ChartYLimits out;
    if (bars.empty())
    {
        return out;
    }
    const int first = std::clamp(win.first, 0, static_cast<int>(bars.size()) - 1);
    const int last = std::clamp(win.last, first, static_cast<int>(bars.size()) - 1);
    double lo = bars[static_cast<std::size_t>(first)].low;
    double hi = bars[static_cast<std::size_t>(first)].high;
    for (int i = first; i <= last; ++i)
    {
        const Bar& bar = bars[static_cast<std::size_t>(i)];
        lo = std::min(lo, bar.low);
        hi = std::max(hi, bar.high);
    }
    if (lo >= hi)
    {
        const double pad = std::abs(lo) * 0.01;
        const double used = pad > 0.0 ? pad : 1.0;
        lo -= used;
        hi += used;
    }
    const double data_range = hi - lo;
    const double pad = data_range * static_cast<double>(settings.scale_padding_pct) / 100.0;
    const double extra = data_range * view.extra_pad_frac;

    switch (settings.scale_range)
    {
    case ChartScaleRange::ConstantRange:
    {
        double range = view.working_range > 0.0 ? view.working_range : settings.constant_range;
        if (range <= 0.0)
        {
            range = data_range + 2.0 * pad;
        }
        const Bar& last_bar = bars[static_cast<std::size_t>(last)];
        const double center = 0.5 * (last_bar.high + last_bar.low);
        out.min = center - range * 0.5 + view.move_offset;
        out.max = center + range * 0.5 + view.move_offset;
        break;
    }
    case ChartScaleRange::UserDefined:
    {
        if (settings.user_top > settings.user_bottom)
        {
            const double ur = settings.user_top - settings.user_bottom;
            out.min = settings.user_bottom - ur * view.extra_pad_frac + view.move_offset;
            out.max = settings.user_top + ur * view.extra_pad_frac + view.move_offset;
        }
        else
        {
            out.min = lo - pad - extra + view.move_offset;
            out.max = hi + pad + extra + view.move_offset;
        }
        break;
    }
    case ChartScaleRange::Automatic:
    default:
        out.min = lo - pad - extra + view.move_offset;
        out.max = hi + pad + extra + view.move_offset;
        break;
    }
    if (out.max <= out.min)
    {
        out.max = out.min + 1.0;
    }
    return out;
}

inline void resetChartScale(CChartViewState& view) noexcept
{
    view.extra_pad_frac = 0.0;
    view.move_offset = 0.0;
    view.working_range = 0.0;
}

}  // namespace terminal
