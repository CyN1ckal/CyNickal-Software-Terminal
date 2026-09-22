// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CStudyCompute.h"
#include "chart/studies/CBollinger.h"
#include "chart/studies/StudyRegistry.h"

#include <cmath>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace {

std::vector<terminal::Bar> bollingerCloses(std::initializer_list<double> closes)
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
        bar.volume = close;
        bars.push_back(bar);
    }
    return bars;
}

terminal::CStudyInstance makeBollingerInstance(int id, int length, int deviations)
{
    terminal::CStudyInstance inst;
    inst.id = id;
    inst.type_id = "bollinger";
    inst.options = {length, static_cast<int>(terminal::CBollinger::Source::Close), deviations};
    return inst;
}

}  // namespace

TEST_CASE("CBollinger defaults to a 20 bar close and 2 deviations")
{
    const terminal::CBollinger study;
    CHECK(study.options().length == terminal::CBollinger::kDefaultLength);
    CHECK(study.options().deviations == terminal::CBollinger::kDefaultDeviations);
    CHECK(study.options().source == terminal::CBollinger::Source::Close);
    CHECK(study.label() == "BB 20 2 C");
}

TEST_CASE("CBollinger clamps length and deviations")
{
    terminal::CBollinger study{{.length = 0, .source = terminal::CBollinger::Source::High, .deviations = 0}};
    CHECK(study.options().length == terminal::CBollinger::kMinLength);
    CHECK(study.options().deviations == terminal::CBollinger::kMinDeviations);
    CHECK(study.options().source == terminal::CBollinger::Source::High);
    CHECK(study.label() == "BB 1 1 H");

    study.setOptions({.length = 99999, .deviations = 99});
    CHECK(study.options().length == terminal::CBollinger::kMaxLength);
    CHECK(study.options().deviations == terminal::CBollinger::kMaxDeviations);
    CHECK(study.label() == "BB 10000 10 C");
}

TEST_CASE("CBollinger of an empty series is three empty bands")
{
    const terminal::CBollinger study{{.length = 5, .deviations = 2}};
    const terminal::CBollinger::Bands bands = study.process({});
    CHECK(bands.upper.empty());
    CHECK(bands.middle.empty());
    CHECK(bands.lower.empty());
}

TEST_CASE("CBollinger length 1 copies the field onto every band")
{
    terminal::Bar bar;
    bar.open = 1.0;
    bar.high = 8.0;
    bar.low = 0.5;
    bar.close = 3.0;
    bar.volume = 10.0;
    const std::vector<terminal::Bar> bars{bar};
    const terminal::CBollinger study{{.length = 1, .source = terminal::CBollinger::Source::High, .deviations = 2}};
    const terminal::CBollinger::Bands bands = study.process(bars);
    REQUIRE(bands.middle.size() == 1);
    CHECK(bands.middle[0] == Catch::Approx(8.0));
    CHECK(bands.upper[0] == Catch::Approx(8.0));
    CHECK(bands.lower[0] == Catch::Approx(8.0));
}

TEST_CASE("CBollinger warms up then places the bands around the mean")
{
    const auto bars = bollingerCloses({1.0, 2.0, 3.0, 4.0, 5.0});
    const terminal::CBollinger study{{.length = 3, .deviations = 2}};
    const terminal::CBollinger::Bands bands = study.process(bars);
    REQUIRE(bands.middle.size() == 5);
    CHECK_FALSE(std::isfinite(bands.middle[0]));
    CHECK_FALSE(std::isfinite(bands.upper[1]));
    CHECK_FALSE(std::isfinite(bands.lower[1]));

    const double offset = 2.0 * std::sqrt(2.0 / 3.0);
    CHECK(bands.middle[2] == Catch::Approx(2.0));
    CHECK(bands.upper[2] == Catch::Approx(2.0 + offset));
    CHECK(bands.lower[2] == Catch::Approx(2.0 - offset));
    CHECK(bands.middle[3] == Catch::Approx(3.0));
    CHECK(bands.upper[3] == Catch::Approx(3.0 + offset));
    CHECK(bands.lower[3] == Catch::Approx(3.0 - offset));
    CHECK(bands.middle[4] == Catch::Approx(4.0));
    CHECK(study.label() == "BB 3 2 C");
}

TEST_CASE("CBollinger longer than the series is all NaN")
{
    const auto bars = bollingerCloses({1.0, 2.0, 3.0});
    const terminal::CBollinger study{{.length = 4, .deviations = 2}};
    const terminal::CBollinger::Bands bands = study.process(bars);
    REQUIRE(bands.upper.size() == bars.size());
    for (const double value : bands.upper)
    {
        CHECK_FALSE(std::isfinite(value));
    }
    for (const double value : bands.middle)
    {
        CHECK_FALSE(std::isfinite(value));
    }
    for (const double value : bands.lower)
    {
        CHECK_FALSE(std::isfinite(value));
    }
}

TEST_CASE("bollinger registers three traces from one study")
{
    const terminal::StudyType* type = terminal::findStudy("bollinger");
    REQUIRE(type != nullptr);
    CHECK(type->process != nullptr);
    CHECK(type->label != nullptr);
    CHECK(type->options.size() == 3);
    REQUIRE(type->outputs.size() == 3);
    CHECK(std::string_view{type->outputs[0].key} == "upper");
    CHECK(std::string_view{type->outputs[1].key} == "middle");
    CHECK(std::string_view{type->outputs[2].key} == "lower");
    CHECK(type->graph == terminal::StudyGraph::Line);
    CHECK(type->default_chart_region == terminal::kStudyMainChartRegion);

    const auto bars = bollingerCloses({1.0, 2.0, 3.0, 4.0, 5.0});
    const std::vector<terminal::CStudyInstance> studies{makeBollingerInstance(4, 3, 2)};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 3);
    CHECK(series[0].study_id == 4);
    CHECK(series[1].study_id == 4);
    CHECK(series[2].study_id == 4);
    CHECK(series[0].label == "BB 3 2 C U");
    CHECK(series[1].label == "BB 3 2 C M");
    CHECK(series[2].label == "BB 3 2 C L");
    CHECK(series[0].color == terminal::kStudyDefaultColor);
    CHECK(series[1].color == terminal::kStudyDefaultColor);
    CHECK(series[2].color == terminal::kStudyDefaultColor);
    CHECK(series[0].line == terminal::StudyLineStyle::Solid);
    CHECK(series[1].line == terminal::StudyLineStyle::Solid);
    CHECK(series[2].line == terminal::StudyLineStyle::Solid);
    CHECK(series[0].placement == terminal::StudyPlacement::Overlay);
    CHECK(terminal::studyShortLabel(studies[0]) == "BB 3 2 C");
    REQUIRE(series[1].values.size() == 5);
    CHECK_FALSE(std::isfinite(series[1].values[0]));
    CHECK(series[1].values[2] == Catch::Approx(2.0));
    CHECK(series[0].values[2] == Catch::Approx(series[1].values[2] + 2.0 * std::sqrt(2.0 / 3.0)));
    CHECK(series[2].values[2] == Catch::Approx(series[1].values[2] - 2.0 * std::sqrt(2.0 / 3.0)));

    const std::vector<terminal::Bar> empty;
    const auto blank = terminal::computeStudies(empty, studies);
    REQUIRE(blank.size() == 3);
    CHECK(blank[0].values.empty());
    CHECK(blank[0].label == "BB 3 2 C U");
}

TEST_CASE("bollinger traces keep their own color and line style")
{
    terminal::CStudyInstance inst = makeBollingerInstance(4, 3, 2);
    inst.outputs = {
        {.color = terminal::kStudyPalette[0], .line = terminal::StudyLineStyle::Dashed},
        {.color = terminal::kStudyPalette[1], .line = terminal::StudyLineStyle::Dotted},
        {.color = terminal::kStudyPalette[2], .line = terminal::StudyLineStyle::Solid},
    };
    const auto bars = bollingerCloses({1.0, 2.0, 3.0});
    const std::vector<terminal::CStudyInstance> studies{inst};
    const auto series = terminal::computeStudies(bars, studies);
    REQUIRE(series.size() == 3);
    CHECK(series[0].color == terminal::kStudyPalette[0]);
    CHECK(series[1].color == terminal::kStudyPalette[1]);
    CHECK(series[2].color == terminal::kStudyPalette[2]);
    CHECK(series[0].line == terminal::StudyLineStyle::Dashed);
    CHECK(series[1].line == terminal::StudyLineStyle::Dotted);
    CHECK(series[2].line == terminal::StudyLineStyle::Solid);
    CHECK(series[0].label == "BB 3 2 C U");
    CHECK(series[1].label == "BB 3 2 C M");
    CHECK(series[2].label == "BB 3 2 C L");
}

TEST_CASE("normalize copies one color onto every bollinger output")
{
    terminal::CStudyInstance inst = makeBollingerInstance(1, 20, 2);
    inst.color = terminal::kStudyPalette[1];
    terminal::normalizeStudyOutputs(inst);
    REQUIRE(inst.outputs.size() == 3);
    CHECK(inst.outputs[0].color == terminal::kStudyPalette[1]);
    CHECK(inst.outputs[1].color == terminal::kStudyPalette[1]);
    CHECK(inst.outputs[2].color == terminal::kStudyPalette[1]);
    CHECK(inst.outputs[0].line == terminal::StudyLineStyle::Solid);
    CHECK(inst.outputs[2].line == terminal::StudyLineStyle::Solid);
    CHECK(inst.color == terminal::kStudyPalette[1]);

    inst.outputs[0].color = terminal::kStudyPalette[3];
    inst.outputs[0].line = terminal::StudyLineStyle::Dashed;
    inst.outputs[2].line = terminal::StudyLineStyle::Dotted;
    terminal::normalizeStudyOutputs(inst);
    CHECK(inst.outputs[0].color == terminal::kStudyPalette[3]);
    CHECK(inst.outputs[0].line == terminal::StudyLineStyle::Dashed);
    CHECK(inst.outputs[1].color == terminal::kStudyPalette[1]);
    CHECK(inst.outputs[2].line == terminal::StudyLineStyle::Dotted);
    CHECK(inst.color == terminal::kStudyPalette[3]);
}

TEST_CASE("a new bollinger study gives each band its own color")
{
    terminal::CStudyInstance inst;
    inst.type_id = "bollinger";
    terminal::assignStudyOutputDefaults(inst, 0);
    REQUIRE(inst.outputs.size() == 3);
    CHECK(inst.outputs[0].color == terminal::studyPaletteColor(0));
    CHECK(inst.outputs[1].color == terminal::studyPaletteColor(1));
    CHECK(inst.outputs[2].color == terminal::studyPaletteColor(2));
    CHECK(inst.outputs[0].line == terminal::StudyLineStyle::Solid);
    CHECK(inst.outputs[1].line == terminal::StudyLineStyle::Solid);
    CHECK(inst.outputs[2].line == terminal::StudyLineStyle::Solid);
    CHECK(inst.color == inst.outputs[0].color);
}
