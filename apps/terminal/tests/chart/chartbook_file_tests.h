// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartbookFile.h"
#include "chart/CStudy.h"
#include "chart/studies/CBollinger.h"
#include "chart/studies/CMovingAverage.h"

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
