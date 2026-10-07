// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartView.h"
#include "market_data/Types.h"

#include <cmath>
#include <limits>
#include <string>
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

TEST_CASE("automatic y limits expand to the overlay before padding")
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
    settings.scale_padding_pct = 10.0f;
    terminal::CChartViewState view;
    terminal::OverlayYExtent overlay;
    overlay.valid = true;
    overlay.min = 0.0;
    overlay.max = 30.0;
    const auto y = terminal::computeYLimits(bars, win, settings, view, overlay);
    // Merged range is 30. Padding is 10% of that range, not of the candle range.
    CHECK(y.min == Catch::Approx(0.0 - 3.0));
    CHECK(y.max == Catch::Approx(30.0 + 3.0));
}

TEST_CASE("constant range ignores overlay extent")
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
    terminal::OverlayYExtent overlay;
    overlay.valid = true;
    overlay.min = -1.0e9;
    overlay.max = 1.0e9;
    const auto y = terminal::computeYLimits(bars, win, settings, view, overlay);
    CHECK(y.min == Catch::Approx(95.0));
    CHECK(y.max == Catch::Approx(115.0));
}

TEST_CASE("constant range fallback keeps the candle range when overlay is set")
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
    settings.scale_range = terminal::ChartScaleRange::ConstantRange;
    settings.constant_range = 0.0;
    settings.scale_padding_pct = 4.0f;
    terminal::CChartViewState view;
    terminal::OverlayYExtent overlay;
    overlay.valid = true;
    overlay.min = 0.0;
    overlay.max = 30.0;
    const auto y = terminal::computeYLimits(bars, win, settings, view, overlay);
    // Candle range 10, pad 0.4, fallback range 10.8, centered on 15.
    CHECK(y.min == Catch::Approx(9.6));
    CHECK(y.max == Catch::Approx(20.4));
}

TEST_CASE("user defined scale ignores overlay when top and bottom are set")
{
    std::vector<terminal::Bar> bars(1);
    bars[0].low = 10.0;
    bars[0].high = 20.0;
    terminal::ChartVisibleWindow win;
    win.first = 0;
    win.last = 0;
    terminal::CChartSettings settings;
    settings.scale_range = terminal::ChartScaleRange::UserDefined;
    settings.user_bottom = 40.0;
    settings.user_top = 80.0;
    terminal::CChartViewState view;
    terminal::OverlayYExtent overlay;
    overlay.valid = true;
    overlay.min = -1000.0;
    overlay.max = 5000.0;
    const auto y = terminal::computeYLimits(bars, win, settings, view, overlay);
    CHECK(y.min == Catch::Approx(40.0));
    CHECK(y.max == Catch::Approx(80.0));
}

TEST_CASE("user defined fallback uses candle high low and ignores overlay")
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
    settings.scale_range = terminal::ChartScaleRange::UserDefined;
    settings.user_top = 1.0;
    settings.user_bottom = 2.0;
    settings.scale_padding_pct = 4.0f;
    terminal::CChartViewState view;
    terminal::OverlayYExtent overlay;
    overlay.valid = true;
    overlay.min = 0.0;
    overlay.max = 100.0;
    const auto y = terminal::computeYLimits(bars, win, settings, view, overlay);
    CHECK(y.min == Catch::Approx(10.0 - 0.4));
    CHECK(y.max == Catch::Approx(20.0 + 0.4));
}

TEST_CASE("a spoiled bar spacing still gives a usable window")
{
    // std::clamp passes NaN through, and computeVisibleWindow would then cast a
    // NaN slot count to int.
    const auto nan = terminal::computeVisibleWindow(1000, 800.0f, std::numeric_limits<float>::quiet_NaN(), 0,
                                                    terminal::kChartRightFillBars);
    CHECK(nan.slot_count == static_cast<int>(800.0f / terminal::kChartDefaultBarSpacingPx));
    CHECK(nan.first <= nan.last);
    CHECK(nan.first >= 0);

    const auto inf = terminal::computeVisibleWindow(1000, 800.0f, std::numeric_limits<float>::infinity(), 0,
                                                    terminal::kChartRightFillBars);
    CHECK(inf.slot_count == 6);
    CHECK(inf.scroll == 0);
}

TEST_CASE("a non-finite scale range falls back to the bars")
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
    terminal::CChartViewState view;

    terminal::CChartSettings settings;
    settings.scale_range = terminal::ChartScaleRange::UserDefined;
    settings.user_bottom = 0.0;
    settings.user_top = std::numeric_limits<double>::infinity();
    const auto user = terminal::computeYLimits(bars, win, settings, view);
    CHECK(user.min == Catch::Approx(10.0 - 0.4));
    CHECK(user.max == Catch::Approx(20.0 + 0.4));

    settings.scale_range = terminal::ChartScaleRange::ConstantRange;
    settings.constant_range = std::numeric_limits<double>::infinity();
    const auto constant = terminal::computeYLimits(bars, win, settings, view);
    CHECK(std::isfinite(constant.min));
    CHECK(std::isfinite(constant.max));
    CHECK(constant.min < constant.max);
}

TEST_CASE("clamped limits leave no NaN in the pixel fields")
{
    terminal::CChartSettings settings;
    settings.bar_spacing_px = std::numeric_limits<float>::quiet_NaN();
    settings.bar_width_frac = std::numeric_limits<float>::quiet_NaN();
    settings.scale_padding_pct = std::numeric_limits<float>::quiet_NaN();
    terminal::clampV1Limits(settings);
    CHECK(settings.bar_spacing_px == terminal::kChartMinBarSpacingPx);
    CHECK(settings.bar_width_frac == 0.10f);
    CHECK(settings.scale_padding_pct == 0.0f);

    // A NaN spacing would divide the plot width into a NaN slot count, and
    // truncating that is undefined.
    const terminal::ChartVisibleWindow win = terminal::computeVisibleWindow(100, 800.0f, settings.bar_spacing_px, 0,
                                                                            terminal::kChartRightFillBars);
    CHECK(win.slot_count >= 1);
}

TEST_CASE("chart strip scale label")
{
    CHECK(std::string(terminal::chartScaleStripLabel(terminal::ChartScaleRange::Automatic)) == "auto");
    CHECK(std::string(terminal::chartScaleStripLabel(terminal::ChartScaleRange::ConstantRange)) == "range");
    CHECK(std::string(terminal::chartScaleStripLabel(terminal::ChartScaleRange::UserDefined)) == "user");
}
