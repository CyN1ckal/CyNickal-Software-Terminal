// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CStudyCompute.h"
#include "chart/studies/CMovingAverage.h"
#include "chart/studies/StudyRegistry.h"

#include <cmath>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace {

std::vector<terminal::Bar> smaCloseBars(std::initializer_list<double> closes)
{
    std::vector<terminal::Bar> bars;
    bars.reserve(closes.size());
    for (const double close : closes)
    {
        terminal::Bar bar;
        bar.open = close;
        bar.high = close;
        bar.low = close;
        bar.close = close;
        bars.push_back(bar);
    }
    return bars;
}

terminal::CStudyInstance makeSmaInstance(int id,
                                         int length,
                                         terminal::CMovingAverage::Source source,
                                         bool enabled = true)
{
    terminal::CStudyInstance inst;
    inst.id = id;
    inst.type_id = "moving_average";
    inst.enabled = enabled;
    // The third slot is the chartbook method token. The average ignores it.
    inst.options = {length, static_cast<int>(source), 0};
    return inst;
}

}  // namespace

TEST_CASE("empty bars still emit one labeled moving average")
{
    const std::vector<terminal::Bar> bars;
    const std::vector<terminal::CStudyInstance> studies{
        makeSmaInstance(7, 20, terminal::CMovingAverage::Source::Close)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].study_id == 7);
    CHECK(series[0].values.empty());
    CHECK(series[0].label == "MA 20 C");
    CHECK(series[0].placement == terminal::StudyPlacement::Overlay);
}

TEST_CASE("length 1 moving average copies the close")
{
    const auto bars = smaCloseBars({1.5, 2.5, 3.5});
    const std::vector<terminal::CStudyInstance> studies{
        makeSmaInstance(1, 1, terminal::CMovingAverage::Source::Close)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    REQUIRE(series[0].values.size() == 3);
    CHECK(series[0].values[0] == Catch::Approx(1.5));
    CHECK(series[0].values[1] == Catch::Approx(2.5));
    CHECK(series[0].values[2] == Catch::Approx(3.5));
}

TEST_CASE("length 3 simple moving average warms up with NaN")
{
    const auto bars = smaCloseBars({1.0, 2.0, 3.0, 4.0, 5.0});
    const std::vector<terminal::CStudyInstance> studies{
        makeSmaInstance(1, 3, terminal::CMovingAverage::Source::Close)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    REQUIRE(series[0].values.size() == 5);
    CHECK_FALSE(std::isfinite(series[0].values[0]));
    CHECK_FALSE(std::isfinite(series[0].values[1]));
    CHECK(series[0].values[2] == Catch::Approx(2.0));
    CHECK(series[0].values[3] == Catch::Approx(3.0));
    CHECK(series[0].values[4] == Catch::Approx(4.0));
}

TEST_CASE("moving average source selects open or close")
{
    terminal::Bar first;
    first.open = 1.0;
    first.high = 8.0;
    first.low = 0.5;
    first.close = 3.0;
    terminal::Bar second = first;
    second.open = 5.0;
    second.close = 7.0;
    const std::vector<terminal::Bar> bars{first, second};
    const std::vector<terminal::CStudyInstance> studies{
        makeSmaInstance(1, 1, terminal::CMovingAverage::Source::Open),
        makeSmaInstance(2, 1, terminal::CMovingAverage::Source::Close)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 2);
    REQUIRE(series[0].values.size() == 2);
    REQUIRE(series[1].values.size() == 2);
    CHECK(series[0].label == "MA 1 O");
    CHECK(series[0].values[0] == Catch::Approx(1.0));
    CHECK(series[0].values[1] == Catch::Approx(5.0));
    CHECK(series[1].label == "MA 1 C");
    CHECK(series[1].values[0] == Catch::Approx(3.0));
    CHECK(series[1].values[1] == Catch::Approx(7.0));
}

TEST_CASE("moving average longer than the series is all NaN")
{
    const auto bars = smaCloseBars({1.0, 2.0, 3.0});
    const std::vector<terminal::CStudyInstance> studies{
        makeSmaInstance(1, 4, terminal::CMovingAverage::Source::Close)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    REQUIRE(series[0].values.size() == bars.size());
    CHECK(series[0].label == "MA 4 C");
    for (const double value : series[0].values)
    {
        CHECK_FALSE(std::isfinite(value));
    }
}

TEST_CASE("disabled studies are omitted")
{
    const auto bars = smaCloseBars({1.0, 2.0, 3.0, 4.0, 5.0});
    const std::vector<terminal::CStudyInstance> only_disabled{
        makeSmaInstance(1, 1, terminal::CMovingAverage::Source::Close, false)};
    CHECK(terminal::computeStudies(bars, only_disabled).empty());

    const std::vector<terminal::CStudyInstance> mixed{
        makeSmaInstance(1, 1, terminal::CMovingAverage::Source::Close, false),
        makeSmaInstance(2, 1, terminal::CMovingAverage::Source::Close, true)};
    const auto series = terminal::computeStudies(bars, mixed);
    REQUIRE(series.size() == 1);
    CHECK(series[0].study_id == 2);
    CHECK(series[0].label == "MA 1 C");
}

TEST_CASE("two moving averages keep id order and labels")
{
    const auto bars = smaCloseBars({1.0, 2.0, 3.0});
    terminal::CStudyInstance slow = makeSmaInstance(4, 20, terminal::CMovingAverage::Source::Close);
    terminal::CStudyInstance fast = makeSmaInstance(8, 50, terminal::CMovingAverage::Source::Close);
    slow.color = terminal::kStudyPalette[0];
    fast.color = terminal::kStudyPalette[1];
    const std::vector<terminal::CStudyInstance> studies{slow, fast};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 2);
    CHECK(series[0].study_id == 4);
    CHECK(series[1].study_id == 8);
    CHECK(series[0].label == "MA 20 C");
    CHECK(series[1].label == "MA 50 C");
    CHECK(series[0].color == terminal::kStudyPalette[0]);
    CHECK(series[1].color == terminal::kStudyPalette[1]);
    CHECK(series[0].values.size() == 3);
    CHECK(series[1].values.size() == 3);
}

TEST_CASE("studyShortLabel formats length and source")
{
    const auto inst = makeSmaInstance(1, 20, terminal::CMovingAverage::Source::Close);
    CHECK(terminal::studyShortLabel(inst) == "MA 20 C");
}

TEST_CASE("moving average and volume register process callbacks")
{
    const terminal::StudyType* moving = terminal::findStudy("moving_average");
    const terminal::StudyType* volume = terminal::findStudy("volume");
    REQUIRE(moving != nullptr);
    REQUIRE(volume != nullptr);
    CHECK(moving->process != nullptr);
    CHECK(volume->process != nullptr);
    CHECK(moving->options.size() == 3);
    CHECK(volume->options.empty());
    REQUIRE(moving->outputs.size() == 1);
    REQUIRE(volume->outputs.size() == 2);
    CHECK(std::string_view{moving->outputs[0].key} == "average");
    CHECK(std::string_view{volume->outputs[0].key} == "up");
    CHECK(std::string_view{volume->outputs[1].key} == "down");
    CHECK(volume->color_by_bar);
    CHECK(volume->outputs[0].palette_index == 2);
    CHECK(volume->outputs[1].palette_index == 3);
    CHECK(moving->default_chart_region == terminal::kStudyMainChartRegion);
    CHECK(volume->default_chart_region == terminal::kStudyVolumeChartRegion);
    CHECK(volume->graph == terminal::StudyGraph::Histogram);
    CHECK(volume->anchor_zero);
    CHECK(volume->palette_index == 2);
}

TEST_CASE("unsupported study kinds are omitted")
{
    auto inst = makeSmaInstance(1, 1, terminal::CMovingAverage::Source::Close);
    inst.type_id = "not-a-study";
    const auto bars = smaCloseBars({1.0, 2.0, 3.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    CHECK(terminal::computeStudies(bars, studies).empty());
    CHECK_FALSE(terminal::isStudyInstanceSupported(inst));
}

TEST_CASE("study line styles are solid dotted and dashed")
{
    terminal::StudyLineStyle style = terminal::StudyLineStyle::Solid;
    CHECK(terminal::parseStudyLineStyle("dotted", style));
    CHECK(style == terminal::StudyLineStyle::Dotted);
    CHECK(std::string_view{terminal::studyLineStyleToken(style)} == "dotted");
    CHECK(std::string_view{terminal::studyLineStyleLabel(style)} == "Dotted");
    CHECK(terminal::parseStudyLineStyle("dashed", style));
    CHECK(style == terminal::StudyLineStyle::Dashed);
    CHECK(std::string_view{terminal::studyLineStyleLabel(style)} == "Dashed");
    CHECK(terminal::parseStudyLineStyle("solid", style));
    CHECK(style == terminal::StudyLineStyle::Solid);
    CHECK_FALSE(terminal::parseStudyLineStyle("dash", style));
    CHECK(style == terminal::StudyLineStyle::Solid);
}

TEST_CASE("study palette cycles stratum colors")
{
    CHECK(terminal::kStudyPalette[0] == 0xFFC9976Fu);
    CHECK(terminal::kStudyPalette[1] == 0xFF5285C0u);
    CHECK(terminal::kStudyPalette[2] == 0xFF638A5Fu);
    CHECK(terminal::kStudyPalette[3] == 0xFF4E54B5u);
    CHECK(terminal::kStudyDefaultColor == terminal::kStudyPalette[0]);
    CHECK(terminal::kStudyPaletteCount == 4);
    CHECK(terminal::studyPaletteColor(0) == terminal::kStudyPalette[0]);
    CHECK(terminal::studyPaletteColor(4) == terminal::kStudyPalette[0]);
    CHECK(terminal::studyPaletteColor(-1) == terminal::kStudyPalette[3]);
}

TEST_CASE("overlayYExtent uses finite samples and skips mismatches")
{
    terminal::CStudySeries warmup;
    warmup.placement = terminal::StudyPlacement::Overlay;
    warmup.values = {terminal::studyNaN(), terminal::studyNaN(), 2.0, 8.0, 3.0};
    terminal::ChartVisibleWindow win;
    win.first = 0;
    win.last = 4;
    const std::vector<terminal::CStudySeries> warmup_only{warmup};
    const auto extent = terminal::overlayYExtent(warmup_only, win, 5);
    CHECK(extent.valid);
    CHECK(extent.min == Catch::Approx(2.0));
    CHECK(extent.max == Catch::Approx(8.0));

    terminal::CStudySeries all_nan = warmup;
    all_nan.values = {terminal::studyNaN(), terminal::studyNaN(), terminal::studyNaN()};
    win.last = 2;
    const std::vector<terminal::CStudySeries> nan_only{all_nan};
    CHECK_FALSE(terminal::overlayYExtent(nan_only, win, 3).valid);

    terminal::CStudySeries mismatch = warmup;
    mismatch.values = {1.0, 2.0};
    win.last = 4;
    const std::vector<terminal::CStudySeries> mismatched{mismatch};
    CHECK_FALSE(terminal::overlayYExtent(mismatched, win, 5).valid);

    terminal::CStudySeries subgraph = warmup;
    subgraph.placement = terminal::StudyPlacement::Subgraph;
    subgraph.values = {1.0, 50.0, 3.0};
    terminal::CStudySeries matched;
    matched.placement = terminal::StudyPlacement::Overlay;
    matched.values = {terminal::studyNaN(), 4.0, 6.0};
    win.first = 0;
    win.last = 2;
    const std::vector<terminal::CStudySeries> mixed{mismatch, subgraph, matched};
    const auto mixed_extent = terminal::overlayYExtent(mixed, win, 3);
    CHECK(mixed_extent.valid);
    CHECK(mixed_extent.min == Catch::Approx(4.0));
    CHECK(mixed_extent.max == Catch::Approx(6.0));
}

TEST_CASE("studiesForLoad follows ready bars and drops other statuses")
{
    const std::vector<terminal::CStudyInstance> studies{
        makeSmaInstance(1, 2, terminal::CMovingAverage::Source::Close)};

    terminal::ChartLoadResult ready;
    ready.status = terminal::ChartLoadStatus::Ready;
    ready.bars = smaCloseBars({1.0, 2.0, 3.0, 4.0, 5.0});
    auto series = terminal::studiesForLoad(ready, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].values.size() == 5);

    ready.bars.clear();
    CHECK(terminal::studiesForLoad(ready, studies).empty());

    ready.bars = smaCloseBars({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0});
    series = terminal::studiesForLoad(ready, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].values.size() == 10);

    ready.bars = smaCloseBars({1.0, 2.0, 3.0});
    series = terminal::studiesForLoad(ready, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].values.size() == 3);

    const std::vector<terminal::Bar> leftover = smaCloseBars({9.0, 8.0, 7.0});
    for (const terminal::ChartLoadStatus status :
         {terminal::ChartLoadStatus::Empty, terminal::ChartLoadStatus::Error})
    {
        terminal::ChartLoadResult other;
        other.status = status;
        other.bars = leftover;
        CHECK(terminal::studiesForLoad(other, studies).empty());
    }

    terminal::ChartLoadResult busy;
    busy.status = terminal::ChartLoadStatus::Busy;
    CHECK(terminal::studiesForLoad(busy, studies).empty());

    busy.bars = leftover;
    CHECK(terminal::studiesForLoad(busy, studies).empty());
}

namespace {

std::vector<terminal::Bar> volumeBars(std::initializer_list<double> volumes)
{
    std::vector<terminal::Bar> bars;
    bars.reserve(volumes.size());
    for (const double volume : volumes)
    {
        terminal::Bar bar;
        bar.open = 1.0;
        bar.high = 2.0;
        bar.low = 0.5;
        bar.close = 1.5;
        bar.volume = volume;
        bars.push_back(bar);
    }
    return bars;
}

terminal::CStudyInstance makeVolumeInstance(int id, int region = terminal::kStudyVolumeChartRegion,
                                            bool enabled = true)
{
    terminal::CStudyInstance inst;
    inst.id = id;
    inst.type_id = "volume";
    inst.enabled = enabled;
    inst.chart_region = region;
    inst.color = terminal::kStudyPalette[2];
    inst.outputs = {
        {.color = terminal::kStudyPalette[2], .line = terminal::StudyLineStyle::Solid},
        {.color = terminal::kStudyPalette[3], .line = terminal::StudyLineStyle::Solid},
    };
    return inst;
}

}  // namespace

TEST_CASE("moving average of volume averages bar volume")
{
    const auto bars = volumeBars({2.0, 4.0, 6.0, 8.0});
    const std::vector<terminal::CStudyInstance> studies{
        makeSmaInstance(3, 2, terminal::CMovingAverage::Source::Volume)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].label == "MA 2 V");
    CHECK(series[0].placement == terminal::StudyPlacement::Overlay);
    REQUIRE(series[0].values.size() == 4);
    CHECK_FALSE(std::isfinite(series[0].values[0]));
    CHECK(series[0].values[1] == Catch::Approx(3.0));
    CHECK(series[0].values[2] == Catch::Approx(5.0));
    CHECK(series[0].values[3] == Catch::Approx(7.0));
}

TEST_CASE("volume copies each bar and defaults to chart region 2")
{
    const auto bars = volumeBars({10.0, 0.0, 25.5});
    const std::vector<terminal::CStudyInstance> studies{makeVolumeInstance(3)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].study_id == 3);
    CHECK(series[0].type_id == "volume");
    CHECK(series[0].histogram);
    CHECK(series[0].anchor_zero);
    CHECK(series[0].label == "Vol");
    CHECK(series[0].chart_region == 2);
    CHECK(series[0].placement == terminal::StudyPlacement::Subgraph);
    CHECK(series[0].color_by_bar);
    CHECK(series[0].color == terminal::kStudyPalette[2]);
    CHECK(series[0].down_color == terminal::kStudyPalette[3]);
    REQUIRE(series[0].values.size() == 3);
    CHECK(series[0].values[0] == Catch::Approx(10.0));
    CHECK(series[0].values[1] == Catch::Approx(0.0));
    CHECK(series[0].values[2] == Catch::Approx(25.5));
    CHECK(terminal::studyChartRegionCount(series) == 2);
}

TEST_CASE("volume up and down colors are independent")
{
    auto inst = makeVolumeInstance(3);
    inst.outputs[0].color = terminal::kStudyPalette[0];
    inst.outputs[1].color = terminal::kStudyPalette[1];
    const auto bars = volumeBars({5.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].color_by_bar);
    CHECK(series[0].color == terminal::kStudyPalette[0]);
    CHECK(series[0].down_color == terminal::kStudyPalette[1]);
    CHECK(terminal::studyHistogramColor(series[0], true) == terminal::kStudyPalette[0]);
    CHECK(terminal::studyHistogramColor(series[0], false) == terminal::kStudyPalette[1]);

    terminal::CStudyInstance fresh;
    fresh.type_id = "volume";
    fresh.color = 1;
    terminal::normalizeStudyOutputs(fresh);
    REQUIRE(fresh.outputs.size() == 2);
    CHECK(fresh.outputs[0].color == terminal::kStudyPalette[2]);
    CHECK(fresh.outputs[1].color == terminal::kStudyPalette[3]);
    CHECK(fresh.color == terminal::kStudyPalette[2]);
}

TEST_CASE("empty bars still emit one labeled volume series")
{
    const std::vector<terminal::Bar> bars;
    const std::vector<terminal::CStudyInstance> studies{makeVolumeInstance(1)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].values.empty());
    CHECK(series[0].label == "Vol");
    CHECK(series[0].placement == terminal::StudyPlacement::Subgraph);
}

TEST_CASE("volume on chart region 1 is an overlay")
{
    auto inst = makeVolumeInstance(4, 1);
    const auto bars = volumeBars({8.0, 12.0, 9.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].chart_region == terminal::kStudyMainChartRegion);
    CHECK(series[0].placement == terminal::StudyPlacement::Overlay);
    terminal::ChartVisibleWindow win;
    win.first = 0;
    win.last = 2;
    const auto extent = terminal::overlayYExtent(series, win, 3);
    CHECK(extent.valid);
    CHECK(extent.min == Catch::Approx(8.0));
    CHECK(extent.max == Catch::Approx(12.0));
}

TEST_CASE("moving average chart region selects the subgraph")
{
    auto inst = makeSmaInstance(6, 1, terminal::CMovingAverage::Source::Close);
    inst.chart_region = 4;
    const auto bars = smaCloseBars({2.0, 4.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].chart_region == 4);
    CHECK(series[0].placement == terminal::StudyPlacement::Subgraph);
    CHECK(series[0].label == "MA 1 C");
    REQUIRE(series[0].values.size() == 2);
    CHECK(series[0].values[0] == Catch::Approx(2.0));
    terminal::ChartVisibleWindow win;
    win.first = 0;
    win.last = 1;
    CHECK_FALSE(terminal::overlayYExtent(series, win, 2).valid);
    CHECK(terminal::studyChartRegionCount(series) == 4);
}

TEST_CASE("chart region clamps into 1..12")
{
    CHECK(terminal::clampStudyChartRegion(0) == 1);
    CHECK(terminal::clampStudyChartRegion(1) == 1);
    CHECK(terminal::clampStudyChartRegion(12) == 12);
    CHECK(terminal::clampStudyChartRegion(99) == 12);

    auto inst = makeVolumeInstance(1, 99);
    const auto bars = volumeBars({1.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].chart_region == 12);
    CHECK(terminal::studyChartRegionCount(series) == 12);
    CHECK(terminal::studyChartRegionCount({}) == 1);
}

TEST_CASE("disabled volume is omitted and a bad payload is unsupported")
{
    const auto bars = volumeBars({1.0, 2.0});
    const std::vector<terminal::CStudyInstance> disabled{makeVolumeInstance(1, 2, false)};
    CHECK(terminal::computeStudies(bars, disabled).empty());

    auto inst = makeVolumeInstance(2);
    inst.options = {20, 0, 0};
    CHECK_FALSE(terminal::isStudyInstanceSupported(inst));
    const std::vector<terminal::CStudyInstance> bad{inst};
    CHECK(terminal::computeStudies(bars, bad).empty());
}

TEST_CASE("study region limits baseline volume and fit a moving average")
{
    terminal::CStudySeries volume;
    volume.anchor_zero = true;
    volume.histogram = true;
    volume.chart_region = 2;
    volume.values = {10.0, 40.0, 5.0};
    terminal::CStudySeries other;
    other.chart_region = 3;
    other.values = {100.0, 110.0, 90.0};
    const std::vector<terminal::CStudySeries> series{volume, other};
    terminal::ChartVisibleWindow win;
    win.first = 0;
    win.last = 2;

    const auto vol = terminal::computeStudyRegionYLimits(series, 2, win, 3, 0.0f, 0.0, 0.0);
    CHECK(vol.min == Catch::Approx(0.0));
    CHECK(vol.max == Catch::Approx(40.0));

    const auto padded = terminal::computeStudyRegionYLimits(series, 2, win, 3, 10.0f, 0.0, 3.0);
    CHECK(padded.min == Catch::Approx(-1.0));
    CHECK(padded.max == Catch::Approx(47.0));

    const auto ma = terminal::computeStudyRegionYLimits(series, 3, win, 3, 0.0f, 0.0, 0.0);
    CHECK(ma.min == Catch::Approx(90.0));
    CHECK(ma.max == Catch::Approx(110.0));

    const auto empty = terminal::computeStudyRegionYLimits(series, 4, win, 3, 0.0f, 0.0, 0.0);
    CHECK(empty.min == Catch::Approx(0.0));
    CHECK(empty.max == Catch::Approx(1.0));

    volume.values[1] = terminal::studyNaN();
    const std::vector<terminal::CStudySeries> with_nan{volume};
    const auto finite = terminal::computeStudyRegionYLimits(with_nan, 2, win, 3, 0.0f, 0.0, 0.0);
    CHECK(finite.min == Catch::Approx(0.0));
    CHECK(finite.max == Catch::Approx(10.0));
}

TEST_CASE("studiesForLoad computes volume from ready bars")
{
    const std::vector<terminal::CStudyInstance> studies{makeVolumeInstance(1)};
    terminal::ChartLoadResult ready;
    ready.status = terminal::ChartLoadStatus::Ready;
    ready.bars = volumeBars({4.0, 6.0});
    const auto series = terminal::studiesForLoad(ready, studies);
    REQUIRE(series.size() == 1);
    CHECK(series[0].values[1] == Catch::Approx(6.0));
    CHECK(series[0].chart_region == 2);
}
