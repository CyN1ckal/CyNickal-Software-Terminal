// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CStudyCompute.h"
#include "chart/studies/CMovingAverage.h"
#include "chart/studies/StudyRegistry.h"

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <string_view>
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
    CHECK(average.options().method == terminal::CMovingAverage::Method::Simple);
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

TEST_CASE("CMovingAverage confines a NaN bar to the windows that hold it")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto bars = movingAverageCloses({1.0, 2.0, nan, 4.0, 5.0});
    const terminal::CMovingAverage average{{.length = 2}};
    const std::vector<double> values = average.process(bars);
    REQUIRE(values.size() == 5);
    CHECK_FALSE(std::isfinite(values[0]));
    CHECK(values[1] == Catch::Approx(1.5));
    CHECK_FALSE(std::isfinite(values[2]));
    CHECK_FALSE(std::isfinite(values[3]));
    CHECK(values[4] == Catch::Approx(4.5));
}

TEST_CASE("CMovingAverage recovers after an infinity bar leaves the window")
{
    const auto bars = movingAverageCloses({1.0, 2.0, 3.0, HUGE_VAL, 5.0, 6.0});
    const terminal::CMovingAverage average{{.length = 2}};
    const std::vector<double> values = average.process(bars);
    REQUIRE(values.size() == 6);
    CHECK(std::isinf(values[3]));
    CHECK(std::isinf(values[4]));  // the window still holds the inf bar
    CHECK(values[5] == Catch::Approx(5.5));
}

TEST_CASE("CMovingAverage exponential seeds with the simple average")
{
    // Closes 2, 4, 6, 10, 2, 8. Length 3. k = 2 / 4 = 1/2.
    // Seed (2+4+6)/3 = 4, then 7, 4.5, 6.25.
    const auto bars = movingAverageCloses({2.0, 4.0, 6.0, 10.0, 2.0, 8.0});
    const terminal::CMovingAverage average{
        {.length = 3,
         .source = terminal::CMovingAverage::Source::Close,
         .method = terminal::CMovingAverage::Method::Exponential}};
    const std::vector<double> values = average.process(bars);
    REQUIRE(values.size() == 6);
    CHECK_FALSE(std::isfinite(values[0]));
    CHECK_FALSE(std::isfinite(values[1]));
    CHECK(values[2] == Catch::Approx(4.0));
    CHECK(values[3] == Catch::Approx(7.0));
    CHECK(values[4] == Catch::Approx(4.5));
    CHECK(values[5] == Catch::Approx(6.25));
    CHECK(average.label() == "MA 3 C");
}

TEST_CASE("CMovingAverage weighted uses weights one through length")
{
    // Closes 2, 4, 6, 10, 2, 8. Length 3. Divisor 6.
    // (1*2+2*4+3*6)/6 = 14/3, then 23/3, 16/3, 19/3.
    const auto bars = movingAverageCloses({2.0, 4.0, 6.0, 10.0, 2.0, 8.0});
    const terminal::CMovingAverage average{
        {.length = 3,
         .source = terminal::CMovingAverage::Source::Close,
         .method = terminal::CMovingAverage::Method::Weighted}};
    const std::vector<double> values = average.process(bars);
    REQUIRE(values.size() == 6);
    CHECK_FALSE(std::isfinite(values[0]));
    CHECK_FALSE(std::isfinite(values[1]));
    CHECK(values[2] == Catch::Approx(14.0 / 3.0));
    CHECK(values[3] == Catch::Approx(23.0 / 3.0));
    CHECK(values[4] == Catch::Approx(16.0 / 3.0));
    CHECK(values[5] == Catch::Approx(19.0 / 3.0));
    CHECK(average.label() == "MA 3 C");
}

TEST_CASE("CMovingAverage exponential and weighted read the selected field")
{
    const std::vector<terminal::Bar> bars{
        movingAverageBar(9.0, 2.0, 1.0, 100.0, 2.0),
        movingAverageBar(9.0, 4.0, 1.0, 100.0, 4.0),
        movingAverageBar(9.0, 6.0, 1.0, 100.0, 6.0),
        movingAverageBar(9.0, 10.0, 1.0, 100.0, 10.0),
    };
    const terminal::CMovingAverage exponential{
        {.length = 3,
         .source = terminal::CMovingAverage::Source::Volume,
         .method = terminal::CMovingAverage::Method::Exponential}};
    const std::vector<double> ema = exponential.process(bars);
    REQUIRE(ema.size() == 4);
    CHECK_FALSE(std::isfinite(ema[0]));
    CHECK_FALSE(std::isfinite(ema[1]));
    CHECK(ema[2] == Catch::Approx(4.0));
    CHECK(ema[3] == Catch::Approx(7.0));
    CHECK(exponential.label() == "MA 3 V");

    const terminal::CMovingAverage weighted{
        {.length = 3,
         .source = terminal::CMovingAverage::Source::High,
         .method = terminal::CMovingAverage::Method::Weighted}};
    const std::vector<double> wma = weighted.process(bars);
    REQUIRE(wma.size() == 4);
    CHECK_FALSE(std::isfinite(wma[0]));
    CHECK_FALSE(std::isfinite(wma[1]));
    CHECK(wma[2] == Catch::Approx(14.0 / 3.0));
    CHECK(wma[3] == Catch::Approx(23.0 / 3.0));
    CHECK(weighted.label() == "MA 3 H");
}

TEST_CASE("CMovingAverage length 1 is the selected field for every method")
{
    const std::vector<terminal::Bar> bars{
        movingAverageBar(1.0, 8.0, 0.5, 3.0, 10.0),
        movingAverageBar(5.0, 9.0, 0.25, 7.0, 12.0),
    };
    for (const terminal::CMovingAverage::Method method : {terminal::CMovingAverage::Method::Simple,
                                                          terminal::CMovingAverage::Method::Exponential,
                                                          terminal::CMovingAverage::Method::Weighted})
    {
        const terminal::CMovingAverage average{
            {.length = 1, .source = terminal::CMovingAverage::Source::Low, .method = method}};
        const std::vector<double> values = average.process(bars);
        REQUIRE(values.size() == 2);
        CHECK(values[0] == Catch::Approx(0.5));
        CHECK(values[1] == Catch::Approx(0.25));
        CHECK(average.label() == "MA 1 L");
    }
}

TEST_CASE("CMovingAverage exponential and weighted longer than the series are NaN")
{
    const auto bars = movingAverageCloses({1.0, 2.0, 3.0});
    for (const terminal::CMovingAverage::Method method : {terminal::CMovingAverage::Method::Exponential,
                                                          terminal::CMovingAverage::Method::Weighted})
    {
        const terminal::CMovingAverage average{
            {.length = 4, .source = terminal::CMovingAverage::Source::Close, .method = method}};
        const std::vector<double> values = average.process(bars);
        REQUIRE(values.size() == bars.size());
        for (const double value : values)
        {
            CHECK_FALSE(std::isfinite(value));
        }
        CHECK(average.label() == "MA 4 C");
    }
}

TEST_CASE("moving average chartbook method selects exponential and weighted")
{
    const auto bars = movingAverageCloses({2.0, 4.0, 6.0, 10.0, 2.0, 8.0});
    const auto run = [&](terminal::CMovingAverage::Method method) {
        terminal::CStudyInstance inst;
        inst.id = 3;
        inst.type_id = "moving_average";
        inst.options = {3, static_cast<int>(terminal::CMovingAverage::Source::Close), static_cast<int>(method)};
        const auto series = terminal::computeStudies(bars, std::vector<terminal::CStudyInstance>{inst});
        const terminal::CMovingAverage average{
            {.length = 3, .source = terminal::CMovingAverage::Source::Close, .method = method},
        };
        const std::vector<double> expected = average.process(bars);
        REQUIRE(series.size() == 1);
        CHECK(series[0].label == "MA 3 C");
        REQUIRE(series[0].values.size() == expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            if (!std::isfinite(expected[index]))
            {
                CHECK_FALSE(std::isfinite(series[0].values[index]));
                continue;
            }
            CHECK(series[0].values[index] == Catch::Approx(expected[index]));
        }
    };
    run(terminal::CMovingAverage::Method::Simple);
    run(terminal::CMovingAverage::Method::Exponential);
    run(terminal::CMovingAverage::Method::Weighted);
}

TEST_CASE("moving average method option is shown")
{
    const terminal::StudyType* type = terminal::findStudy("moving_average");
    REQUIRE(type != nullptr);
    CHECK(std::string_view{type->id} == "moving_average");
    REQUIRE(type->options.size() == 3);
    CHECK(std::string_view{type->options[2].key} == "method");
    CHECK(type->options[2].shown);
    CHECK(type->options[2].fallback == 0);
    REQUIRE(type->options[2].choices.size() == 3);
    CHECK(std::string_view{type->options[2].choices[0].token} == "simple");
    CHECK(std::string_view{type->options[2].choices[0].label} == "Simple");
    CHECK(std::string_view{type->options[2].choices[1].token} == "exponential");
    CHECK(std::string_view{type->options[2].choices[1].label} == "Exponential");
    CHECK(std::string_view{type->options[2].choices[2].token} == "weighted");
    CHECK(std::string_view{type->options[2].choices[2].label} == "Weighted");
}
