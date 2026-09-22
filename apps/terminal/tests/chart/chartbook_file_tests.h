// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartbookFile.h"
#include "chart/CStudy.h"

#include <filesystem>
#include <fstream>

namespace {

[[nodiscard]] terminal::CChartbookDocument sampleChartbook()
{
    using terminal::ChartBarPeriod;
    using terminal::ChartInteractiveScale;
    using terminal::ChartScaleRange;
    using terminal::ChartbookFloating;
    using terminal::ChartbookPane;
    using terminal::CStudyInstance;
    using terminal::MovingAverageParams;
    using terminal::StudyKind;
    using terminal::StudySource;
    using terminal::VolumeParams;
    using terminal::chartbookLeaf;
    using terminal::chartbookSplit;
    using terminal::paneWindowId;

    terminal::CChartbookDocument document;
    document.name = "equities";
    document.focused_pane = 1;
    document.next_pane_id = 3;
    document.data.symbol = "AAPL";
    document.data.from = "20260907";
    document.data.to = "20260921";
    document.data.ingest_timeframe = "1m";
    document.data.selected_symbol = "AAPL";
    document.data.selected_timeframe = "1m";
    document.data.sort_column = "symbol";
    document.data.sort_descending = false;
    document.data.columns.push_back({"symbol", 80.f, true, 0});
    document.data.columns.push_back({"exch", 48.f, false, 1});

    document.layout = chartbookSplit(terminal::ChartbookSplitAxis::Horizontal, 0.30f,
                                     chartbookLeaf({"data"}, "data"),
                                     chartbookLeaf({paneWindowId(1)}, paneWindowId(1)));

    ChartbookFloating floating;
    floating.window = paneWindowId(2);
    floating.x = 12.f;
    floating.y = 24.f;
    floating.w = 640.f;
    floating.h = 480.f;
    document.floating.push_back(floating);

    ChartbookPane first;
    first.id = 1;
    first.settings.symbol = "AAPL";
    first.settings.period = ChartBarPeriod::Minute5;
    first.settings.scale_range = ChartScaleRange::ConstantRange;
    first.settings.constant_range = 4.5;
    first.settings.bar_spacing_px = 9.f;
    first.interactive = ChartInteractiveScale::Range;
    first.region_ratios = {0.72f, 0.28f};
    first.next_study_id = 3;
    CStudyInstance moving;
    moving.id = 1;
    moving.kind = StudyKind::MovingAverage;
    moving.color = terminal::kStudyDefaultColor;
    moving.chart_region = 1;
    MovingAverageParams params;
    params.source = StudySource::Volume;
    params.length = 20;
    moving.params = params;
    CStudyInstance volume;
    volume.id = 2;
    volume.kind = StudyKind::Volume;
    volume.color = terminal::kStudyPalette[3];
    volume.chart_region = 2;
    volume.params = VolumeParams{};
    first.studies.push_back(std::move(moving));
    first.studies.push_back(std::move(volume));

    ChartbookPane second;
    second.id = 2;
    second.settings.symbol = "MSFT";
    second.settings.period = ChartBarPeriod::Day1;

    document.panes.push_back(std::move(first));
    document.panes.push_back(std::move(second));
    return document;
}

}  // namespace

TEST_CASE("chartbook json round trip keeps panes, studies, and layout")
{
    const terminal::CChartbookDocument original = sampleChartbook();
    const terminal::ChartbookLoadResult loaded = terminal::chartbookFromJson(terminal::chartbookToJson(original));
    REQUIRE(loaded.ok);
    CHECK(loaded.document.name == "equities");
    CHECK(loaded.document.focused_pane == 1);
    CHECK(loaded.document.next_pane_id == 3);
    CHECK(loaded.document.data.symbol == "AAPL");
    CHECK(loaded.document.data.selected_timeframe == "1m");
    CHECK(loaded.document.data.sort_column == "symbol");
    CHECK(loaded.document.data.columns.size() == 2);
    CHECK(loaded.document.data.columns[1].visible == false);
    REQUIRE(loaded.document.layout.root >= 0);
    const terminal::ChartbookLayoutNode& root =
        loaded.document.layout.nodes[static_cast<std::size_t>(loaded.document.layout.root)];
    CHECK(root.is_split);
    CHECK(root.ratio == Catch::Approx(0.30f));
    CHECK(loaded.document.floating.size() == 1);
    CHECK(loaded.document.floating[0].window == "pane:2");
    CHECK(loaded.document.floating[0].w == Catch::Approx(640.f));
    REQUIRE(loaded.document.panes.size() == 2);
    CHECK(loaded.document.panes[0].settings.symbol == "AAPL");
    CHECK(loaded.document.panes[0].settings.period == terminal::ChartBarPeriod::Minute5);
    CHECK(loaded.document.panes[0].interactive == terminal::ChartInteractiveScale::Range);
    CHECK(loaded.document.panes[0].region_ratios.size() == 2);
    REQUIRE(loaded.document.panes[0].studies.size() == 2);
    const auto* moving = std::get_if<terminal::MovingAverageParams>(&loaded.document.panes[0].studies[0].params);
    REQUIRE(moving != nullptr);
    CHECK(moving->source == terminal::StudySource::Volume);
    CHECK(moving->length == 20);
    CHECK(loaded.document.panes[0].studies[1].kind == terminal::StudyKind::Volume);
    CHECK(loaded.document.panes[1].settings.period == terminal::ChartBarPeriod::Day1);
    CHECK(terminal::chartbookPaneIsOpen(loaded.document, 1));
    CHECK(terminal::chartbookPaneIsOpen(loaded.document, 2));
    CHECK_FALSE(terminal::chartbookPaneIsOpen(loaded.document, 9));
}

TEST_CASE("chartbook file rejects a bad format, an unknown period, and an unknown study")
{
    const terminal::ChartbookLoadResult format = terminal::chartbookFromJson(R"({"format":2})");
    CHECK_FALSE(format.ok);
    CHECK(format.document.panes.empty());

    const char* unknown_period = R"({
        "format": 1,
        "name": "bad",
        "focused_pane": 1,
        "next_pane_id": 2,
        "data": {},
        "layout": {"windows": ["pane:1"], "selected": "pane:1"},
        "panes": [{
            "id": 1,
            "settings": {
                "symbol": "AAPL", "period": "2m", "bar_type": "candlestick",
                "limit_mode": "session_count", "intraday_session_count": 14,
                "historical_session_count": 1260, "scale_range": "automatic",
                "constant_range": 0, "user_top": 0, "user_bottom": 0,
                "bar_spacing_px": 8, "bar_width_frac": 0.6, "scale_padding_pct": 4
            },
            "interactive_scale": "move",
            "next_study_id": 1,
            "studies": []
        }]
    })";
    const terminal::ChartbookLoadResult period = terminal::chartbookFromJson(unknown_period);
    CHECK_FALSE(period.ok);
    CHECK(period.document.panes.empty());

    const char* unknown_study = R"({
        "format": 1,
        "name": "studies",
        "focused_pane": 1,
        "next_pane_id": 2,
        "data": {"columns": [{"id": "nope", "width": 10, "visible": true, "order": 0},
                             {"id": "symbol", "width": 40, "visible": true, "order": 1}]},
        "notes": "ignored",
        "layout": {"windows": ["pane:1"], "selected": "pane:1"},
        "panes": [{
            "id": 1,
            "settings": {
                "symbol": "", "period": "1m", "bar_type": "candlestick",
                "limit_mode": "session_count", "intraday_session_count": 14,
                "historical_session_count": 1260, "scale_range": "automatic",
                "constant_range": 0, "user_top": 0, "user_bottom": 0,
                "bar_spacing_px": 8, "bar_width_frac": 0.6, "scale_padding_pct": 4
            },
            "interactive_scale": "move",
            "next_study_id": 2,
            "studies": [
                {"id": 9, "kind": "rsi", "enabled": true, "color": 1, "chart_region": 1},
                {"id": 1, "kind": "moving_average", "enabled": true, "color": 1,
                 "chart_region": 1, "source": "close", "length": 10, "method": "simple"}
            ]
        }]
    })";
    const terminal::ChartbookLoadResult studies = terminal::chartbookFromJson(unknown_study);
    REQUIRE(studies.ok);
    CHECK(studies.document.panes[0].studies.size() == 1);
    CHECK(studies.document.data.columns.size() == 1);
    CHECK(studies.document.data.columns[0].id == "symbol");
}

TEST_CASE("chartbook names stay inside the chartbooks directory")
{
    CHECK(terminal::chartbookPathForStem("../equities").empty());
    CHECK(terminal::chartbookPathForStem("a/b").empty());
    CHECK(terminal::chartbookPathForStem("a:b").empty());
    CHECK(terminal::chartbookPathForStem(".").empty());
    CHECK(terminal::chartbookPathForStem("").empty());
    CHECK(terminal::chartbookPathForStem("*").empty());

    const std::filesystem::path path = terminal::chartbookPathForStem("equities");
    CHECK(path.filename() == "equities.chartbook.json");
    CHECK(path.parent_path().filename() == "chartbooks");
    CHECK(path.generic_string().find("data/chartbooks/") != std::string::npos);
    CHECK(terminal::chartbookPathsEqual(path, terminal::chartbookPathForStem("equities.chartbook.json")));
}

TEST_CASE("startup list keeps order and reports a missing chartbook")
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "terminal-chartbook-tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::filesystem::path first = directory / "first.chartbook.json";
    const std::filesystem::path missing = directory / "missing.chartbook.json";
    const std::filesystem::path third = directory / "third.chartbook.json";
    const terminal::CChartbookDocument document = terminal::makeDefaultChartbook("first");
    REQUIRE(terminal::saveChartbook(first, document).empty());
    terminal::CChartbookDocument other = document;
    other.name = "third";
    REQUIRE(terminal::saveChartbook(third, other).empty());
    CHECK(std::filesystem::exists(first.string() + ".bak") == false);

    REQUIRE(terminal::saveChartbook(first, document).empty());
    CHECK(std::filesystem::exists(first.string() + ".bak"));

    const terminal::ChartbookLoadResult round_trip = terminal::loadChartbook(first);
    REQUIRE(round_trip.ok);
    CHECK(round_trip.document.name == "first");
    CHECK(round_trip.document.next_pane_id == 2);
    CHECK(terminal::chartbookPaneIsOpen(round_trip.document, 1));

    terminal::StartupSettings settings;
    settings.open_on_startup = {first.string(), missing.string(), third.string()};
    const std::filesystem::path startup = directory / "terminal.json";
    REQUIRE(terminal::saveStartupSettings(startup, settings).empty());
    const terminal::StartupLoadResult loaded = terminal::loadStartupSettings(startup);
    REQUIRE(loaded.ok);
    REQUIRE(loaded.settings.open_on_startup.size() == 3);
    CHECK(loaded.settings.open_on_startup[0] == first.string());
    CHECK(loaded.settings.open_on_startup[2] == third.string());

    const terminal::StartupOpenResult opened = terminal::openStartupChartbooks(loaded.settings);
    REQUIRE(opened.books.size() == 2);
    CHECK(opened.errors.size() == 1);
    CHECK(opened.books[0].document.name == "first");
    CHECK(opened.books[1].document.name == "third");
    CHECK(terminal::chartbookPathsEqual(opened.books[0].path, first));

    std::filesystem::remove_all(directory);
}

TEST_CASE("a new pane docks beside DATA and a closed pane is not open")
{
    terminal::ChartbookLayout layout = terminal::makeDefaultChartbook("chartbook1").layout;
    terminal::chartbookInsertPane(layout, 2);
    terminal::CChartbookDocument document;
    document.layout = layout;
    CHECK(terminal::chartbookWindowReferenced(document, "pane:2"));
    CHECK(terminal::chartbookWindowReferenced(document, "data"));

    terminal::ChartbookLayout charts_only = terminal::chartbookLeaf({"pane:1"}, "pane:1");
    terminal::chartbookInsertData(charts_only);
    document.layout = charts_only;
    CHECK(terminal::chartbookWindowReferenced(document, "data"));
    CHECK(terminal::chartbookWindowReferenced(document, "pane:1"));

    terminal::ChartbookLayout with_data = terminal::makeDefaultChartbook("chartbook1").layout;
    terminal::chartbookRemoveWindow(with_data, "data");
    document.layout = with_data;
    document.floating.clear();
    CHECK_FALSE(terminal::chartbookWindowReferenced(document, "data"));
    CHECK(terminal::chartbookWindowReferenced(document, "pane:1"));
}
