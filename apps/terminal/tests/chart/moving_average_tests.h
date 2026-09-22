// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/studies/CMovingAverage.h"

#include <cmath>
#include <initializer_list>
#include <vector>

namespace {

terminal::Bar movingAverageBar(double open, double high, double low, double close, double volume)
{
    terminal::Bar bar;
    bar.open = open;
    bar.high = high;
    bar.low = low;
    bar.close = close;
    bar.volume = volume;
    return bar;
}

std::vector<terminal::Bar> movingAverageCloses(std::initializer_list<double> closes)
{
    std::vector<terminal::Bar> bars;
    bars.reserve(closes.size());
    for (const double close : closes)
    {
        bars.push_back(movingAverageBar(close, close, close, close, close));
    }
    return bars;
}

}  // namespace

TEST_CASE("CMovingAverage defaults to a 20 bar close")
{
    const terminal::CMovingAverage average;
    CHECK(average.options().length == terminal::CMovingAverage::kDefaultLength);
    CHECK(average.options().source == terminal::CMovingAverage::Source::Close);
    CHECK(average.label() == "MA 20 C");
}

TEST_CASE("CMovingAverage length clamps into range")
{
    terminal::CMovingAverage average{{.length = 0, .source = terminal::CMovingAverage::Source::High}};
    CHECK(average.options().length == terminal::CMovingAverage::kMinLength);
    CHECK(average.options().source == terminal::CMovingAverage::Source::High);
    CHECK(average.label() == "MA 1 H");

    average.setOptions({.length = 99999, .source = terminal::CMovingAverage::Source::Low});
    CHECK(average.options().length == terminal::CMovingAverage::kMaxLength);
    CHECK(average.options().source == terminal::CMovingAverage::Source::Low);
    CHECK(average.label() == "MA 10000 L");
}

TEST_CASE("CMovingAverage of an empty series is empty")
{
    const terminal::CMovingAverage average{{.length = 5}};
    CHECK(average.process({}).empty());
    CHECK(average.label() == "MA 5 C");
}

TEST_CASE("CMovingAverage copies one bar field when length is 1")
{
    const std::vector<terminal::Bar> bars{
        movingAverageBar(1.0, 8.0, 0.5, 3.0, 10.0),
        movingAverageBar(5.0, 9.0, 0.25, 7.0, 12.0),
    };
    const auto run = [&](terminal::CMovingAverage::Source source) {
        const terminal::CMovingAverage average{{.length = 1, .source = source}};
        return average.process(bars);
    };

    const std::vector<double> open = run(terminal::CMovingAverage::Source::Open);
    const std::vector<double> high = run(terminal::CMovingAverage::Source::High);
    const std::vector<double> low = run(terminal::CMovingAverage::Source::Low);
    const std::vector<double> close = run(terminal::CMovingAverage::Source::Close);
    const std::vector<double> volume = run(terminal::CMovingAverage::Source::Volume);
    REQUIRE(close.size() == 2);
    CHECK(open[0] == Catch::Approx(1.0));
    CHECK(open[1] == Catch::Approx(5.0));
    CHECK(high[0] == Catch::Approx(8.0));
    CHECK(high[1] == Catch::Approx(9.0));
    CHECK(low[0] == Catch::Approx(0.5));
    CHECK(low[1] == Catch::Approx(0.25));
    CHECK(close[0] == Catch::Approx(3.0));
    CHECK(close[1] == Catch::Approx(7.0));
    CHECK(volume[0] == Catch::Approx(10.0));
    CHECK(volume[1] == Catch::Approx(12.0));
}

TEST_CASE("CMovingAverage warms up with NaN")
{
    const auto bars = movingAverageCloses({1.0, 2.0, 3.0, 4.0, 5.0});
    const terminal::CMovingAverage average{{.length = 3}};
    const std::vector<double> values = average.process(bars);
    REQUIRE(values.size() == 5);
    CHECK_FALSE(std::isfinite(values[0]));
    CHECK_FALSE(std::isfinite(values[1]));
    CHECK(values[2] == Catch::Approx(2.0));
    CHECK(values[3] == Catch::Approx(3.0));
    CHECK(values[4] == Catch::Approx(4.0));
    CHECK(average.label() == "MA 3 C");
}

TEST_CASE("CMovingAverage longer than the series is all NaN")
{
    const auto bars = movingAverageCloses({1.0, 2.0, 3.0});
    const terminal::CMovingAverage average{{.length = 4}};
    const std::vector<double> values = average.process(bars);
    REQUIRE(values.size() == bars.size());
    for (const double value : values)
    {
        CHECK_FALSE(std::isfinite(value));
    }
    CHECK(average.label() == "MA 4 C");
}
