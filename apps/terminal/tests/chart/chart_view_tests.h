#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartView.h"
#include "market_data/Types.h"

#include <vector>

TEST_CASE("visible window right-aligns the last bar")
{
    const auto w = terminal::computeVisibleWindow(100, 80.0f, 8.0f, 0, 2);
    CHECK(w.slot_count == 10);
    CHECK(w.scroll == 0);
    CHECK(w.last == 99);
    CHECK(w.first == 92);
}

TEST_CASE("increasing bar spacing drops left bars and keeps the last")
{
    const auto tight = terminal::computeVisibleWindow(100, 80.0f, 8.0f, 0, 2);
    const auto wide = terminal::computeVisibleWindow(100, 80.0f, 16.0f, 0, 2);
    CHECK(wide.last == tight.last);
    CHECK(wide.first > tight.first);
    CHECK(wide.slot_count == 5);
}

TEST_CASE("scroll_from_end reveals older bars")
{
    const auto w = terminal::computeVisibleWindow(100, 80.0f, 8.0f, 20, 2);
    CHECK(w.last == 81);
    CHECK(w.scroll == 20);
}

TEST_CASE("scroll clamps when all bars fit")
{
    const auto w = terminal::computeVisibleWindow(10, 800.0f, 8.0f, 999, 2);
    CHECK(w.scroll == 0);
    CHECK(w.first == 0);
    CHECK(w.last == 9);
}

TEST_CASE("automatic y limits pad the visible high/low")
{
    std::vector<terminal::Bar> bars(3);
    for (terminal::Bar& bar : bars)
    {
        bar.low = 10.0;
        bar.high = 20.0;
    }
    terminal::ChartVisibleWindow win;
    win.first = 0;
    win.last = 2;
    terminal::CChartSettings settings;
    terminal::CChartViewState view;
    const auto y = terminal::computeYLimits(bars, win, settings, view);
    CHECK(y.min == Catch::Approx(10.0 - 0.4));
    CHECK(y.max == Catch::Approx(20.0 + 0.4));
}

TEST_CASE("constant range centers on the last visible bar")
{
    std::vector<terminal::Bar> bars(2);
    bars[0].low = 0.0;
    bars[0].high = 1.0;
    bars[1].low = 100.0;
    bars[1].high = 110.0;
    terminal::ChartVisibleWindow win;
    win.first = 0;
    win.last = 1;
    terminal::CChartSettings settings;
    settings.scale_range = terminal::ChartScaleRange::ConstantRange;
    settings.constant_range = 20.0;
    terminal::CChartViewState view;
    const auto y = terminal::computeYLimits(bars, win, settings, view);
    CHECK(y.min == Catch::Approx(95.0));
    CHECK(y.max == Catch::Approx(115.0));
}

TEST_CASE("resetChartScale clears interactive extras")
{
    terminal::CChartViewState view;
    view.extra_pad_frac = 1.5;
    view.move_offset = 12.0;
    view.working_range = 9.0;
    terminal::resetChartScale(view);
    CHECK(view.extra_pad_frac == 0.0);
    CHECK(view.move_offset == 0.0);
    CHECK(view.working_range == 0.0);
}
