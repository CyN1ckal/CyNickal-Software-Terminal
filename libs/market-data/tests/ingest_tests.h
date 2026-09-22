// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/MboumIngest.h"
#include "market_data/MboumJson.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Secrets.h"
#include "market_data/Store.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

TEST_CASE("loadMboumApiKey reads mboum from secrets.json")
{
    const auto path = std::filesystem::temp_directory_path() / "terminal-secrets-test.json";
    {
        std::ofstream out(path);
        out << R"({"mboum":"test-key-123"})";
    }
    CHECK(terminal::loadMboumApiKey(path) == "test-key-123");
    std::filesystem::remove(path);
}

TEST_CASE("loadMboumApiKey rejects missing key")
{
    const auto path = std::filesystem::temp_directory_path() / "terminal-secrets-empty.json";
    {
        std::ofstream out(path);
        out << R"({"other":"x"})";
    }
    CHECK_THROWS_AS(terminal::loadMboumApiKey(path), std::runtime_error);
    std::filesystem::remove(path);
}

TEST_CASE("NYSE holidays 2025 and Saturday observation")
{
    CHECK(terminal::isNyseHoliday(20250101));
    CHECK(terminal::isNyseHoliday(20250418));  // Good Friday
    CHECK(terminal::isNyseHoliday(20250619));  // Juneteenth
    CHECK(terminal::isNyseHoliday(20251127));  // Thanksgiving
    CHECK(terminal::isNyseHoliday(20260703));  // July 4 2026 is Saturday → Friday
    CHECK(terminal::isNyseHoliday(20211231));  // New Year 2022 Saturday → prior Friday
    CHECK(terminal::isNyseHoliday(20271231));
    CHECK_FALSE(terminal::isNyseHoliday(20280101));  // Saturday; weekend skip, not observed
    CHECK_FALSE(terminal::isNyseHoliday(20250115));
    const auto sessions = terminal::nyseSessions(20250101, 20250103);
    REQUIRE(sessions.size() == 2);
    CHECK(sessions[0] == 20250102);
    CHECK(sessions[1] == 20250103);
    const auto nye = terminal::nyseSessions(20271230, 20280103);
    REQUIRE(nye.size() == 2);
    CHECK(nye[0] == 20271230);
    CHECK(nye[1] == 20280103);
}

TEST_CASE("parse v3 page from live shape")
{
    constexpr std::string_view json = R"({
      "meta": {"ticker":"AAPL","interval":"1min","splits":"0","dividends":"0","status":200},
      "body": [
        {"datetime":"2025-01-15 09:30","open":234.635,"high":235.775,"low":234.43,"close":235.775,"volume":774061},
        {"datetime":"2025-01-15 15:59","open":237.81,"high":238,"low":237.6,"close":237.87,"volume":10027991}
      ]
    })";
    const auto page = terminal::parseMboumV3Historical(json);
    CHECK_FALSE(page.splits);
    REQUIRE(page.bars.size() == 2);
    CHECK(page.bars.front().datetime == "2025-01-15 09:30");
    CHECK(page.bars.back().volume == 10027991.0);
}

TEST_CASE("parse v3 no-data 500 body")
{
    const auto page = terminal::parseMboumV3Historical(R"({"message":"Failed to fetch historical data"})");
    CHECK(page.no_data);
    CHECK(page.bars.empty());
}

TEST_CASE("v3 URL uses Laravel 0/1 booleans and 1min")
{
    const auto url = terminal::mboumV3HistoricalUrl("AAPL", 20250115);
    CHECK(url.find("interval=1min") != std::string::npos);
    CHECK(url.find("splits=0") != std::string::npos);
    CHECK(url.find("dividends=0") != std::string::npos);
    CHECK(url.find("startDate=20250115093000") != std::string::npos);
    CHECK(url.find("endDate=20250115160000") != std::string::npos);
    CHECK(url.find("false") == std::string::npos);
}

TEST_CASE("parse v3 daily page from live shape")
{
    constexpr std::string_view json = R"({
      "meta": {"ticker":"AAPL","interval":"daily","splits":"0","dividends":"0","status":200},
      "body": [
        {"symbol":"AAPL","date":"2026-01-05","open":271.01,"high":271.51,"low":266.14,"close":266.9144,"volume":45703896},
        {"symbol":"AAPL","date":"2026-01-06","open":267.12,"high":267.84,"low":262.12,"close":262.3,"volume":52370895}
      ]
    })";
    const auto page = terminal::parseMboumV3Daily(json);
    CHECK_FALSE(page.splits);
    REQUIRE(page.bars.size() == 2);
    CHECK(page.bars.front().date == "2026-01-05");
    CHECK(page.bars.front().close == 266.9144);
    CHECK(page.bars.back().volume == 52370895.0);
}

TEST_CASE("parse v3 daily skips rows without date and ignores 1m datetime pages")
{
    const auto empty = terminal::parseMboumV3Daily(R"({
      "meta": {"splits":"0"},
      "body": [
        {"datetime":"2025-01-15 09:30","open":10,"high":11,"low":9,"close":10,"volume":100}
      ]
    })");
    CHECK(empty.bars.empty());
    CHECK_FALSE(empty.no_data);

    const auto mixed = terminal::parseMboumV3Daily(R"({
      "body": [
        {"open":1,"high":1,"low":1,"close":1,"volume":1},
        {"date":"2025-01-15","open":2,"high":3,"low":1,"close":2,"volume":10}
      ]
    })");
    REQUIRE(mixed.bars.size() == 1);
    CHECK(mixed.bars.front().date == "2025-01-15");
}

TEST_CASE("parse v3 daily no-data 404 body")
{
    const auto page =
        terminal::parseMboumV3Daily(R"({"message":"No historical data found for the specified criteria"})");
    CHECK(page.no_data);
    CHECK(page.bars.empty());
}

TEST_CASE("v3 daily URL uses interval daily and YYYYMMDD dates")
{
    const auto url = terminal::mboumV3DailyUrl("AAPL", 19900101, 20260921);
    CHECK(url.find("interval=daily") != std::string::npos);
    CHECK(url.find("limit=4000") != std::string::npos);
    CHECK(url.find("startDate=19900101") != std::string::npos);
    CHECK(url.find("endDate=20260921") != std::string::npos);
    CHECK(url.find("splits=0") != std::string::npos);
    CHECK(url.find("dividends=0") != std::string::npos);
    CHECK(url.find("order=asc") != std::string::npos);
    CHECK(url.find("false") == std::string::npos);
    CHECK(url.find("000000") == std::string::npos);
}

TEST_CASE("ingestSymbol maps a v3 page and skips holidays")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const std::string json = R"({
      "meta": {"splits":"0","status":200},
      "body": [
        {"datetime":"2025-01-15 09:30","open":10,"high":11,"low":9,"close":10,"volume":100},
        {"datetime":"2025-01-15 09:31","open":10,"high":11,"low":9,"close":10.5,"volume":110}
      ]
    })";
    std::string last_url;
    auto get = [&](std::string_view url) {
        last_url = std::string(url);
        terminal::HttpResponse response;
        response.status = 200;
        response.body = json;
        return response;
    };
    const auto result = terminal::ingestSymbol(store, get, "AAPL", 20250101, 20250115);
    CHECK(result.instrument_id > 0);
    bool saw_new_year = false;
    bool saw_session = false;
    for (const auto& day : result.days)
    {
        if (day.session_date == 20250101)
        {
            saw_new_year = true;
            CHECK(day.status == terminal::CoverageStatus::Complete);
            CHECK(day.bar_count == 0);
        }
        if (day.session_date == 20250115)
        {
            saw_session = true;
            CHECK(day.status == terminal::CoverageStatus::Partial);
            CHECK(day.bar_count == 2);
            CHECK(last_url.find("ticker=AAPL") != std::string::npos);
            CHECK(last_url.find("splits=0") != std::string::npos);
        }
    }
    CHECK(saw_new_year);
    CHECK(saw_session);
    CHECK(store.findInstrument("AAPL", std::nullopt).has_value());
}

TEST_CASE("ingestSymbol on_day fires once per session row")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto get = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = R"({"meta":{"splits":"0","status":200},"body":[]})";
        return response;
    };
    int calls = 0;
    const auto result = terminal::ingestSymbol(
        store, get, "MSFT", 20250120, 20250121,
        [&](const terminal::IngestDayResult&) { ++calls; });
    CHECK(calls == static_cast<int>(result.days.size()));
    CHECK(calls >= 1);
}

TEST_CASE("ingestSymbol reuses an existing exchange-qualified instrument")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    inst.exchange = "NMS";
    const auto id = store.upsertInstrument(inst);
    auto get = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = R"({"meta":{"splits":"0","status":200},"body":[]})";
        return response;
    };
    const auto result = terminal::ingestSymbol(store, get, "AAPL", 20250120, 20250120);
    CHECK(result.instrument_id == id);
    CHECK(store.findInstrumentsBySymbol("AAPL").size() == 1);
    CHECK_FALSE(store.findInstrument("AAPL", std::nullopt).has_value());
}

namespace {

[[nodiscard]] bool isV1SplitsUrl(std::string_view url)
{
    return url.find("diffandsplits=true") != std::string::npos;
}

[[nodiscard]] terminal::HttpResponse emptySplitsHttp()
{
    terminal::HttpResponse response;
    response.status = 200;
    response.body = R"({"meta":{"status":200},"body":{}})";
    return response;
}

std::string dailyPageJson(const std::vector<terminal::SessionDate>& dates, bool splits = false)
{
    std::string json = R"({"meta":{"splits":")";
    json += splits ? "1" : "0";
    json += R"(","status":200},"body":[)";
    for (std::size_t i = 0; i < dates.size(); ++i)
    {
        if (i != 0)
        {
            json += ',';
        }
        json += R"({"symbol":"AAPL","date":")";
        json += terminal::formatSessionDate(dates[i]);
        json += R"(","open":10,"high":11,"low":9,"close":10,"volume":100})";
    }
    json += "]}";
    return json;
}

}  // namespace

TEST_CASE("ingestDailySymbol maps a page and writes holiday coverage")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto json = dailyPageJson({20241231, 20250102});
    std::string last_url;
    int gets = 0;
    auto get = [&](std::string_view url) {
        last_url = std::string(url);
        ++gets;
        if (isV1SplitsUrl(url))
        {
            return emptySplitsHttp();
        }
        terminal::HttpResponse response;
        response.status = 200;
        response.body = json;
        return response;
    };
    const auto result = terminal::ingestDailySymbol(store, get, "AAPL", 20241231, 20250102);
    CHECK(gets == 2);
    CHECK(last_url.find("interval=daily") != std::string::npos);
    CHECK(last_url.find("startDate=20241231") != std::string::npos);
    CHECK(last_url.find("endDate=20250102") != std::string::npos);
    CHECK(store.queryBars(result.instrument_id, terminal::kTimeframe1d, 0, 4000000000).size() == 2);
    CHECK(store.queryBars(result.instrument_id, terminal::kTimeframe1m, 0, 4000000000).empty());
    bool saw_holiday = false;
    bool saw_session = false;
    for (const auto& day : result.days)
    {
        if (day.session_date == 20250101)
        {
            saw_holiday = true;
            CHECK(day.status == terminal::CoverageStatus::Complete);
            CHECK(day.bar_count == 0);
        }
        if (day.session_date == 20250102)
        {
            saw_session = true;
            CHECK(day.status == terminal::CoverageStatus::Complete);
            CHECK(day.bar_count == 1);
        }
    }
    CHECK(saw_holiday);
    CHECK(saw_session);
}

TEST_CASE("ingestDailySymbol skips bar HTTP when daily coverage is complete")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto get = [&](std::string_view url) {
        if (isV1SplitsUrl(url))
        {
            return emptySplitsHttp();
        }
        terminal::HttpResponse response;
        response.status = 200;
        response.body = dailyPageJson({20250115});
        return response;
    };
    (void)terminal::ingestDailySymbol(store, get, "AAPL", 20250115, 20250115);
    int gets = 0;
    std::string splits_url;
    auto blocked = [&](std::string_view url) {
        ++gets;
        splits_url = std::string(url);
        return emptySplitsHttp();
    };
    const auto again = terminal::ingestDailySymbol(store, blocked, "AAPL", 20250115, 20250115);
    CHECK(gets == 1);
    CHECK(splits_url.find("diffandsplits=true") != std::string::npos);
    CHECK(splits_url.find("interval=daily") == std::string::npos);
    CHECK_FALSE(again.days.empty());
}

TEST_CASE("ingestDailySymbol 404 stops without missing flood")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto get = [](std::string_view url) {
        if (isV1SplitsUrl(url))
        {
            return emptySplitsHttp();
        }
        terminal::HttpResponse response;
        response.status = 404;
        response.body = R"({"message":"No historical data found for the specified criteria"})";
        return response;
    };
    const auto result = terminal::ingestDailySymbol(store, get, "AAPL", 19800101, 19810115);
    CHECK(result.days.empty());
    CHECK(store.queryCoverageDays(result.instrument_id, terminal::kTimeframe1d).empty());
}

TEST_CASE("ingestDailySymbol splits page is error and writes no bars")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto get = [](std::string_view url) {
        if (isV1SplitsUrl(url))
        {
            return emptySplitsHttp();
        }
        terminal::HttpResponse response;
        response.status = 200;
        response.body = dailyPageJson({20250115}, true);
        return response;
    };
    const auto result = terminal::ingestDailySymbol(store, get, "AAPL", 20250115, 20250115);
    CHECK(store.queryBars(result.instrument_id, terminal::kTimeframe1d, 0, 4000000000).empty());
    REQUIRE_FALSE(result.days.empty());
    CHECK(result.days.front().status == terminal::CoverageStatus::Error);
}

TEST_CASE("ingestDailySymbol pages oldest-ward when the first page is full")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto sessions = terminal::nyseSessions(20080101, 20250115);
    REQUIRE(sessions.size() > terminal::kMboumDailyPageLimit + 5);
    const std::vector<terminal::SessionDate> page1(
        sessions.end() - terminal::kMboumDailyPageLimit, sessions.end());
    const std::vector<terminal::SessionDate> page2(sessions.begin() + 10, sessions.begin() + 15);
    std::vector<std::string> urls;
    auto get = [&](std::string_view url) {
        urls.emplace_back(url);
        terminal::HttpResponse response;
        response.status = 200;
        const std::string text{url};
        if (text.find("diffandsplits=true") != std::string::npos)
        {
            response.body = R"({"meta":{"status":200},"body":{}})";
        }
        else if (text.find("endDate=20250115") != std::string::npos)
        {
            response.body = dailyPageJson(page1);
        }
        else
        {
            response.body = dailyPageJson(page2);
        }
        return response;
    };
    const auto result = terminal::ingestDailySymbol(store, get, "AAPL", 20080101, 20250115);
    REQUIRE(urls.size() == 3);
    CHECK(urls.front().find("diffandsplits=true") != std::string::npos);
    CHECK(urls[1].find("endDate=20250115") != std::string::npos);
    CHECK(urls.back().find("interval=daily") != std::string::npos);
    CHECK(urls.back().find("endDate=20250115") == std::string::npos);
    const auto bars = store.queryBars(result.instrument_id, terminal::kTimeframe1d, 0, 4000000000);
    CHECK(bars.size() == static_cast<std::size_t>(terminal::kMboumDailyPageLimit + 5));
}

TEST_CASE("ingestSymbol fails closed when a symbol has two instruments")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument aapl;
    aapl.symbol = "AAPL";
    store.upsertInstrument(aapl);
    aapl.exchange = "NMS";
    store.upsertInstrument(aapl);
    auto get = [](std::string_view) {
        return terminal::HttpResponse{};
    };
    CHECK_THROWS_AS(terminal::ingestSymbol(store, get, "AAPL", 20250120, 20250120), std::runtime_error);
}

TEST_CASE("parseMboumV1SplitEvents reads NVDA 10-for-1 and skips bad rows")
{
    const char* json = R"({
      "meta": {"status": 200},
      "body": {
        "1717767000": {"date": "2024-06-07", "open": 119.77, "close": 120.888, "volume": 412385000},
        "events": {
          "dividends": {"1718112600": {"amount": 0.01, "date": 1718112600}},
          "splits": {
            "1718026200": {"date": 1718026200, "numerator": 10, "denominator": 1, "splitRatio": "10:1"},
            "1": {"date": 1, "numerator": 1, "denominator": 0},
            "2": {"numerator": 2, "denominator": 1}
          }
        }
      }
    })";
    const auto events = terminal::parseMboumV1SplitEvents(json);
    REQUIRE(events.size() == 1);
    CHECK(events[0].ex_ts == 1718026200);
    CHECK(events[0].split_ratio == 10.0);
    const std::string url = terminal::mboumV1SplitsUrl("NVDA");
    CHECK(url.find("ticker=NVDA") != std::string::npos);
    CHECK(url.find("interval=1mo") != std::string::npos);
    CHECK(url.find("interval=1d") == std::string::npos);
    CHECK(url.find("diffandsplits=true") != std::string::npos);
}

TEST_CASE("ingestSplits writes one split and does not change bars")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument([] {
        terminal::Instrument inst;
        inst.symbol = "NVDA";
        inst.timezone = "America/New_York";
        return inst;
    }());
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1d;
    bar.ts = 1717767000;
    bar.open = 1197.7;
    bar.high = 1216.91;
    bar.low = 1180.22;
    bar.close = 1208.88;
    bar.volume = 41238500.0;
    CHECK(store.upsertBars(std::vector<terminal::Bar>{bar}).written == 1);

    const char* json = R"({
      "body": {"events": {"splits": {
        "1718026200": {"date": 1718026200, "numerator": 10, "denominator": 1}
      }}}
    })";
    int gets = 0;
    auto get = [&](std::string_view) {
        ++gets;
        terminal::HttpResponse response;
        response.status = 200;
        response.body = json;
        return response;
    };
    const auto first = terminal::ingestSplits(store, get, "NVDA");
    CHECK(first.upserted == 1);
    CHECK(gets == 1);
    const auto stored = store.queryBars(id, terminal::kTimeframe1d, 0, 4000000000);
    REQUIRE(stored.size() == 1);
    CHECK(stored[0].close == 1208.88);

    const auto again = terminal::ingestSplits(store, get, "NVDA");
    CHECK(again.upserted == 1);
    const auto rows = store.queryCorporateActions(id, 0, 2000000000);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].type == terminal::CorporateActionType::Split);
    CHECK(rows[0].split_ratio == 10.0);
    CHECK(rows[0].ex_ts == 1718026200);

    const char* other_ratio = R"({
      "body": {"events": {"splits": {
        "1718026200": {"date": 1718026200, "numerator": 5, "denominator": 1}
      }}}
    })";
    auto replace = [&](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = other_ratio;
        return response;
    };
    const auto blocked = terminal::ingestSplits(store, replace, "NVDA");
    CHECK(blocked.upserted == 0);
    const auto kept = store.queryCorporateActions(id, 0, 2000000000);
    REQUIRE(kept.size() == 1);
    CHECK(kept[0].split_ratio == 10.0);
}

TEST_CASE("ingestSplits authentication failure throws and writes nothing")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto get = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 401;
        return response;
    };
    CHECK_THROWS_AS(terminal::ingestSplits(store, get, "NVDA"), std::runtime_error);
    const auto found = store.findInstrumentsBySymbol("NVDA");
    REQUIRE(found.size() == 1);
    CHECK(store.queryCorporateActions(found.front().id, 0, 2000000000).empty());
}

TEST_CASE("ingestSplits transport, HTTP, and parse failures throw and write nothing")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto transport = [](std::string_view) -> terminal::HttpResponse {
        throw std::runtime_error("connection reset");
    };
    CHECK_THROWS_AS(terminal::ingestSplits(store, transport, "NVDA"), std::runtime_error);

    auto http = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 500;
        return response;
    };
    CHECK_THROWS_AS(terminal::ingestSplits(store, http, "NVDA"), std::runtime_error);

    auto parse = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = "not-json";
        return response;
    };
    CHECK_THROWS_AS(terminal::ingestSplits(store, parse, "NVDA"), std::runtime_error);

    const auto found = store.findInstrumentsBySymbol("NVDA");
    REQUIRE(found.size() == 1);
    CHECK(store.queryCorporateActions(found.front().id, 0, 2000000000).empty());
    CHECK(store.queryBars(found.front().id, terminal::kTimeframe1d, 0, 4000000000).empty());
}

TEST_CASE("ingestDailySymbol propagates a splits fetch failure and writes no bars")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto get = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 502;
        return response;
    };
    CHECK_THROWS_AS(terminal::ingestDailySymbol(store, get, "AAPL", 20250115, 20250115),
                    std::runtime_error);
    const auto found = store.findInstrumentsBySymbol("AAPL");
    REQUIRE(found.size() == 1);
    CHECK(store.queryBars(found.front().id, terminal::kTimeframe1d, 0, 4000000000).empty());
    CHECK(store.queryCorporateActions(found.front().id, 0, 2000000000).empty());
}

TEST_CASE("parse v2 statement keeps vendor order, typed values, and TTM")
{
    constexpr std::string_view json = R"({
      "meta": {"version": "v1.0"},
      "body": {
        "revenue": {"2025-09-27": 416161000000, "2024-09-28": 391035000000},
        "epsdil": {"2025-09-27": 6.08},
        "fiscalYear": {"2025-09-27": "2025", "TTM": "2025"}
      }
    })";
    const auto page = terminal::parseMboumV2Statement(json);
    CHECK_FALSE(page.no_data);
    REQUIRE(page.cells.size() == 5);
    CHECK(page.cells[0].line_item == "revenue");
    CHECK(page.cells[0].period_end == "2025-09-27");
    CHECK(std::get<std::int64_t>(page.cells[0].value) == 416161000000);
    CHECK(page.cells[1].period_end == "2024-09-28");
    CHECK(page.cells[2].line_item == "epsdil");
    CHECK(std::get<double>(page.cells[2].value) == 6.08);
    CHECK(page.cells[4].period_end == "TTM");
    CHECK(std::get<std::string>(page.cells[4].value) == "2025");
}

TEST_CASE("parse v2 statement treats no-data as an empty grid and rejects a bad period")
{
    const auto empty = terminal::parseMboumV2Statement(
        R"({"success":false,"message":"No data returned"})");
    CHECK(empty.no_data);
    CHECK(empty.cells.empty());
    CHECK_THROWS_AS(terminal::parseMboumV2Statement(R"({"body":{"revenue":{"FY2025":1}}})"),
                    std::runtime_error);
    const auto skipped = terminal::parseMboumV2Statement(
        R"({"body":{"revenue":{"2025-09-27":null,"2024-09-28":2}}})");
    REQUIRE(skipped.cells.size() == 1);
    CHECK(skipped.cells[0].period_end == "2024-09-28");
}

TEST_CASE("v2 statement URL names the module and timeframe")
{
    const auto annual = terminal::mboumV2StatementUrl("AAPL", terminal::StatementKind::Income,
                                                      terminal::StatementTimeframe::Annually);
    CHECK(annual.find("ticker=AAPL") != std::string::npos);
    CHECK(annual.find("module=income-statement-v2") != std::string::npos);
    CHECK(annual.find("timeframe=annually") != std::string::npos);
    const auto quarter = terminal::mboumV2StatementUrl("MSFT", terminal::StatementKind::Cashflow,
                                                       terminal::StatementTimeframe::Quarterly);
    CHECK(quarter.find("module=cashflow-statement-v2") != std::string::npos);
    CHECK(quarter.find("timeframe=quarterly") != std::string::npos);
    const auto balance = terminal::mboumV2StatementUrl("IBM", terminal::StatementKind::Balance,
                                                       terminal::StatementTimeframe::Annually);
    CHECK(balance.find("module=balance-sheet-v2") != std::string::npos);
}

TEST_CASE("ingestStatement stores yearly and quarterly grids and an empty no-data snapshot")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const char* annual = R"({
      "body": {"revenue": {"2025-09-27": 10, "2024-09-28": 9}}
    })";
    const char* quarter = R"({
      "body": {"revenue": {"2025-06-28": 3}}
    })";
    auto annual_get = [&](std::string_view url) {
        CHECK(std::string(url).find("timeframe=annually") != std::string::npos);
        CHECK(std::string(url).find("module=income-statement-v2") != std::string::npos);
        terminal::HttpResponse response;
        response.status = 200;
        response.body = annual;
        return response;
    };
    const auto yearly = terminal::ingestStatement(store, annual_get, "AAPL",
                                                  terminal::StatementKind::Income,
                                                  terminal::StatementTimeframe::Annually);
    CHECK(yearly.cell_count == 2);
    CHECK_FALSE(yearly.no_data);
    auto quarter_get = [&](std::string_view url) {
        CHECK(std::string(url).find("timeframe=quarterly") != std::string::npos);
        terminal::HttpResponse response;
        response.status = 200;
        response.body = quarter;
        return response;
    };
    const auto quarterly = terminal::ingestStatement(store, quarter_get, "AAPL",
                                                     terminal::StatementKind::Income,
                                                     terminal::StatementTimeframe::Quarterly);
    CHECK(quarterly.instrument_id == yearly.instrument_id);
    CHECK(quarterly.cell_count == 1);
    const auto annual_line = store.queryStatementLine(
        yearly.instrument_id, terminal::StatementKind::Income,
        terminal::StatementTimeframe::Annually, "revenue");
    REQUIRE(annual_line.size() == 2);
    const auto quarter_line = store.queryStatementLine(
        yearly.instrument_id, terminal::StatementKind::Income,
        terminal::StatementTimeframe::Quarterly, "revenue");
    REQUIRE(quarter_line.size() == 1);
    CHECK(std::get<std::int64_t>(quarter_line[0].value) == 3);

    auto none = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = R"({"success":false,"message":"No data returned"})";
        return response;
    };
    const auto cleared = terminal::ingestStatement(store, none, "AAPL", terminal::StatementKind::Income,
                                                   terminal::StatementTimeframe::Quarterly);
    CHECK(cleared.no_data);
    CHECK(cleared.cell_count == 0);
    CHECK(store.findStatementSnapshot(yearly.instrument_id, terminal::StatementKind::Income,
                                      terminal::StatementTimeframe::Quarterly)
              .has_value());
    CHECK(store.queryStatementCells(yearly.instrument_id, terminal::StatementKind::Income,
                                    terminal::StatementTimeframe::Quarterly)
              .empty());
    CHECK(store.queryStatementLine(yearly.instrument_id, terminal::StatementKind::Income,
                                   terminal::StatementTimeframe::Annually, "revenue")
              .size() == 2);
}

TEST_CASE("ingestStatement HTTP and parse failures leave the stored grid")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto ok = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = R"({"body":{"revenue":{"2025-09-27":4}}})";
        return response;
    };
    const auto first = terminal::ingestStatement(store, ok, "AAPL", terminal::StatementKind::Income,
                                                 terminal::StatementTimeframe::Annually);
    auto http = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 500;
        return response;
    };
    CHECK_THROWS_AS(terminal::ingestStatement(store, http, "AAPL", terminal::StatementKind::Income,
                                              terminal::StatementTimeframe::Annually),
                    std::runtime_error);
    auto parse = [](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = "not-json";
        return response;
    };
    CHECK_THROWS_AS(terminal::ingestStatement(store, parse, "AAPL", terminal::StatementKind::Income,
                                              terminal::StatementTimeframe::Annually),
                    std::runtime_error);
    const auto kept = store.queryStatementLine(first.instrument_id, terminal::StatementKind::Income,
                                               terminal::StatementTimeframe::Annually, "revenue");
    REQUIRE(kept.size() == 1);
    CHECK(std::get<std::int64_t>(kept[0].value) == 4);
}
