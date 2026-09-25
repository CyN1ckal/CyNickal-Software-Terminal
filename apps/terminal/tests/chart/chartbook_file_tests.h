// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartbookFile.h"
#include "chart/CStudy.h"
#include "chart/studies/CBollinger.h"
#include "chart/studies/CMovingAverage.h"
#include "chart/studies/CNBarPercentChange.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>

namespace {

[[nodiscard]] terminal::CChartbookDocument sampleChartbook()
{
    using terminal::ChartBarPeriod;
    using terminal::ChartInteractiveScale;
    using terminal::ChartScaleRange;
    using terminal::ChartbookFloating;
    using terminal::ChartbookPane;
    using terminal::CMovingAverage;
    using terminal::CStudyInstance;
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
    document.data.columns.push_back({"figi", 96.f, false, 1});

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
    moving.type_id = "moving_average";
    moving.color = terminal::kStudyDefaultColor;
    moving.chart_region = 1;
    moving.options = {20, static_cast<int>(CMovingAverage::Source::Volume), 0};
    CStudyInstance volume;
    volume.id = 2;
    volume.type_id = "volume";
    volume.color = terminal::kStudyPalette[2];
    volume.chart_region = 2;
    volume.outputs = {
        {.color = terminal::kStudyPalette[2], .line = terminal::StudyLineStyle::Solid},
        {.color = terminal::kStudyPalette[3], .line = terminal::StudyLineStyle::Solid},
    };
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
    const terminal::CStudyInstance& moving = loaded.document.panes[0].studies[0];
    REQUIRE(moving.options.size() == 3);
    CHECK(moving.options[0] == 20);
    CHECK(moving.options[1] == static_cast<int>(terminal::CMovingAverage::Source::Volume));
    CHECK(moving.options[2] == 0);
    REQUIRE(moving.outputs.size() == 1);
    CHECK(moving.outputs[0].color == terminal::kStudyDefaultColor);
    CHECK(moving.outputs[0].line == terminal::StudyLineStyle::Solid);
    CHECK(moving.color == terminal::kStudyDefaultColor);
    const terminal::CStudyInstance& volume = loaded.document.panes[0].studies[1];
    CHECK(volume.type_id == "volume");
    REQUIRE(volume.outputs.size() == 2);
    CHECK(volume.outputs[0].color == terminal::kStudyPalette[2]);
    CHECK(volume.outputs[1].color == terminal::kStudyPalette[3]);
    CHECK(volume.outputs[0].line == terminal::StudyLineStyle::Solid);
    CHECK(volume.color == terminal::kStudyPalette[2]);
    CHECK(loaded.document.panes[1].settings.period == terminal::ChartBarPeriod::Day1);
    CHECK(terminal::chartbookPaneIsOpen(loaded.document, 1));
    CHECK(terminal::chartbookPaneIsOpen(loaded.document, 2));
    CHECK_FALSE(terminal::chartbookPaneIsOpen(loaded.document, 9));
    CHECK(loaded.document.focused_financials == 0);
    CHECK(loaded.document.next_financials_id == 1);
    CHECK(loaded.document.financials.empty());
}

TEST_CASE("chartbook figi keys are optional and round trip when set")
{
    terminal::CChartbookDocument document = sampleChartbook();
    const std::string plain = terminal::chartbookToJson(document);
    CHECK(plain.find("\"figi\":") == std::string::npos);
    const terminal::ChartbookLoadResult unpinned = terminal::chartbookFromJson(plain);
    REQUIRE(unpinned.ok);
    CHECK(unpinned.document.panes[0].settings.figi.empty());

    document.panes[0].settings.figi = "BBG000B9XRY4";
    terminal::ChartbookFinancials financials;
    financials.id = 1;
    financials.symbol = "META";
    financials.figi = "BBG000MM2P62";
    document.financials.push_back(financials);
    document.next_financials_id = 2;
    terminal::ChartbookOptions options;
    options.id = 1;
    options.symbol = "$SPX";
    options.figi = "BBG000H4FSM0";
    document.options.push_back(options);
    document.next_options_id = 2;
    const terminal::ChartbookLoadResult pinned = terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(pinned.ok);
    CHECK(pinned.document.panes[0].settings.figi == "BBG000B9XRY4");
    CHECK(pinned.document.panes[1].settings.figi.empty());
    REQUIRE(pinned.document.financials.size() == 1);
    CHECK(pinned.document.financials[0].figi == "BBG000MM2P62");
    REQUIRE(pinned.document.options.size() == 1);
    CHECK(pinned.document.options[0].figi == "BBG000H4FSM0");
}

TEST_CASE("a book saved with the exch column opens with the figi column")
{
    terminal::CChartbookDocument document = sampleChartbook();
    document.data.columns[1].id = "exch";
    document.data.sort_column = "exch";
    const terminal::ChartbookLoadResult loaded = terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.data.columns.size() == 2);
    CHECK(loaded.document.data.columns[1].id == "figi");
    CHECK(loaded.document.data.sort_column == "figi");
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
    const terminal::CStudyInstance& legacy = studies.document.panes[0].studies[0];
    REQUIRE(legacy.outputs.size() == 1);
    CHECK(legacy.outputs[0].color == 1);
    CHECK(legacy.outputs[0].line == terminal::StudyLineStyle::Solid);
    CHECK(legacy.color == 1);
}

TEST_CASE("chartbook studies keep per-output color and line style")
{
    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("bands");
    terminal::CStudyInstance bands;
    bands.id = 1;
    bands.type_id = "bollinger";
    bands.chart_region = 1;
    bands.options = {20, static_cast<int>(terminal::CBollinger::Source::Close), 2};
    bands.outputs = {
        {.color = terminal::kStudyPalette[3], .line = terminal::StudyLineStyle::Dashed},
        {.color = terminal::kStudyPalette[0], .line = terminal::StudyLineStyle::Solid},
        {.color = terminal::kStudyPalette[1], .line = terminal::StudyLineStyle::Dotted},
    };
    bands.color = bands.outputs[0].color;
    document.panes[0].next_study_id = 2;
    document.panes[0].studies.push_back(std::move(bands));

    const terminal::ChartbookLoadResult loaded =
        terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.panes[0].studies.size() == 1);
    const terminal::CStudyInstance& study = loaded.document.panes[0].studies[0];
    REQUIRE(study.outputs.size() == 3);
    CHECK(study.color == terminal::kStudyPalette[3]);
    CHECK(study.outputs[0].color == terminal::kStudyPalette[3]);
    CHECK(study.outputs[0].line == terminal::StudyLineStyle::Dashed);
    CHECK(study.outputs[1].color == terminal::kStudyPalette[0]);
    CHECK(study.outputs[1].line == terminal::StudyLineStyle::Solid);
    CHECK(study.outputs[2].color == terminal::kStudyPalette[1]);
    CHECK(study.outputs[2].line == terminal::StudyLineStyle::Dotted);
    REQUIRE(study.options.size() == 3);
    CHECK(study.options[0] == 20);
    CHECK(study.options[2] == 2);
}

TEST_CASE("chartbook keeps a value label style")
{
    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("label");
    terminal::CStudyInstance study;
    study.id = 1;
    study.type_id = "n_bar_percent_change";
    study.chart_region = 2;
    study.options = {1, static_cast<int>(terminal::CNBarPercentChange::Source::Close)};
    study.outputs = {
        {.color = terminal::kStudyPalette[1], .line = terminal::StudyLineStyle::Value},
    };
    study.color = study.outputs[0].color;
    document.panes[0].next_study_id = 2;
    document.panes[0].studies.push_back(std::move(study));

    const terminal::ChartbookLoadResult loaded =
        terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.panes[0].studies.size() == 1);
    const terminal::CStudyInstance& saved = loaded.document.panes[0].studies[0];
    REQUIRE(saved.outputs.size() == 1);
    CHECK(saved.outputs[0].line == terminal::StudyLineStyle::Value);
    CHECK(saved.outputs[0].color == terminal::kStudyPalette[1]);
    CHECK(saved.type_id == "n_bar_percent_change");
}

TEST_CASE("chartbook rejects an unknown study line style")
{
    const char* text = R"({
        "format": 1,
        "name": "lines",
        "focused_pane": 1,
        "next_pane_id": 2,
        "data": {},
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
            "studies": [{
                "id": 1, "kind": "moving_average", "enabled": true, "color": 1,
                "chart_region": 1, "source": "close", "length": 10, "method": "simple",
                "outputs": [{"key": "average", "color": 1, "line": "wavy"}]
            }]
        }]
    })";
    const terminal::ChartbookLoadResult loaded = terminal::chartbookFromJson(text);
    CHECK_FALSE(loaded.ok);
    CHECK(loaded.document.panes.empty());
}

TEST_CASE("legacy volume color keeps the candle up and down colors")
{
    const char* text = R"({
        "format": 1,
        "name": "volume",
        "focused_pane": 1,
        "next_pane_id": 2,
        "data": {},
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
            "studies": [{
                "id": 1, "kind": "volume", "enabled": true, "color": 1,
                "chart_region": 2,
                "outputs": [{"key": "volume", "color": 1}]
            }]
        }]
    })";
    const terminal::ChartbookLoadResult legacy = terminal::chartbookFromJson(text);
    REQUIRE(legacy.ok);
    REQUIRE(legacy.document.panes[0].studies.size() == 1);
    const terminal::CStudyInstance& volume = legacy.document.panes[0].studies[0];
    REQUIRE(volume.outputs.size() == 2);
    CHECK(volume.outputs[0].color == terminal::kStudyPalette[2]);
    CHECK(volume.outputs[1].color == terminal::kStudyPalette[3]);
    CHECK(volume.color == terminal::kStudyPalette[2]);

    const char* bare = R"({
        "format": 1,
        "name": "volume",
        "focused_pane": 1,
        "next_pane_id": 2,
        "data": {},
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
            "studies": [{
                "id": 1, "kind": "volume", "enabled": true, "color": 1, "chart_region": 2
            }]
        }]
    })";
    const terminal::ChartbookLoadResult older = terminal::chartbookFromJson(bare);
    REQUIRE(older.ok);
    REQUIRE(older.document.panes[0].studies[0].outputs.size() == 2);
    CHECK(older.document.panes[0].studies[0].outputs[0].color == terminal::kStudyPalette[2]);
    CHECK(older.document.panes[0].studies[0].outputs[1].color == terminal::kStudyPalette[3]);
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

TEST_CASE("financials panels dock with the charts and round trip as a collection")
{
    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("chartbook1");
    terminal::chartbookInsertFinancials(document.layout, 1);
    terminal::chartbookInsertFinancials(document.layout, 2);
    CHECK(terminal::chartbookFinancialsIsOpen(document, 1));
    CHECK(terminal::chartbookFinancialsIsOpen(document, 2));
    CHECK_FALSE(terminal::chartbookFinancialsIsOpen(document, 9));
    const terminal::ChartbookLayoutNode& root =
        document.layout.nodes[static_cast<std::size_t>(document.layout.root)];
    REQUIRE(root.is_split);
    const terminal::ChartbookLayoutNode& chart =
        document.layout.nodes[static_cast<std::size_t>(root.second)];
    CHECK_FALSE(chart.is_split);
    CHECK(chart.selected == terminal::financialsWindowId(2));
    CHECK(std::ranges::find(chart.windows, terminal::financialsWindowId(1)) != chart.windows.end());

    terminal::ChartbookFinancials income;
    income.id = 1;
    income.symbol = "AAPL";
    income.statement = "cashflow";
    income.timeframe = "quarterly";
    terminal::ChartbookFinancials balance;
    balance.id = 2;
    balance.symbol = "MSFT";
    balance.statement = "balance";
    balance.timeframe = "annually";
    document.financials.push_back(std::move(income));
    document.financials.push_back(std::move(balance));
    document.focused_financials = 2;
    document.next_financials_id = 3;
    const terminal::ChartbookLoadResult loaded =
        terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.financials.size() == 2);
    CHECK(loaded.document.focused_financials == 2);
    CHECK(loaded.document.next_financials_id == 3);
    CHECK(loaded.document.financials[0].id == 1);
    CHECK(loaded.document.financials[0].symbol == "AAPL");
    CHECK(loaded.document.financials[0].statement == "cashflow");
    CHECK(loaded.document.financials[0].timeframe == "quarterly");
    CHECK(loaded.document.financials[1].symbol == "MSFT");
    CHECK(loaded.document.financials[1].statement == "balance");
    CHECK(terminal::chartbookFinancialsIsOpen(loaded.document, 1));
    CHECK(terminal::chartbookFinancialsIsOpen(loaded.document, 2));
    CHECK(loaded.document.options.empty());
    CHECK(loaded.document.next_options_id == 1);
    CHECK(loaded.document.focused_options == 0);
    CHECK(loaded.document.portfolios.empty());
    CHECK(loaded.document.next_portfolio_id == 1);
    CHECK(loaded.document.focused_portfolio == 0);

    const char* legacy = R"({
        "format": 1,
        "name": "old",
        "focused_pane": 0,
        "next_pane_id": 1,
        "data": {},
        "financials": {"symbol": "META", "statement": "balance", "timeframe": "annually"},
        "layout": {
            "split": "horizontal",
            "ratio": 0.2,
            "first": {"windows": ["data"], "selected": "data"},
            "second": {"windows": ["financials"], "selected": "financials"}
        },
        "panes": []
    })";
    const terminal::ChartbookLoadResult migrated = terminal::chartbookFromJson(legacy);
    REQUIRE(migrated.ok);
    REQUIRE(migrated.document.financials.size() == 1);
    CHECK(migrated.document.financials[0].id == 1);
    CHECK(migrated.document.financials[0].symbol == "META");
    CHECK(migrated.document.financials[0].statement == "balance");
    CHECK(migrated.document.financials[0].timeframe == "annually");
    CHECK(migrated.document.next_financials_id == 2);
    CHECK(terminal::chartbookFinancialsIsOpen(migrated.document, 1));
    CHECK_FALSE(terminal::chartbookWindowReferenced(migrated.document, "financials"));

    const char* bad_period = R"({
        "format": 1,
        "name": "bad",
        "focused_pane": 0,
        "next_pane_id": 1,
        "data": {},
        "financials": {"symbol": "AAPL", "statement": "income", "timeframe": "trailing"},
        "layout": {"windows": ["financials"], "selected": "financials"},
        "panes": []
    })";
    CHECK_FALSE(terminal::chartbookFromJson(bad_period).ok);

    const char* bad_id = R"({
        "format": 1,
        "name": "bad",
        "focused_pane": 0,
        "next_pane_id": 1,
        "next_financials_id": 2,
        "data": {},
        "financials": [
            {"id": 1, "symbol": "AAPL", "statement": "income", "timeframe": "annually"},
            {"id": 1, "symbol": "MSFT", "statement": "balance", "timeframe": "quarterly"}
        ],
        "layout": {"windows": ["financials:1"], "selected": "financials:1"},
        "panes": []
    })";
    CHECK_FALSE(terminal::chartbookFromJson(bad_id).ok);

    const char* missing_panel = R"({
        "format": 1,
        "name": "bad",
        "focused_pane": 0,
        "next_pane_id": 1,
        "next_financials_id": 1,
        "data": {},
        "financials": [],
        "layout": {"windows": ["financials:1"], "selected": "financials:1"},
        "panes": []
    })";
    CHECK_FALSE(terminal::chartbookFromJson(missing_panel).ok);
}

TEST_CASE("options chains dock and round trip beside the charts")
{
    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("chartbook1");
    terminal::chartbookInsertOptions(document.layout, 1);
    CHECK(terminal::chartbookOptionsIsOpen(document, 1));
    terminal::ChartbookOptions chain;
    chain.id = 1;
    chain.symbol = "$SPX";
    chain.expiration = 20261016;
    chain.expiration_type = "monthly";
    document.options.push_back(std::move(chain));
    document.focused_options = 1;
    document.next_options_id = 2;
    const terminal::ChartbookLoadResult loaded =
        terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.options.size() == 1);
    CHECK(loaded.document.options[0].symbol == "$SPX");
    CHECK(loaded.document.options[0].expiration == 20261016);
    CHECK(loaded.document.options[0].expiration_type == "monthly");
    CHECK(terminal::optionChainColumnsAreDefault(loaded.document.options[0].columns));
    CHECK(loaded.document.focused_options == 1);
    CHECK(loaded.document.next_options_id == 2);
    CHECK(terminal::chartbookOptionsIsOpen(loaded.document, 1));

    const char* missing = R"({
        "format": 1,
        "name": "bad",
        "focused_pane": 0,
        "next_pane_id": 1,
        "next_options_id": 2,
        "data": {},
        "options": [{"id": 1, "symbol": "AAPL", "expiration": 20260925, "expiration_type": "weekly"}],
        "layout": {"windows": ["options:9"], "selected": "options:9"},
        "panes": []
    })";
    CHECK_FALSE(terminal::chartbookFromJson(missing).ok);
}

TEST_CASE("option chain columns select, persist, and default when omitted")
{
    using nlohmann::json;

    std::vector<std::string> columns = terminal::defaultOptionChainColumns();
    CHECK(terminal::optionChainColumnsAreDefault(columns));
    CHECK(terminal::setOptionChainColumnVisible(columns, "iv", false));
    const std::vector<std::string> without_iv{"bid", "ask", "last", "delta", "vol", "oi"};
    CHECK(columns == without_iv);
    CHECK(terminal::setOptionChainColumnVisible(columns, "theta", true));
    const std::vector<std::string> with_theta{"bid", "ask", "last", "delta", "theta", "vol", "oi"};
    CHECK(columns == with_theta);
    CHECK_FALSE(terminal::setOptionChainColumnVisible(columns, "theta", true));
    CHECK_FALSE(terminal::setOptionChainColumnVisible(columns, "nope", false));
    CHECK(terminal::setOptionChainColumnVisible(columns, "bid", false));
    CHECK(terminal::setOptionChainColumnVisible(columns, "ask", false));
    CHECK(terminal::setOptionChainColumnVisible(columns, "last", false));
    CHECK(terminal::setOptionChainColumnVisible(columns, "delta", false));
    CHECK(terminal::setOptionChainColumnVisible(columns, "theta", false));
    CHECK(terminal::setOptionChainColumnVisible(columns, "vol", false));
    CHECK(terminal::setOptionChainColumnVisible(columns, "oi", false));
    CHECK(columns.empty());

    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("chartbook1");
    terminal::chartbookInsertOptions(document.layout, 1);
    terminal::ChartbookOptions chain;
    chain.id = 1;
    chain.symbol = "AAPL";
    chain.expiration = 20261016;
    chain.expiration_type = "monthly";
    chain.columns = {"oi", "bid", "theta"};
    document.options.push_back(chain);
    document.focused_options = 1;
    document.next_options_id = 2;

    const json written = json::parse(terminal::chartbookToJson(document));
    const json saved_order = json::array({"oi", "bid", "theta"});
    CHECK(written["options"][0]["columns"] == saved_order);

    const terminal::ChartbookLoadResult loaded = terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.options.size() == 1);
    const std::vector<std::string> catalog_order{"bid", "theta", "oi"};
    CHECK(loaded.document.options[0].columns == catalog_order);

    document.options[0].columns.clear();
    const json strike_only = json::parse(terminal::chartbookToJson(document));
    CHECK(strike_only["options"][0]["columns"].is_array());
    CHECK(strike_only["options"][0]["columns"].empty());
    const terminal::ChartbookLoadResult bare = terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(bare.ok);
    CHECK(bare.document.options[0].columns.empty());

    terminal::ChartbookOptions stock;
    stock.id = 1;
    stock.symbol = "AAPL";
    document.options[0] = stock;
    const json omitted = json::parse(terminal::chartbookToJson(document));
    CHECK_FALSE(omitted["options"][0].contains("columns"));
    const terminal::ChartbookLoadResult defaults = terminal::chartbookFromJson(omitted.dump());
    REQUIRE(defaults.ok);
    CHECK(terminal::optionChainColumnsAreDefault(defaults.document.options[0].columns));

    json unknown = omitted;
    unknown["options"][0]["columns"] = json::array({"bid", "gamma"});
    const terminal::ChartbookLoadResult bad_name = terminal::chartbookFromJson(unknown.dump());
    CHECK_FALSE(bad_name.ok);
    CHECK(bad_name.error == "unknown option column");

    json duplicate = omitted;
    duplicate["options"][0]["columns"] = json::array({"bid", "bid"});
    const terminal::ChartbookLoadResult bad_dup = terminal::chartbookFromJson(duplicate.dump());
    CHECK_FALSE(bad_dup.ok);
    CHECK(bad_dup.error == "duplicate option column");

    json not_array = omitted;
    not_array["options"][0]["columns"] = "bid";
    const terminal::ChartbookLoadResult bad_type = terminal::chartbookFromJson(not_array.dump());
    CHECK_FALSE(bad_type.ok);
    CHECK(bad_type.error == "option columns is not an array");

    json not_string = omitted;
    not_string["options"][0]["columns"] = json::array({1});
    const terminal::ChartbookLoadResult bad_item = terminal::chartbookFromJson(not_string.dump());
    CHECK_FALSE(bad_item.ok);
    CHECK(bad_item.error == "option column is not a string");
}

TEST_CASE("portfolio windows round trip and a book without them still opens")
{
    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("chartbook1");
    terminal::chartbookInsertPortfolio(document.layout, 1);
    CHECK(terminal::chartbookPortfolioIsOpen(document, 1));
    terminal::ChartbookPortfolio panel;
    panel.id = 1;
    panel.portfolio_id = 4;
    document.portfolios.push_back(panel);
    document.focused_portfolio = 1;
    document.next_portfolio_id = 2;
    const terminal::ChartbookLoadResult loaded =
        terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.portfolios.size() == 1);
    CHECK(loaded.document.portfolios[0].id == 1);
    CHECK(loaded.document.portfolios[0].portfolio_id == 4);
    CHECK(loaded.document.focused_portfolio == 1);
    CHECK(loaded.document.next_portfolio_id == 2);
    CHECK(terminal::chartbookPortfolioIsOpen(loaded.document, 1));

    const char* legacy = R"({
        "format": 1,
        "name": "old",
        "focused_pane": 0,
        "next_pane_id": 1,
        "data": {},
        "layout": {"windows": ["data"], "selected": "data"},
        "panes": []
    })";
    const terminal::ChartbookLoadResult old = terminal::chartbookFromJson(legacy);
    REQUIRE(old.ok);
    CHECK(old.document.portfolios.empty());

    const char* missing = R"({
        "format": 1,
        "name": "bad",
        "focused_pane": 0,
        "next_pane_id": 1,
        "next_portfolio_id": 2,
        "data": {},
        "portfolios": [{"id": 1, "book": 3}],
        "layout": {"windows": ["portfolio:9"], "selected": "portfolio:9"},
        "panes": []
    })";
    CHECK_FALSE(terminal::chartbookFromJson(missing).ok);
}

TEST_CASE("portfolio risk view round trips and a book without it keeps the defaults")
{
    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("chartbook1");
    terminal::chartbookInsertPortfolio(document.layout, 1);
    terminal::ChartbookPortfolio panel;
    panel.id = 1;
    panel.portfolio_id = 4;
    panel.var_confidence_pct = 99;
    panel.show_position_var = false;
    panel.show_unit_var = true;
    document.portfolios.push_back(panel);
    document.focused_portfolio = 1;
    document.next_portfolio_id = 2;
    const terminal::ChartbookLoadResult loaded =
        terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.portfolios.size() == 1);
    CHECK(loaded.document.portfolios[0].var_confidence_pct == 99);
    CHECK_FALSE(loaded.document.portfolios[0].show_position_var);
    CHECK(loaded.document.portfolios[0].show_unit_var);

    const char* legacy = R"({
        "format": 1,
        "name": "old",
        "focused_pane": 0,
        "next_pane_id": 1,
        "next_portfolio_id": 2,
        "focused_portfolio": 1,
        "data": {},
        "portfolios": [{"id": 1, "book": 3}],
        "layout": {"windows": ["portfolio:1"], "selected": "portfolio:1"},
        "panes": []
    })";
    const terminal::ChartbookLoadResult old = terminal::chartbookFromJson(legacy);
    REQUIRE(old.ok);
    REQUIRE(old.document.portfolios.size() == 1);
    CHECK(old.document.portfolios[0].var_confidence_pct == 95);
    CHECK(old.document.portfolios[0].show_position_var);
    CHECK(old.document.portfolios[0].show_unit_var);

    const char* invalid = R"({
        "format": 1,
        "name": "bad",
        "focused_pane": 0,
        "next_pane_id": 1,
        "next_portfolio_id": 2,
        "data": {},
        "portfolios": [{"id": 1, "book": 3, "var_confidence": 10}],
        "layout": {"windows": ["portfolio:1"], "selected": "portfolio:1"},
        "panes": []
    })";
    CHECK_FALSE(terminal::chartbookFromJson(invalid).ok);
}

TEST_CASE("saved data column order must name every live column once")
{
    std::vector<int> orders;
    std::vector<terminal::ChartbookColumn> partial;
    partial.push_back({"symbol", 80.f, true, 0});
    CHECK_FALSE(terminal::chartbookColumnDisplayOrders(partial, terminal::kChartbookDataColumnCount, orders));
    CHECK(orders.empty());

    std::vector<terminal::ChartbookColumn> columns;
    for (int index = 0; index < terminal::kChartbookDataColumnCount; ++index)
    {
        terminal::ChartbookColumn column;
        column.id = terminal::kChartbookDataColumns[index];
        column.width = 40.f;
        column.visible = true;
        column.order = terminal::kChartbookDataColumnCount - 1 - index;
        columns.push_back(std::move(column));
    }
    REQUIRE(terminal::chartbookColumnDisplayOrders(columns, terminal::kChartbookDataColumnCount, orders));
    REQUIRE(orders.size() == static_cast<std::size_t>(terminal::kChartbookDataColumnCount));
    CHECK(orders.front() == terminal::kChartbookDataColumnCount - 1);
    CHECK(orders.back() == 0);

    columns[1].order = columns[0].order;
    CHECK_FALSE(terminal::chartbookColumnDisplayOrders(columns, terminal::kChartbookDataColumnCount, orders));
    CHECK(orders.empty());
}

TEST_CASE("a failed chartbook write leaves the previous file and its bak")
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "terminal-chartbook-replace";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    const std::filesystem::path path = directory / "book.chartbook.json";
    const terminal::CChartbookDocument first = terminal::makeDefaultChartbook("first");
    terminal::CChartbookDocument second = first;
    second.name = "second";
    terminal::CChartbookDocument third = first;
    third.name = "third";
    REQUIRE(terminal::saveChartbook(path, first).empty());
    REQUIRE(terminal::saveChartbook(path, second).empty());

    const std::filesystem::path blocked = path.string() + ".tmp";
    std::filesystem::create_directory(blocked);
    const std::string error = terminal::saveChartbook(path, third);
    CHECK_FALSE(error.empty());
    std::filesystem::remove_all(blocked);

    const terminal::ChartbookLoadResult current = terminal::loadChartbook(path);
    REQUIRE(current.ok);
    CHECK(current.document.name == "second");
    const terminal::ChartbookLoadResult backup = terminal::loadChartbook(path.string() + ".bak");
    REQUIRE(backup.ok);
    CHECK(backup.document.name == "first");

    const std::filesystem::path startup = directory / "terminal.json";
    terminal::StartupSettings settings;
    settings.open_on_startup = {"a"};
    REQUIRE(terminal::saveStartupSettings(startup, settings).empty());
    settings.open_on_startup = {"b"};
    REQUIRE(terminal::saveStartupSettings(startup, settings).empty());
    const std::filesystem::path startup_blocked = startup.string() + ".tmp";
    std::filesystem::create_directory(startup_blocked);
    settings.open_on_startup = {"c"};
    CHECK_FALSE(terminal::saveStartupSettings(startup, settings).empty());
    std::filesystem::remove_all(startup_blocked);

    const terminal::StartupLoadResult startup_now = terminal::loadStartupSettings(startup);
    REQUIRE(startup_now.ok);
    REQUIRE(startup_now.settings.open_on_startup.size() == 1);
    CHECK(startup_now.settings.open_on_startup[0] == "b");
    const terminal::StartupLoadResult startup_bak = terminal::loadStartupSettings(startup.string() + ".bak");
    REQUIRE(startup_bak.ok);
    REQUIRE(startup_bak.settings.open_on_startup.size() == 1);
    CHECK(startup_bak.settings.open_on_startup[0] == "a");

    std::filesystem::remove_all(directory);
}

TEST_CASE("symbol link groups persist on chartbook pane records")
{
    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("groups");
    CHECK(terminal::chartbookToJson(document).find("link_group") == std::string::npos);

    document.panes[0].link_group = 1;
    terminal::chartbookInsertFinancials(document.layout, 1);
    terminal::ChartbookFinancials sheet;
    sheet.id = 1;
    sheet.symbol = "AAPL";
    sheet.link_group = 1;
    document.financials.push_back(sheet);
    document.next_financials_id = 2;
    terminal::chartbookInsertOptions(document.layout, 1);
    terminal::ChartbookOptions chain;
    chain.id = 1;
    chain.symbol = "MSFT";
    chain.link_group = 2;
    document.options.push_back(chain);
    document.next_options_id = 2;

    const std::string text = terminal::chartbookToJson(document);
    const terminal::ChartbookLoadResult loaded = terminal::chartbookFromJson(text);
    REQUIRE(loaded.ok);
    CHECK(loaded.document.format == 1);
    CHECK(loaded.document.panes[0].link_group == 1);
    CHECK(loaded.document.financials[0].link_group == 1);
    CHECK(loaded.document.options[0].link_group == 2);
    CHECK(terminal::chartbookToJson(loaded.document) == text);

    document.panes[0].link_group = 0;
    document.financials[0].link_group = 0;
    document.options[0].link_group = 0;
    const std::string bare = terminal::chartbookToJson(document);
    CHECK(bare.find("link_group") == std::string::npos);
    const terminal::ChartbookLoadResult ungrouped = terminal::chartbookFromJson(bare);
    REQUIRE(ungrouped.ok);
    CHECK(ungrouped.document.panes[0].link_group == 0);
    CHECK(ungrouped.document.financials[0].link_group == 0);
    CHECK(ungrouped.document.options[0].link_group == 0);

    nlohmann::json root = nlohmann::json::parse(text);
    root["panes"][0]["link_group"] = "nope";
    const terminal::ChartbookLoadResult bad_type = terminal::chartbookFromJson(root.dump());
    CHECK_FALSE(bad_type.ok);
    CHECK(bad_type.error == "link_group is not an integer");
    CHECK(bad_type.document.panes.empty());

    for (const int group : {0, 5, -1})
    {
        root = nlohmann::json::parse(text);
        root["financials"][0]["link_group"] = group;
        const terminal::ChartbookLoadResult bad_group = terminal::chartbookFromJson(root.dump());
        CHECK_FALSE(bad_group.ok);
        CHECK(bad_group.error == "unknown symbol link group");
        CHECK(bad_group.document.panes.empty());
    }
}
