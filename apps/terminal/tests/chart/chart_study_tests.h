// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CStudyCompute.h"

#include <cmath>
#include <initializer_list>
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
                                         terminal::StudySource source,
                                         bool enabled = true)
{
    terminal::CStudyInstance inst;
    inst.id = id;
    inst.kind = terminal::StudyKind::MovingAverage;
    inst.enabled = enabled;
    terminal::MovingAverageParams params;
    params.source = source;
    params.length = length;
    params.method = terminal::MovingAverageMethod::Simple;
    inst.params = params;
    return inst;
}

}  // namespace

TEST_CASE("empty bars still emit one labeled moving average")
{
    const std::vector<terminal::Bar> bars;
    const std::vector<terminal::CStudyInstance> studies{
        makeSmaInstance(7, 20, terminal::StudySource::Close)};
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
        makeSmaInstance(1, 1, terminal::StudySource::Close)};
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
        makeSmaInstance(1, 3, terminal::StudySource::Close)};
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
        makeSmaInstance(1, 1, terminal::StudySource::Open),
        makeSmaInstance(2, 1, terminal::StudySource::Close)};
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
        makeSmaInstance(1, 4, terminal::StudySource::Close)};
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
        makeSmaInstance(1, 1, terminal::StudySource::Close, false)};
    CHECK(terminal::computeStudies(bars, only_disabled).empty());

    const std::vector<terminal::CStudyInstance> mixed{
        makeSmaInstance(1, 1, terminal::StudySource::Close, false),
        makeSmaInstance(2, 1, terminal::StudySource::Close, true)};
    const auto series = terminal::computeStudies(bars, mixed);
    REQUIRE(series.size() == 1);
    CHECK(series[0].study_id == 2);
    CHECK(series[0].label == "MA 1 C");
}

TEST_CASE("two moving averages keep id order and labels")
{
    const auto bars = smaCloseBars({1.0, 2.0, 3.0});
    terminal::CStudyInstance slow = makeSmaInstance(4, 20, terminal::StudySource::Close);
    terminal::CStudyInstance fast = makeSmaInstance(8, 50, terminal::StudySource::Close);
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

TEST_CASE("clampMovingAverageParams locks length and method")
{
    terminal::MovingAverageParams params;
    params.length = 0;
    params.method = terminal::MovingAverageMethod::Exponential;
    terminal::clampMovingAverageParams(params);
    CHECK(params.length == 1);
    CHECK(params.method == terminal::MovingAverageMethod::Simple);

    params.length = 99999;
    params.method = terminal::MovingAverageMethod::Weighted;
    terminal::clampMovingAverageParams(params);
    CHECK(params.length == terminal::kStudyMaxLength);
    CHECK(params.method == terminal::MovingAverageMethod::Simple);

    params.length = 20;
    params.source = terminal::StudySource::High;
    params.method = terminal::MovingAverageMethod::Simple;
    terminal::clampMovingAverageParams(params);
    CHECK(params.length == 20);
    CHECK(params.source == terminal::StudySource::High);
    CHECK(params.method == terminal::MovingAverageMethod::Simple);
}

TEST_CASE("studyShortLabel formats length and source")
{
    const auto inst = makeSmaInstance(1, 20, terminal::StudySource::Close);
    CHECK(terminal::studyShortLabel(inst) == "MA 20 C");
}

TEST_CASE("non-simple moving average methods compute as simple")
{
    auto inst = makeSmaInstance(5, 1, terminal::StudySource::Close);
    auto* params = std::get_if<terminal::MovingAverageParams>(&inst.params);
    REQUIRE(params != nullptr);
    params->method = terminal::MovingAverageMethod::Exponential;
    const auto bars = smaCloseBars({4.0, 8.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 1);
    REQUIRE(series[0].values.size() == 2);
    CHECK(series[0].values[0] == Catch::Approx(4.0));
    CHECK(series[0].values[1] == Catch::Approx(8.0));
    CHECK(series[0].label == "MA 1 C");
}

TEST_CASE("unsupported study kinds are omitted")
{
    auto inst = makeSmaInstance(1, 1, terminal::StudySource::Close);
    inst.kind = static_cast<terminal::StudyKind>(9);
    const auto bars = smaCloseBars({1.0, 2.0, 3.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    CHECK(terminal::computeStudies(bars, studies).empty());
    CHECK_FALSE(terminal::isStudyInstanceSupported(inst));
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
        makeSmaInstance(1, 2, terminal::StudySource::Close)};

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
