// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartAxis.h"

#include "market_data/Time.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <exception>
#include <string_view>
#include <utility>
#include <vector>

namespace terminal {
namespace {

enum class LabelKind : std::uint8_t
{
    Time,
    Date,
    Month,
    Year,
};

struct Mark
{
    ChartLocalTime stamp{};
    int session_origin{0};
    int week_key{0};
    bool session_start{false};
};

struct Placed
{
    int index{0};
    LabelKind kind{LabelKind::Time};
    float width{0.0f};
};

constexpr int kTimeStepsS[] = {60, 120, 300, 600, 900, 1800, 3600, 7200, 10800, 14400, 21600};
constexpr int kYearSteps[] = {1, 2, 5, 10, 20, 50, 100};

// 0.1 is not a binary fraction, so a product such as 0.1 * 10 is not an integer
// bit-for-bit. This slack still treats that product as sitting on the lattice.
constexpr double kGridRelativeEps = 1.0e-8;

[[nodiscard]] int dateKey(const ChartLocalTime& stamp) noexcept
{
    return (stamp.year * 10000) + (stamp.month * 100) + stamp.day;
}

[[nodiscard]] int mondayKey(int year, int month, int day) noexcept
{
    const int fallback = (year * 10000) + (month * 100) + day;
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31)
    {
        return fallback;
    }
    const std::chrono::year_month_day ymd{std::chrono::year{year},
                                          std::chrono::month{static_cast<unsigned>(month)},
                                          std::chrono::day{static_cast<unsigned>(day)}};
    if (!ymd.ok())
    {
        return fallback;
    }
    const std::chrono::sys_days day_tp{ymd};
    const std::chrono::weekday weekday{day_tp};
    const auto since_monday = (weekday.c_encoding() + 6U) % 7U;
    const std::chrono::year_month_day monday{day_tp - std::chrono::days{since_monday}};
    const auto y = static_cast<int>(monday.year());
    const auto m = static_cast<int>(static_cast<unsigned>(monday.month()));
    const auto d = static_cast<int>(static_cast<unsigned>(monday.day()));
    return (y * 10000) + (m * 100) + d;
}

[[nodiscard]] float kindWidth(LabelKind kind, const ChartTickMetrics& metrics) noexcept
{
    switch (kind)
    {
    case LabelKind::Time:
        return metrics.time_px;
    case LabelKind::Month:
        return metrics.month_px;
    case LabelKind::Year:
        return metrics.year_px;
    case LabelKind::Date:
        return metrics.date_px;
    }
    return metrics.date_px;
}

[[nodiscard]] std::string formatLabel(LabelKind kind, const ChartLocalTime& stamp)
{
    char buf[32]{};
    switch (kind)
    {
    case LabelKind::Time:
        std::snprintf(buf, sizeof(buf), "%02d:%02d", stamp.hour, stamp.minute);
        break;
    case LabelKind::Month:
        std::snprintf(buf, sizeof(buf), "%04d-%02d", stamp.year, stamp.month);
        break;
    case LabelKind::Year:
        std::snprintf(buf, sizeof(buf), "%04d", stamp.year);
        break;
    case LabelKind::Date:
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", stamp.year, stamp.month, stamp.day);
        break;
    }
    return std::string{buf};
}

[[nodiscard]] const Mark& markAt(const std::vector<Mark>& marks, int origin, int index)
{
    return marks[static_cast<std::size_t>(index - origin)];
}

[[nodiscard]] bool labelClearsPriceScale(int index,
                                         float width,
                                         const ChartVisibleWindow& win,
                                         float spacing) noexcept
{
    const double span = win.x_max - win.x_min;
    if (!(span > 0.0) || !(spacing > 0.0f))
    {
        return true;
    }
    const auto plot_w = static_cast<float>(span * static_cast<double>(spacing));
    const auto px = static_cast<float>((static_cast<double>(index) - win.x_min) * static_cast<double>(spacing));
    return px + (width * 0.5f) <= plot_w + 1.0f;
}

[[nodiscard]] bool collides(int index_a, float width_a, int index_b, float width_b, float spacing, float gap) noexcept
{
    const int delta = index_a > index_b ? index_a - index_b : index_b - index_a;
    const auto dist = static_cast<float>(delta) * spacing;
    return dist + 0.01f < ((0.5f * (width_a + width_b)) + gap);
}

[[nodiscard]] bool seriesFits(const std::vector<int>& indices, float width, float spacing, float gap) noexcept
{
    const float need = width + gap;
    for (std::size_t i = 1; i < indices.size(); ++i)
    {
        const auto dist = static_cast<float>(indices[i] - indices[i - 1]) * spacing;
        if (dist + 0.01f < need)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] int findSessionOrigin(std::span<const Bar> bars, int index, std::string_view timezone)
{
    const int date = dateKey(chartLocalTime(timezone, bars[static_cast<std::size_t>(index)].ts));
    while (index > 0)
    {
        const int prev = dateKey(chartLocalTime(timezone, bars[static_cast<std::size_t>(index - 1)].ts));
        if (prev != date)
        {
            break;
        }
        --index;
    }
    return index;
}

[[nodiscard]] std::vector<Mark> buildMarks(std::span<const Bar> bars,
                                           int origin,
                                           int last,
                                           std::string_view timezone)
{
    std::vector<Mark> marks;
    const auto mark_count = static_cast<std::size_t>(last) - static_cast<std::size_t>(origin) + 1U;
    marks.reserve(mark_count);
    int prev_date = -1;
    if (origin > 0)
    {
        prev_date = dateKey(chartLocalTime(timezone, bars[static_cast<std::size_t>(origin - 1)].ts));
    }
    int session_origin = origin;
    int week_key = 0;
    for (int i = origin; i <= last; ++i)
    {
        Mark mark;
        mark.stamp = chartLocalTime(timezone, bars[static_cast<std::size_t>(i)].ts);
        const int date = dateKey(mark.stamp);
        mark.session_start = date != prev_date;
        if (mark.session_start)
        {
            session_origin = i;
            week_key = mondayKey(mark.stamp.year, mark.stamp.month, mark.stamp.day);
        }
        mark.session_origin = session_origin;
        mark.week_key = week_key;
        prev_date = date;
        marks.push_back(mark);
    }
    return marks;
}

[[nodiscard]] std::vector<int> ticksOnGrid(std::span<const Bar> bars,
                                           const std::vector<Mark>& marks,
                                           int origin,
                                           int first,
                                           int last,
                                           int time_step_s)
{
    std::vector<int> out;
    int open_index = -1;
    UnixSeconds open_ts = 0;
    const auto step = static_cast<UnixSeconds>(time_step_s);
    for (int i = first; i <= last; ++i)
    {
        const Mark& mark = markAt(marks, origin, i);
        if (mark.session_origin != open_index)
        {
            open_index = mark.session_origin;
            open_ts = bars[static_cast<std::size_t>(open_index)].ts;
        }
        const UnixSeconds delta = bars[static_cast<std::size_t>(i)].ts - open_ts;
        if (delta < 0 || step <= 0 || delta % step != 0)
        {
            continue;
        }
        if (!out.empty() && out.back() >= open_index &&
            bars[static_cast<std::size_t>(out.back())].ts - open_ts == delta)
        {
            continue;
        }
        out.push_back(i);
    }
    return out;
}

// Smallest gap between candidates that share a session. Zero when no session
// holds two candidates. Cross-session gaps are ignored so a tight close/open
// pair does not reject an otherwise round clock step.
[[nodiscard]] int minSameSessionGap(const std::vector<int>& candidates,
                                    const std::vector<Mark>& marks,
                                    int origin)
{
    int gap = 0;
    for (std::size_t i = 1; i < candidates.size(); ++i)
    {
        const int left = candidates[i - 1];
        const int right = candidates[i];
        if (markAt(marks, origin, left).session_origin != markAt(marks, origin, right).session_origin)
        {
            continue;
        }
        const int delta = right - left;
        if (gap == 0 || delta < gap)
        {
            gap = delta;
        }
    }
    return gap;
}

// Drop a candidate that would overlap the previous kept label. A session open
// wins over interior times when only one of them fits.
[[nodiscard]] std::vector<int> spaceIndices(const std::vector<int>& candidates,
                                            const std::vector<Mark>& marks,
                                            int origin,
                                            float spacing,
                                            float need)
{
    std::vector<int> kept;
    auto opens = [&](int index) {
        return markAt(marks, origin, index).session_start;
    };
    for (const int index : candidates)
    {
        if (opens(index))
        {
            std::size_t trim = kept.size();
            while (trim > 0 && !opens(kept[trim - 1]))
            {
                const auto dist = static_cast<float>(index - kept[trim - 1]) * spacing;
                if (dist + 0.01f >= need)
                {
                    break;
                }
                --trim;
            }
            const bool fits = trim == 0 ||
                              (static_cast<float>(index - kept[trim - 1]) * spacing) + 0.01f >= need;
            if (fits)
            {
                kept.resize(trim);
                kept.push_back(index);
            }
            continue;
        }
        if (kept.empty() || (static_cast<float>(index - kept.back()) * spacing) + 0.01f >= need)
        {
            kept.push_back(index);
        }
    }
    return kept;
}

// Insert or widen a date label. Interior times that would overlap it are removed.
// Returns false, and leaves ticks unchanged, when the date would overlap another date.
bool placeDate(std::vector<Placed>& ticks, int index, float date_px, float spacing, float gap)
{
    std::vector<Placed> next;
    next.reserve(ticks.size() + 1);
    bool replaced = false;
    for (const Placed& tick : ticks)
    {
        if (tick.index == index)
        {
            next.push_back(Placed{.index=index, .kind=LabelKind::Date, .width=date_px});
            replaced = true;
            continue;
        }
        if (collides(tick.index, tick.width, index, date_px, spacing, gap))
        {
            if (tick.kind != LabelKind::Time)
            {
                return false;
            }
            continue;
        }
        next.push_back(tick);
    }
    if (!replaced)
    {
        auto where = next.begin();
        while (where != next.end() && where->index < index)
        {
            ++where;
        }
        next.insert(where, Placed{.index=index, .kind=LabelKind::Date, .width=date_px});
    }
    ticks.swap(next);
    return true;
}

void upgradeSessionDates(std::vector<Placed>& ticks,
                         const std::vector<Mark>& marks,
                         int origin,
                         int first,
                         int last,
                         float date_px,
                         float spacing,
                         float gap)
{
    for (int i = first; i <= last; ++i)
    {
        if (!markAt(marks, origin, i).session_start)
        {
            continue;
        }
        placeDate(ticks, i, date_px, spacing, gap);
    }
}

[[nodiscard]] bool anyDate(const std::vector<Placed>& ticks) noexcept
{
    return std::ranges::any_of(ticks, [](const Placed& tick) { return tick.kind == LabelKind::Date; });
}

[[nodiscard]] bool onlySessionOpens(const std::vector<Placed>& ticks,
                                    const std::vector<Mark>& marks,
                                    int origin)
{
    return std::ranges::all_of(ticks, [&](const Placed& tick) {
        return markAt(marks, origin, tick.index).session_start;
    });
}

[[nodiscard]] std::vector<Placed> tryIntraday(std::span<const Bar> bars,
                                                     const std::vector<Mark>& marks,
                                                     int origin,
                                                     int first,
                                                     int last,
                                                     const ChartTickMetrics& metrics)
{
    int sessions = 0;
    for (int i = first; i <= last; ++i)
    {
        if (markAt(marks, origin, i).session_start)
        {
            ++sessions;
        }
    }
    const int bar_count = last - first + 1;
    if (sessions >= bar_count)
    {
        return {};
    }

    const float spacing = metrics.spacing_px;
    const float gap = metrics.gap_px;
    const float time_need = metrics.time_px + gap;
    std::vector<int> previous;
    for (const int step_s : kTimeStepsS)
    {
        const std::vector<int> candidates = ticksOnGrid(bars, marks, origin, first, last, step_s);
        if (candidates.empty() || candidates == previous)
        {
            continue;
        }
        previous = candidates;
        const int bar_gap = minSameSessionGap(candidates, marks, origin);
        if (bar_gap > 0 && (static_cast<float>(bar_gap) * spacing) + 0.01f < time_need)
        {
            continue;
        }
        const std::vector<int> kept = spaceIndices(candidates, marks, origin, spacing, time_need);
        if (kept.empty())
        {
            continue;
        }
        std::vector<Placed> placed;
        placed.reserve(kept.size());
        for (const int index : kept)
        {
            placed.push_back(Placed{.index=index, .kind=LabelKind::Time, .width=metrics.time_px});
        }
        upgradeSessionDates(placed, marks, origin, first, last, metrics.date_px, spacing, gap);
        if (placed.empty() || (onlySessionOpens(placed, marks, origin) && !anyDate(placed)))
        {
            continue;
        }
        if (!anyDate(placed))
        {
            placeDate(placed, first, metrics.date_px, spacing, gap);
        }
        return placed;
    }
    return {};
}

// pred(previous, current) decides a calendar boundary. previous is null only
// for the first bar of the series, not for the first bar of the window.
template <typename Pred>
[[nodiscard]] std::vector<int> collectSessions(std::span<const Bar> bars,
                                               const std::vector<Mark>& marks,
                                               int origin,
                                               int first,
                                               int last,
                                               std::string_view timezone,
                                               Pred pred)
{
    std::vector<int> out;
    Mark prev{};
    const Mark* previous = nullptr;
    if (origin > 0)
    {
        prev.stamp = chartLocalTime(timezone, bars[static_cast<std::size_t>(origin - 1)].ts);
        prev.week_key = mondayKey(prev.stamp.year, prev.stamp.month, prev.stamp.day);
        previous = &prev;
    }
    for (int i = origin; i <= last; ++i)
    {
        const Mark& mark = markAt(marks, origin, i);
        if (!mark.session_start)
        {
            continue;
        }
        if (pred(previous, mark) && i >= first)
        {
            out.push_back(i);
        }
        prev = mark;
        previous = &prev;
    }
    return out;
}

[[nodiscard]] std::vector<int> dayIndices(const std::vector<Mark>& marks, int origin, int first, int last)
{
    std::vector<int> out;
    for (int i = first; i <= last; ++i)
    {
        if (markAt(marks, origin, i).session_start)
        {
            out.push_back(i);
        }
    }
    return out;
}

void addLeading(std::vector<int>& indices, int first, float width, float spacing, float gap)
{
    if (indices.empty() || first >= indices.front())
    {
        return;
    }
    const auto dist = static_cast<float>(indices.front() - first) * spacing;
    if (dist + 0.01f < width + gap)
    {
        return;
    }
    indices.insert(indices.begin(), first);
}

[[nodiscard]] std::vector<Placed> tryCalendar(std::span<const Bar> bars,
                                               const std::vector<Mark>& marks,
                                               int origin,
                                               int first,
                                               int last,
                                               std::string_view timezone,
                                               const ChartTickMetrics& metrics)
{
    const float spacing = metrics.spacing_px;
    const float gap = metrics.gap_px;
    auto accept = [&](LabelKind kind, std::vector<int> indices) {
        std::vector<Placed> placed;
        const float width = kindWidth(kind, metrics);
        if (indices.empty() || !seriesFits(indices, width, spacing, gap))
        {
            return placed;
        }
        addLeading(indices, first, width, spacing, gap);
        placed.reserve(indices.size());
        for (const int index : indices)
        {
            placed.push_back(Placed{.index=index, .kind=kind, .width=width});
        }
        return placed;
    };

    if (std::vector<Placed> placed = accept(LabelKind::Date, dayIndices(marks, origin, first, last));
        !placed.empty())
    {
        return placed;
    }
    if (std::vector<Placed> placed =
            accept(LabelKind::Date, collectSessions(bars, marks, origin, first, last, timezone,
                                                    [](const Mark* prev, const Mark& cur) {
                                                        return prev == nullptr || prev->week_key != cur.week_key;
                                                    }));
        !placed.empty())
    {
        return placed;
    }
    if (std::vector<Placed> placed = accept(
            LabelKind::Month, collectSessions(bars, marks, origin, first, last, timezone,
                                              [](const Mark* prev, const Mark& cur) {
                                                  return prev == nullptr || prev->stamp.year != cur.stamp.year ||
                                                         prev->stamp.month != cur.stamp.month;
                                              }));
        !placed.empty())
    {
        return placed;
    }
    if (std::vector<Placed> placed = accept(
            LabelKind::Month,
            collectSessions(bars, marks, origin, first, last, timezone, [](const Mark* prev, const Mark& cur) {
                const bool quarter_month = ((cur.stamp.month - 1) % 3) == 0;
                if (prev == nullptr)
                {
                    return quarter_month;
                }
                const bool new_month = prev->stamp.year != cur.stamp.year || prev->stamp.month != cur.stamp.month;
                return new_month && quarter_month;
            }));
        !placed.empty())
    {
        return placed;
    }
    for (const int step : kYearSteps)
    {
        if (std::vector<Placed> placed = accept(
                LabelKind::Year,
                collectSessions(bars, marks, origin, first, last, timezone,
                                [step](const Mark* prev, const Mark& cur) {
                                    if (cur.stamp.year % step != 0)
                                    {
                                        return false;
                                    }
                                    return prev == nullptr || prev->stamp.year != cur.stamp.year;
                                }));
            !placed.empty())
        {
            return placed;
        }
    }
    return {};
}

[[nodiscard]] std::vector<ChartAxisTick> emitTicks(const std::vector<Placed>& placed,
                                                   const std::vector<Mark>& marks,
                                                   int origin,
                                                   const ChartVisibleWindow& win,
                                                   float spacing)
{
    std::vector<ChartAxisTick> out;
    out.reserve(placed.size());
    for (const Placed& tick : placed)
    {
        if (!labelClearsPriceScale(tick.index, tick.width, win, spacing))
        {
            continue;
        }
        ChartAxisTick axis_tick;
        axis_tick.x = static_cast<double>(tick.index);
        axis_tick.label = formatLabel(tick.kind, markAt(marks, origin, tick.index).stamp);
        out.push_back(std::move(axis_tick));
    }
    return out;
}

[[nodiscard]] ChartAxisTick fallbackTick(const std::vector<Mark>& marks, int origin, int first, int last)
{
    const int center = first + ((last - first) / 2);
    const ChartLocalTime& stamp = markAt(marks, origin, center).stamp;
    const ChartLocalTime& left = markAt(marks, origin, first).stamp;
    const ChartLocalTime& right = markAt(marks, origin, last).stamp;
    LabelKind kind = LabelKind::Date;
    if (left.year != right.year)
    {
        kind = LabelKind::Year;
    }
    else if (left.month != right.month)
    {
        kind = LabelKind::Month;
    }
    ChartAxisTick tick;
    tick.x = static_cast<double>(center);
    tick.label = formatLabel(kind, stamp);
    return tick;
}

[[nodiscard]] ChartTickMetrics sanitized(ChartTickMetrics metrics) noexcept
{
    metrics.spacing_px = std::max(metrics.spacing_px, 0.25f);
    metrics.gap_px = std::max(metrics.gap_px, 0.0f);
    auto at_least_one = [](float width) {
        return width > 1.0f ? width : 1.0f;
    };
    metrics.time_px = at_least_one(metrics.time_px);
    metrics.date_px = at_least_one(metrics.date_px);
    metrics.month_px = at_least_one(metrics.month_px);
    metrics.year_px = at_least_one(metrics.year_px);
    return metrics;
}

// Smallest stride of `indices` whose labels clear one another. One entry when
// even a pair overlaps. Empty stays empty.
[[nodiscard]] std::vector<int> thinIndices(std::vector<int> indices, float width, float spacing, float gap)
{
    if (indices.size() <= 1 || seriesFits(indices, width, spacing, gap))
    {
        return indices;
    }
    const auto count = static_cast<int>(indices.size());
    for (int step = 2; step < count; ++step)
    {
        std::vector<int> thinned;
        thinned.reserve(static_cast<std::size_t>(((count + step) - 1) / step));
        for (int i = 0; i < count; i += step)
        {
            thinned.push_back(indices[static_cast<std::size_t>(i)]);
        }
        if (seriesFits(thinned, width, spacing, gap))
        {
            return thinned;
        }
    }
    return {indices.front()};
}

}  // namespace

ChartLocalTime chartLocalTime(std::string_view timezone, UnixSeconds ts)
{
    ChartLocalTime out;
    try
    {
        const std::string key(timezone.empty() ? "UTC" : timezone);
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
        if (tryUtcTm(t, utc))
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

std::vector<ChartAxisTick> buildChartTimeTicks(std::span<const Bar> bars,
                                               const ChartVisibleWindow& win,
                                               std::string_view timezone,
                                               const ChartTickMetrics& metrics_in)
{
    if (bars.empty() || win.first > win.last)
    {
        return {};
    }
    const int last = std::min(win.last, static_cast<int>(bars.size()) - 1);
    const int first = std::clamp(win.first, 0, last);
    const ChartTickMetrics metrics = sanitized(metrics_in);
    const std::string_view zone = timezone.empty() ? std::string_view{"UTC"} : timezone;
    const int origin = findSessionOrigin(bars, first, zone);
    const std::vector<Mark> marks = buildMarks(bars, origin, last, zone);

    std::vector<Placed> placed = tryIntraday(bars, marks, origin, first, last, metrics);
    if (placed.empty())
    {
        placed = tryCalendar(bars, marks, origin, first, last, zone, metrics);
    }

    std::vector<ChartAxisTick> ticks = emitTicks(placed, marks, origin, win, metrics.spacing_px);
    if (ticks.empty())
    {
        ticks.push_back(fallbackTick(marks, origin, first, last));
    }
    return ticks;
}

std::vector<ChartAxisTick> buildChartVerticalGridTicks(std::span<const Bar> bars,
                                                       const ChartVisibleWindow& win,
                                                       std::string_view timezone,
                                                       const ChartTickMetrics& metrics_in,
                                                       ChartVerticalGrid grid)
{
    if (bars.empty() || win.first > win.last)
    {
        return {};
    }
    const int last = std::min(win.last, static_cast<int>(bars.size()) - 1);
    const int first = std::clamp(win.first, 0, last);
    const ChartTickMetrics metrics = sanitized(metrics_in);
    const std::string_view zone = timezone.empty() ? std::string_view{"UTC"} : timezone;
    const int origin = findSessionOrigin(bars, first, zone);
    const std::vector<Mark> marks = buildMarks(bars, origin, last, zone);

    LabelKind kind = LabelKind::Date;
    std::vector<int> indices;
    switch (grid)
    {
    case ChartVerticalGrid::Weekly:
        indices = collectSessions(bars, marks, origin, first, last, zone,
                                  [](const Mark* prev, const Mark& cur) {
                                      return prev == nullptr || prev->week_key != cur.week_key;
                                  });
        break;
    case ChartVerticalGrid::Monthly:
        kind = LabelKind::Month;
        indices = collectSessions(bars, marks, origin, first, last, zone,
                                  [](const Mark* prev, const Mark& cur) {
                                      return prev == nullptr || prev->stamp.year != cur.stamp.year ||
                                             prev->stamp.month != cur.stamp.month;
                                  });
        break;
    case ChartVerticalGrid::Daily:
    default:
        indices = dayIndices(marks, origin, first, last);
        // The open of the session under `first` is left of the window, so it is
        // not in `indices`, but that session is still on screen. Dropping it
        // makes an afternoon plus the next morning look like one session.
        {
            std::size_t open_sessions = indices.size();
            if (origin < first)
            {
                ++open_sessions;
            }
            if (open_sessions < 2)
            {
                return buildChartTimeTicks(bars, win, timezone, metrics);
            }
        }
        break;
    }

    const float width = kindWidth(kind, metrics);
    indices = thinIndices(std::move(indices), width, metrics.spacing_px, metrics.gap_px);
    addLeading(indices, first, width, metrics.spacing_px, metrics.gap_px);

    std::vector<Placed> placed;
    placed.reserve(indices.size());
    for (const int index : indices)
    {
        placed.push_back(Placed{.index=index, .kind=kind, .width=width});
    }
    std::vector<ChartAxisTick> ticks = emitTicks(placed, marks, origin, win, metrics.spacing_px);
    if (ticks.empty())
    {
        const int center = first + ((last - first) / 2);
        ChartAxisTick tick;
        tick.x = static_cast<double>(center);
        tick.label = formatLabel(kind, markAt(marks, origin, center).stamp);
        ticks.push_back(std::move(tick));
    }
    return ticks;
}

std::vector<double> buildHorizontalGridTicks(double ymin, double ymax, double spacing)
{
    if (!std::isfinite(ymin) || !std::isfinite(ymax) || !std::isfinite(spacing) || !(spacing > 0.0) ||
        !(ymax > ymin))
    {
        return {};
    }

    const double first_index = std::ceil((ymin / spacing) - kGridRelativeEps);
    const double last_index = std::floor((ymax / spacing) + kGridRelativeEps);
    if (!std::isfinite(first_index) || !std::isfinite(last_index) || !(last_index >= first_index))
    {
        return {};
    }

    const double first = first_index * spacing;
    const double last = last_index * spacing;
    if (!std::isfinite(first) || !std::isfinite(last) || !(last >= first))
    {
        return {};
    }

    // Span/spacing counts gaps, so [100, 148] at 1 is 49 levels and a 48-long
    // walk drops 148. Widen from the inclusive count, then keep every widened
    // level that is still <= last, including that far endpoint.
    const double count = std::round((last - first) / spacing) + 1.0;
    if (!(count >= 1.0) || !std::isfinite(count))
    {
        return {};
    }

    double step = spacing;
    if (count > static_cast<double>(kChartMaxHorizontalGridLines))
    {
        const double factor = std::ceil(count / static_cast<double>(kChartMaxHorizontalGridLines));
        step = spacing * factor;
    }
    if (!(step > 0.0) || !std::isfinite(step))
    {
        return {};
    }

    std::vector<double> ticks;
    const double reserve_count = std::min(count, static_cast<double>(kChartMaxHorizontalGridLines));
    ticks.reserve(static_cast<std::size_t>(reserve_count));

    // The stop is `y <= last`, not 48. The index limit only keeps a bad step
    // from walking forever; it sits past the last in-range lattice point.
    const double gaps = (last - first) / step;
    int n_limit = 1;
    if (std::isfinite(gaps) && gaps > 0.0 && gaps < 1.0e6)
    {
        n_limit = static_cast<int>(std::ceil(gaps)) + 2;
    }
    const double scale = std::max({std::abs(first), std::abs(last), step, 1.0});
    const double tol = scale * kGridRelativeEps;
    for (int n = 0; n < n_limit; ++n)
    {
        const double y = first + (static_cast<double>(n) * step);
        if (!std::isfinite(y) || y > last + tol)
        {
            break;
        }
        if (y >= ymin - tol && y <= ymax + tol)
        {
            ticks.push_back(y);
        }
    }
    return ticks;
}

int horizontalGridDecimals(double step)
{
    if (!std::isfinite(step))
    {
        return 0;
    }
    double scale = std::abs(step);
    if (!(scale > 0.0))
    {
        return 0;
    }
    for (int places = 0; places < 8; ++places)
    {
        const double nearest = std::round(scale);
        const double span = std::max(std::abs(scale), std::abs(nearest));
        if (std::abs(scale - nearest) <= span * kGridRelativeEps)
        {
            return places;
        }
        const double next = scale * 10.0;
        if (!(next > scale) || !std::isfinite(next))
        {
            break;
        }
        scale = next;
    }
    return 8;
}

}  // namespace terminal
