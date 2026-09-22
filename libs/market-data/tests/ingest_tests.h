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
        terminal::HttpResponse response;
        response.status = 200;
        response.body = json;
        return response;
    };
    const auto result = terminal::ingestDailySymbol(store, get, "AAPL", 20241231, 20250102);
    CHECK(gets == 1);
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

TEST_CASE("ingestDailySymbol skips HTTP when daily coverage is complete")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto get = [&](std::string_view) {
        terminal::HttpResponse response;
        response.status = 200;
        response.body = dailyPageJson({20250115});
        return response;
    };
    (void)terminal::ingestDailySymbol(store, get, "AAPL", 20250115, 20250115);
    int gets = 0;
    auto blocked = [&](std::string_view) {
        ++gets;
        return terminal::HttpResponse{};
    };
    const auto again = terminal::ingestDailySymbol(store, blocked, "AAPL", 20250115, 20250115);
    CHECK(gets == 0);
    CHECK_FALSE(again.days.empty());
}

TEST_CASE("ingestDailySymbol 404 stops without missing flood")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto get = [](std::string_view) {
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
    auto get = [](std::string_view) {
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
        if (urls.size() == 1)
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
    REQUIRE(urls.size() == 2);
    CHECK(urls.front().find("endDate=20250115") != std::string::npos);
    CHECK(urls.back().find("endDate=") != std::string::npos);
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
