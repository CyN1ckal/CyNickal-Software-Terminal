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
