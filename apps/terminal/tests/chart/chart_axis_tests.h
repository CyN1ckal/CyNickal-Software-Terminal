// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartAxis.h"
#include "market_data/Types.h"

#include <chrono>
#include <string>
#include <vector>

namespace {

terminal::ChartTickMetrics axisMetrics(float spacing)
{
    terminal::ChartTickMetrics metrics;
    metrics.spacing_px = spacing;
    metrics.gap_px = 8.0f;
    metrics.time_px = 40.0f;
    metrics.date_px = 72.0f;
    metrics.month_px = 56.0f;
    metrics.year_px = 32.0f;
    return metrics;
}

terminal::ChartVisibleWindow axisWindow(int first, int last, int right_fill = 2)
{
    terminal::ChartVisibleWindow win;
    win.first = first;
    win.last = last;
    win.x_min = static_cast<double>(first) - 0.5;
    win.x_max = static_cast<double>(last) + static_cast<double>(right_fill) + 0.5;
    return win;
}

float axisLabelWidth(const std::string& label, const terminal::ChartTickMetrics& metrics)
{
    if (label.size() >= 10)
    {
        return metrics.date_px;
    }
    if (label.size() == 7)
    {
        return metrics.month_px;
    }
    if (label.size() == 5)
    {
        return metrics.time_px;
    }
    return metrics.year_px;
}

void axisExpectReadable(const std::vector<terminal::ChartAxisTick>& ticks,
                        const terminal::ChartTickMetrics& metrics)
{
    REQUIRE_FALSE(ticks.empty());
    for (std::size_t i = 1; i < ticks.size(); ++i)
    {
        CHECK(ticks[i].x > ticks[i - 1].x);
        const int delta = static_cast<int>(ticks[i].x - ticks[i - 1].x);
        const float dist = static_cast<float>(delta) * metrics.spacing_px;
        const float need = 0.5f * (axisLabelWidth(ticks[i - 1].label, metrics) +
                                    axisLabelWidth(ticks[i].label, metrics)) +
                           metrics.gap_px;
        CHECK(dist + 0.01f >= need);
    }
}

std::vector<terminal::Bar> axisDailyBars(int count)
{
    std::vector<terminal::Bar> bars;
    bars.reserve(static_cast<std::size_t>(count));
    auto day = std::chrono::sys_days{std::chrono::year{2024} / std::chrono::January / 2};
    while (static_cast<int>(bars.size()) < count)
    {
        const std::chrono::weekday weekday{day};
        if (weekday != std::chrono::Saturday && weekday != std::chrono::Sunday)
        {
            terminal::Bar bar;
            bar.timeframe_s = terminal::kTimeframe1d;
            bar.ts = std::chrono::duration_cast<std::chrono::seconds>(
                         (day + std::chrono::hours{9} + std::chrono::minutes{30}).time_since_epoch())
                         .count();
            bars.push_back(bar);
        }
        day += std::chrono::days{1};
    }
    return bars;
}

std::vector<terminal::Bar> axisMinuteBars(int sessions, int per_session)
{
    std::vector<terminal::Bar> bars;
    auto day = std::chrono::sys_days{std::chrono::year{2024} / std::chrono::January / 2};
    int made = 0;
    while (made < sessions)
    {
        const std::chrono::weekday weekday{day};
        if (weekday != std::chrono::Saturday && weekday != std::chrono::Sunday)
        {
            for (int i = 0; i < per_session; ++i)
            {
                terminal::Bar bar;
                bar.timeframe_s = terminal::kTimeframe1m;
                bar.ts = std::chrono::duration_cast<std::chrono::seconds>(
                             (day + std::chrono::hours{9} + std::chrono::minutes{30 + i}).time_since_epoch())
                             .count();
                bars.push_back(bar);
            }
            ++made;
        }
        day += std::chrono::days{1};
    }
    return bars;
}

std::vector<terminal::Bar> axisHourBars(int sessions)
{
    std::vector<terminal::Bar> bars;
    auto day = std::chrono::sys_days{std::chrono::year{2024} / std::chrono::January / 2};
    int made = 0;
    while (made < sessions)
    {
        const std::chrono::weekday weekday{day};
        if (weekday != std::chrono::Saturday && weekday != std::chrono::Sunday)
        {
            for (int i = 0; i < 7; ++i)
            {
                terminal::Bar bar;
                bar.timeframe_s = 3600;
                bar.ts = std::chrono::duration_cast<std::chrono::seconds>(
                             (day + std::chrono::hours{9} + std::chrono::minutes{30 + 60 * i})
                                 .time_since_epoch())
                             .count();
                bars.push_back(bar);
            }
            ++made;
        }
        day += std::chrono::days{1};
    }
    return bars;
}

int axisIndexOn(const std::vector<terminal::Bar>& bars, int year, int month, int day)
{
    for (std::size_t i = 0; i < bars.size(); ++i)
    {
        const terminal::ChartLocalTime stamp = terminal::chartLocalTime("UTC", bars[i].ts);
        if (stamp.year == year && stamp.month == month && stamp.day == day)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

TEST_CASE("daily labels stay month-aligned when every session would overlap")
{
    const std::vector<terminal::Bar> bars = axisDailyBars(120);
    const terminal::ChartTickMetrics metrics = axisMetrics(8.0f);
    const std::vector<terminal::ChartAxisTick> ticks =
        terminal::buildChartTimeTicks(bars, axisWindow(0, static_cast<int>(bars.size()) - 1), "UTC", metrics);
    axisExpectReadable(ticks, metrics);
    REQUIRE(ticks.size() == 6);
    CHECK(ticks[0].label == "2024-01");
    CHECK(ticks[1].label == "2024-02");
    CHECK(ticks[2].label == "2024-03");
    CHECK(ticks[3].label == "2024-04");
    CHECK(ticks[4].label == "2024-05");
    CHECK(ticks[5].label == "2024-06");
}

TEST_CASE("zoomed daily bars label every session")
{
    const std::vector<terminal::Bar> bars = axisDailyBars(15);
    const terminal::ChartTickMetrics metrics = axisMetrics(100.0f);
    const std::vector<terminal::ChartAxisTick> ticks =
        terminal::buildChartTimeTicks(bars, axisWindow(0, static_cast<int>(bars.size()) - 1), "UTC", metrics);
    axisExpectReadable(ticks, metrics);
    REQUIRE(ticks.size() == bars.size());
    CHECK(ticks.front().label == "2024-01-02");
    CHECK(ticks.back().label == "2024-01-22");
}

TEST_CASE("years of daily bars collapse to year labels")
{
    const std::vector<terminal::Bar> bars = axisDailyBars(1000);
    const terminal::ChartTickMetrics metrics = axisMetrics(0.5f);
    const std::vector<terminal::ChartAxisTick> ticks =
        terminal::buildChartTimeTicks(bars, axisWindow(0, static_cast<int>(bars.size()) - 1), "UTC", metrics);
    axisExpectReadable(ticks, metrics);
    REQUIRE(ticks.size() >= 4);
    for (const terminal::ChartAxisTick& tick : ticks)
    {
        CHECK(tick.label.size() == 4);
    }
    CHECK(ticks.front().label == "2024");
}

TEST_CASE("a window that opens mid-month still labels that month when it fits")
{
    const std::vector<terminal::Bar> bars = axisDailyBars(120);
    const int feb12 = axisIndexOn(bars, 2024, 2, 12);
    REQUIRE(feb12 > 0);
    const terminal::ChartTickMetrics metrics = axisMetrics(8.0f);
    const std::vector<terminal::ChartAxisTick> ticks = terminal::buildChartTimeTicks(
        bars, axisWindow(feb12, static_cast<int>(bars.size()) - 1), "UTC", metrics);
    axisExpectReadable(ticks, metrics);
    REQUIRE_FALSE(ticks.empty());
    CHECK(ticks.front().label == "2024-02");
    CHECK(ticks.front().x == static_cast<double>(feb12));
    bool saw_march = false;
    for (const terminal::ChartAxisTick& tick : ticks)
    {
        if (tick.label == "2024-03")
        {
            saw_march = true;
        }
    }
    CHECK(saw_march);
}

TEST_CASE("minute bars label round times and the session date")
{
    const std::vector<terminal::Bar> bars = axisMinuteBars(3, 60);
    const terminal::ChartTickMetrics metrics = axisMetrics(8.0f);
    const auto win = axisWindow(0, static_cast<int>(bars.size()) - 1);
    const std::vector<terminal::ChartAxisTick> ticks =
        terminal::buildChartTimeTicks(bars, win, "UTC", metrics);
    axisExpectReadable(ticks, metrics);
    REQUIRE_FALSE(ticks.empty());
    CHECK(ticks.front().x == 0.0);
    CHECK(ticks.front().label == "2024-01-02");

    const auto at = [&](int index) -> const terminal::ChartAxisTick* {
        for (const terminal::ChartAxisTick& tick : ticks)
        {
            if (tick.x == static_cast<double>(index))
            {
                return &tick;
            }
        }
        return nullptr;
    };
    const terminal::ChartAxisTick* ten = at(10);
    REQUIRE(ten != nullptr);
    CHECK(ten->label == "09:40");
    CHECK(at(1) == nullptr);
    CHECK(at(5) == nullptr);

    const terminal::ChartAxisTick* next_open = at(60);
    REQUIRE(next_open != nullptr);
    CHECK(next_open->label == "2024-01-03");

    const auto shifted = axisWindow(3, static_cast<int>(bars.size()) - 1);
    const std::vector<terminal::ChartAxisTick> panned =
        terminal::buildChartTimeTicks(bars, shifted, "UTC", metrics);
    const terminal::ChartAxisTick* panned_ten = nullptr;
    for (const terminal::ChartAxisTick& tick : panned)
    {
        if (tick.x == 10.0)
        {
            panned_ten = &tick;
        }
    }
    REQUIRE(panned_ten != nullptr);
    CHECK(panned_ten->label == "09:40");
}

TEST_CASE("zoomed-in minute bars can label every bar")
{
    const std::vector<terminal::Bar> bars = axisMinuteBars(1, 30);
    const terminal::ChartTickMetrics metrics = axisMetrics(128.0f);
    const std::vector<terminal::ChartAxisTick> ticks =
        terminal::buildChartTimeTicks(bars, axisWindow(0, static_cast<int>(bars.size()) - 1), "UTC", metrics);
    axisExpectReadable(ticks, metrics);
    REQUIRE(ticks.size() == bars.size());
    CHECK(ticks.front().label == "2024-01-02");
    CHECK(ticks[1].label == "09:31");
}

TEST_CASE("hourly bars at the default spacing do not label every bar")
{
    const std::vector<terminal::Bar> bars = axisHourBars(30);
    const terminal::ChartTickMetrics metrics = axisMetrics(8.0f);
    const std::vector<terminal::ChartAxisTick> ticks =
        terminal::buildChartTimeTicks(bars, axisWindow(0, static_cast<int>(bars.size()) - 1), "UTC", metrics);
    axisExpectReadable(ticks, metrics);
    CHECK(ticks.size() < bars.size() / 4);
    CHECK(ticks.front().label == "2024-01-02");
}

TEST_CASE("time labels stay apart at every bar spacing")
{
    const std::vector<terminal::Bar> daily = axisDailyBars(180);
    const std::vector<terminal::Bar> minutes = axisMinuteBars(4, 90);
    const float spacings[] = {1.0f, 1.5f, 2.0f, 3.0f, 5.0f, 8.0f, 12.0f, 20.0f,
                              32.0f, 48.0f, 64.0f, 96.0f, 128.0f};
    for (const float spacing : spacings)
    {
        const terminal::ChartTickMetrics metrics = axisMetrics(spacing);
        const std::vector<terminal::ChartAxisTick> daily_ticks = terminal::buildChartTimeTicks(
            daily, axisWindow(0, static_cast<int>(daily.size()) - 1), "UTC", metrics);
        axisExpectReadable(daily_ticks, metrics);
        const std::vector<terminal::ChartAxisTick> minute_ticks = terminal::buildChartTimeTicks(
            minutes, axisWindow(0, static_cast<int>(minutes.size()) - 1), "UTC", metrics);
        axisExpectReadable(minute_ticks, metrics);
    }
}

TEST_CASE("a label that would cover the price scale is omitted")
{
    const std::vector<terminal::Bar> bars = axisDailyBars(8);
    const terminal::ChartTickMetrics metrics = axisMetrics(100.0f);
    terminal::ChartVisibleWindow win = axisWindow(0, static_cast<int>(bars.size()) - 1, 0);
    win.x_max = static_cast<double>(bars.size() - 1) + 0.2;
    const std::vector<terminal::ChartAxisTick> ticks = terminal::buildChartTimeTicks(bars, win, "UTC", metrics);
    axisExpectReadable(ticks, metrics);
    REQUIRE(ticks.size() + 1 == bars.size());
    CHECK(ticks.back().x == static_cast<double>(bars.size() - 2));
}

TEST_CASE("empty and single-bar windows")
{
    const terminal::ChartTickMetrics metrics = axisMetrics(8.0f);
    CHECK(terminal::buildChartTimeTicks({}, axisWindow(0, 0), "UTC", metrics).empty());
    const std::vector<terminal::Bar> bars = axisDailyBars(1);
    terminal::ChartVisibleWindow inverted = axisWindow(0, 0);
    inverted.first = 3;
    inverted.last = 1;
    CHECK(terminal::buildChartTimeTicks(bars, inverted, "UTC", metrics).empty());
    const std::vector<terminal::ChartAxisTick> one =
        terminal::buildChartTimeTicks(bars, axisWindow(0, 0), "UTC", metrics);
    REQUIRE(one.size() == 1);
    CHECK(one.front().label == "2024-01-02");
}
