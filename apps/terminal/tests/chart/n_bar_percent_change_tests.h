// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CStudyCompute.h"
#include "chart/studies/CNBarPercentChange.h"
#include "chart/studies/StudyRegistry.h"

#include <cmath>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace {

terminal::Bar percentChangeBar(double open, double high, double low, double close)
{
    terminal::Bar bar;
    bar.open = open;
    bar.high = high;
    bar.low = low;
    bar.close = close;
    return bar;
}

std::vector<terminal::Bar> percentChangeCloses(std::initializer_list<double> closes)
{
    std::vector<terminal::Bar> bars;
    bars.reserve(closes.size());
    for (const double close : closes)
    {
        bars.push_back(percentChangeBar(close, close, close, close));
    }
    return bars;
}

terminal::CStudyInstance makePercentChangeInstance(int id, int length, terminal::CNBarPercentChange::Source source)
{
    terminal::CStudyInstance inst;
    inst.id = id;
    inst.type_id = "n_bar_percent_change";
    inst.chart_region = 2;
    inst.options = {length, static_cast<int>(source)};
    return inst;
}

}  // namespace

TEST_CASE("CNBarPercentChange defaults to a 1 bar close")
{
    const terminal::CNBarPercentChange study;
    CHECK(study.options().length == terminal::CNBarPercentChange::kDefaultLength);
    CHECK(study.options().source == terminal::CNBarPercentChange::Source::Close);
    CHECK(study.label() == "n% 1 C");
}

TEST_CASE("CNBarPercentChange clamps length")
{
    terminal::CNBarPercentChange study{{.length = 0, .source = terminal::CNBarPercentChange::Source::High}};
    CHECK(study.options().length == terminal::CNBarPercentChange::kMinLength);
    CHECK(study.options().source == terminal::CNBarPercentChange::Source::High);
    CHECK(study.label() == "n% 1 H");

    study.setOptions({.length = 99999, .source = terminal::CNBarPercentChange::Source::Low});
    CHECK(study.options().length == terminal::CNBarPercentChange::kMaxLength);
    CHECK(study.options().source == terminal::CNBarPercentChange::Source::Low);
    CHECK(study.label() == "n% 10000 L");
}

TEST_CASE("CNBarPercentChange of an empty series is empty")
{
    const terminal::CNBarPercentChange study{{.length = 5}};
    CHECK(study.process({}).empty());
    CHECK(study.label() == "n% 5 C");
}

TEST_CASE("CNBarPercentChange is the percent move from n bars back")
{
    const auto bars = percentChangeCloses({100.0, 110.0, 121.0});
    const terminal::CNBarPercentChange one{{.length = 1}};
    const std::vector<double> step = one.process(bars);
    REQUIRE(step.size() == 3);
    CHECK_FALSE(std::isfinite(step[0]));
    CHECK(step[1] == Catch::Approx(10.0));
    CHECK(step[2] == Catch::Approx(10.0));
    CHECK(one.label() == "n% 1 C");

    const terminal::CNBarPercentChange two{{.length = 2}};
    const std::vector<double> span = two.process(bars);
    REQUIRE(span.size() == 3);
    CHECK_FALSE(std::isfinite(span[0]));
    CHECK_FALSE(std::isfinite(span[1]));
    CHECK(span[2] == Catch::Approx(21.0));
    CHECK(two.label() == "n% 2 C");
}

TEST_CASE("CNBarPercentChange reads the selected open high low or close")
{
    const std::vector<terminal::Bar> bars{
        percentChangeBar(50.0, 80.0, 40.0, 100.0),
        percentChangeBar(75.0, 88.0, 20.0, 90.0),
    };
    const auto run = [&](terminal::CNBarPercentChange::Source source) {
        const terminal::CNBarPercentChange study{{.length = 1, .source = source}};
        return study.process(bars);
    };

    const std::vector<double> open = run(terminal::CNBarPercentChange::Source::Open);
    const std::vector<double> high = run(terminal::CNBarPercentChange::Source::High);
    const std::vector<double> low = run(terminal::CNBarPercentChange::Source::Low);
    const std::vector<double> close = run(terminal::CNBarPercentChange::Source::Close);
    REQUIRE(open.size() == 2);
    CHECK_FALSE(std::isfinite(open[0]));
    CHECK(open[1] == Catch::Approx(50.0));
    CHECK(high[1] == Catch::Approx(10.0));
    CHECK(low[1] == Catch::Approx(-50.0));
    CHECK(close[1] == Catch::Approx(-10.0));
}

TEST_CASE("CNBarPercentChange leaves a zero reference as NaN")
{
    const auto bars = percentChangeCloses({0.0, 10.0, 20.0});
    const terminal::CNBarPercentChange study{{.length = 1}};
    const std::vector<double> values = study.process(bars);
    REQUIRE(values.size() == 3);
    CHECK_FALSE(std::isfinite(values[0]));
    CHECK_FALSE(std::isfinite(values[1]));
    CHECK(values[2] == Catch::Approx(100.0));
}

TEST_CASE("CNBarPercentChange length at least the series is all NaN")
{
    const auto bars = percentChangeCloses({100.0, 110.0, 120.0});
    const terminal::CNBarPercentChange study{{.length = 3}};
    const std::vector<double> values = study.process(bars);
    REQUIRE(values.size() == bars.size());
    for (const double value : values)
    {
        CHECK_FALSE(std::isfinite(value));
    }
}

TEST_CASE("n bar percent change registers one trace")
{
    const terminal::StudyType* type = terminal::findStudy("n_bar_percent_change");
    REQUIRE(type != nullptr);
    CHECK(type->process != nullptr);
    CHECK(type->label != nullptr);
    CHECK(std::string_view{type->id} == "n_bar_percent_change");
    CHECK(std::string_view{type->display_name} == "n bar % change");
    REQUIRE(type->options.size() == 2);
    CHECK(std::string_view{type->options[0].key} == "length");
    CHECK(std::string_view{type->options[1].key} == "source");
    CHECK(type->options[1].choices.size() == 4);
    CHECK(std::string_view{type->options[1].choices[0].token} == "close");
    CHECK(std::string_view{type->options[1].choices[1].token} == "open");
    CHECK(std::string_view{type->options[1].choices[2].token} == "high");
    CHECK(std::string_view{type->options[1].choices[3].token} == "low");
    REQUIRE(type->outputs.size() == 1);
    CHECK(std::string_view{type->outputs[0].key} == "change");
    CHECK(type->graph == terminal::StudyGraph::Line);
    CHECK(type->default_chart_region == terminal::kStudyVolumeChartRegion);
    CHECK(type->anchor_zero);
    CHECK_FALSE(type->color_by_bar);
    CHECK(type->value_decimals == 2);

    const auto bars = percentChangeCloses({100.0, 110.0, 121.0});
    const std::vector<terminal::CStudyInstance> studies{
        makePercentChangeInstance(4, 2, terminal::CNBarPercentChange::Source::Close),
    };
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].study_id == 4);
    CHECK(series[0].label == "n% 2 C");
    CHECK(series[0].line == terminal::StudyLineStyle::Value);
    CHECK(series[0].placement == terminal::StudyPlacement::Subgraph);
    CHECK(series[0].chart_region == 2);
    CHECK(series[0].anchor_zero);
    CHECK(terminal::studyChartRegionCount(series) == 1);
    CHECK(terminal::studyValueLabelText(series[0].label, series[0].values, series[0].value_decimals) ==
          "n% 2 C  +21.00");
    CHECK(series[0].value_decimals == 2);
    CHECK_FALSE(series[0].histogram);
    CHECK(terminal::studyShortLabel(studies[0]) == "n% 2 C");
    REQUIRE(series[0].values.size() == 3);
    CHECK_FALSE(std::isfinite(series[0].values[0]));
    CHECK_FALSE(std::isfinite(series[0].values[1]));
    CHECK(series[0].values[2] == Catch::Approx(21.0));

    const std::vector<terminal::Bar> empty;
    const auto blank = terminal::computeStudies(empty, studies);
    REQUIRE(blank.size() == 1);
    CHECK(blank[0].values.empty());
    CHECK(blank[0].label == "n% 2 C");
    CHECK(terminal::studyValueLabelText(blank[0].label, blank[0].values, 2) == "n% 2 C");
}

TEST_CASE("n bar percent change defaults to a value label")
{
    terminal::CStudyInstance inst;
    inst.type_id = "n_bar_percent_change";
    terminal::assignStudyOutputDefaults(inst, 0);
    REQUIRE(inst.outputs.size() == 1);
    CHECK(inst.outputs[0].line == terminal::StudyLineStyle::Value);

    inst.chart_region = 1;
    inst.options = {1, static_cast<int>(terminal::CNBarPercentChange::Source::Close)};
    const auto bars = percentChangeCloses({100.0, 90.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].line == terminal::StudyLineStyle::Value);
    CHECK(series[0].placement == terminal::StudyPlacement::Overlay);
    terminal::ChartVisibleWindow win;
    win.first = 0;
    win.last = 1;
    CHECK_FALSE(terminal::overlayYExtent(series, win, 2).valid);
    CHECK(terminal::studyChartRegionCount(series) == 1);
    CHECK(terminal::studyValueLabelText(series[0].label, series[0].values, series[0].value_decimals) ==
          "n% 1 C  -10.00");

    const auto limits =
        terminal::computeStudyRegionYLimits(series, 1, win, 2, 0.0f, 0.0, 0.0);
    CHECK(limits.min == Catch::Approx(0.0));
    CHECK(limits.max == Catch::Approx(1.0));
}
