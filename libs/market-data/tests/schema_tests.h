// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Figi.h"
#include "market_data/Schema.h"
#include "market_data/Store.h"

#include "../private/Sqlite.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

[[nodiscard]] std::int64_t countRows(const std::filesystem::path& path, std::string_view table)
{
    terminal::SqliteDb db(path);
    const std::string sql = "SELECT COUNT(*) FROM " + std::string(table);
    terminal::SqliteStmt stmt(db.handle(), sql);
    if (!stmt.stepRow())
    {
        throw std::runtime_error("COUNT returned no row");
    }
    const auto count = stmt.columnInt64(0);
    stmt.reset();
    return count;
}

}  // namespace

TEST_CASE("open empty path applies schema v5")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    CHECK(store.userVersion() == 5);
    CHECK(store.foreignKeysEnabled());
    const auto names = store.tableNames();
    REQUIRE(names.size() == 12);
    CHECK(names[0] == "bar");
    CHECK(names[1] == "corporate_action");
    CHECK(names[2] == "coverage_day");
    CHECK(names[3] == "instrument");
    CHECK(names[4] == "instrument_listing");
    CHECK(names[5] == "option_expiry");
    CHECK(names[6] == "option_quote");
    CHECK(names[7] == "option_underlying");
    CHECK(names[8] == "portfolio");
    CHECK(names[9] == "portfolio_holding");
    CHECK(names[10] == "statement_cell");
    CHECK(names[11] == "statement_snapshot");
    const auto views = store.viewNames();
    REQUIRE(views.size() == 1);
    CHECK(views[0] == "instrument_current");
    CHECK(countRows(tmp.path(), "portfolio") == 0);
    CHECK(countRows(tmp.path(), "portfolio_holding") == 0);
}

TEST_CASE("second open does not re-run the baseline")
{
    TempDb tmp;
    {
        terminal::Store first(tmp.path());
        CHECK(first.userVersion() == 5);
        (void)first.testingInsertInstrument("AAPL");
    }
    terminal::Store second(tmp.path());
    CHECK(second.userVersion() == 5);
    CHECK(second.tableNames().size() == 12);
    CHECK(second.findOpenListing("AAPL").has_value());
}

TEST_CASE("schema v1, v2, and v3 files are refused with the reset message")
{
    for (const int version : {1, 2, 3})
    {
        TempDb tmp;
        terminal::Store::testingSetUserVersion(tmp.path(), version);
        try
        {
            terminal::Store store(tmp.path());
            FAIL("expected the reset message");
        }
        catch (const std::runtime_error& ex)
        {
            const std::string what = ex.what();
            CHECK(what.find("schema v" + std::to_string(version)) != std::string::npos);
            CHECK(what.find("v4 changed instrument identity") != std::string::npos);
            CHECK(what.find("v5") == std::string::npos);
            CHECK(what.find("does not migrate") != std::string::npos);
            CHECK(what.find("-wal and -shm") != std::string::npos);
            CHECK(what.find(tmp.path().string()) != std::string::npos);
        }
        // Refusing does not stamp or create anything.
        CHECK(terminal::Store::testingUserVersion(tmp.path()) == version);
        CHECK(terminal::Store::testingTableNames(tmp.path()).empty());
    }
}

TEST_CASE("user_version 99 is refused")
{
    TempDb tmp;
    {
        terminal::Store store(tmp.path());
        CHECK(store.userVersion() == 5);
    }
    terminal::Store::testingSetUserVersion(tmp.path(), 99);
    try
    {
        terminal::Store store(tmp.path());
        FAIL("expected exceeds-binary error");
    }
    catch (const std::runtime_error& ex)
    {
        CHECK(std::string(ex.what()) == "database user_version exceeds this binary");
    }
    CHECK(terminal::Store::testingUserVersion(tmp.path()) == 99);
}

TEST_CASE("user_version 4 missing instrument_listing is refused")
{
    TempDb tmp;
    terminal::Store::testingSetUserVersion(tmp.path(), 4);
    try
    {
        terminal::Store store(tmp.path());
        FAIL("expected missing-table error");
    }
    catch (const std::runtime_error& ex)
    {
        CHECK(std::string(ex.what()).find("missing required table") != std::string::npos);
    }
    CHECK(terminal::Store::testingUserVersion(tmp.path()) == 4);
    CHECK(terminal::Store::testingTableNames(tmp.path()).empty());
}

TEST_CASE("version 4 file migrates and keeps the seeded instrument")
{
    TempDb tmp;
    const std::string symbol = "AAPL";
    terminal::Store::testingCreateSchemaV4(tmp.path());
    CHECK(terminal::Store::testingUserVersion(tmp.path()) == 4);
    terminal::Store::testingSeedV4Instrument(tmp.path(), symbol);
    CHECK(terminal::Store::testingUserVersion(tmp.path()) == 4);
    const auto before = terminal::Store::testingTableNames(tmp.path());
    CHECK(std::find(before.begin(), before.end(), "portfolio") == before.end());
    CHECK(std::find(before.begin(), before.end(), "portfolio_holding") == before.end());

    terminal::Store store(tmp.path());
    CHECK(store.userVersion() == 5);
    const std::string figi = terminal::testingFigiFor(symbol);
    const auto by_figi = store.findInstrumentByFigi(figi);
    const auto open = store.findOpenListing(symbol);
    REQUIRE(by_figi.has_value());
    REQUIRE(open.has_value());
    CHECK(by_figi->id == open->id);
    CHECK(by_figi->figi == figi);
    CHECK(open->symbol == symbol);
    CHECK(open->listing_open);
    CHECK(by_figi->asset_class == terminal::AssetClass::Equity);
    const auto names = store.tableNames();
    REQUIRE(names.size() == 12);
    CHECK(names[8] == "portfolio");
    CHECK(names[9] == "portfolio_holding");
    CHECK(countRows(tmp.path(), "portfolio") == 0);
    CHECK(countRows(tmp.path(), "portfolio_holding") == 0);
}

TEST_CASE("testing hooks do not restamp a file that is not a bare version 4")
{
    TempDb empty;
    CHECK_THROWS_AS(terminal::Store::testingSeedV4Instrument(empty.path(), "AAPL"), std::runtime_error);
    CHECK(terminal::Store::testingUserVersion(empty.path()) == 0);

    TempDb current;
    {
        terminal::Store store(current.path());
        CHECK(store.userVersion() == 5);
    }
    CHECK_THROWS_AS(terminal::Store::testingCreateSchemaV4(current.path()), std::runtime_error);
    CHECK_THROWS_AS(terminal::Store::testingSeedV4Instrument(current.path(), "AAPL"), std::runtime_error);
    CHECK(terminal::Store::testingUserVersion(current.path()) == 5);
}

TEST_CASE("embedded schema matches v4.sql")
{
    const std::filesystem::path sql_path =
        std::filesystem::path(TERMINAL_MARKET_DATA_SCHEMA_DIR) / "v4.sql";
    std::ifstream in(sql_path, std::ios::binary);
    REQUIRE(in);
    std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // MSVC drops CR from raw string literals. The SQL text is what must match.
    file.erase(std::remove(file.begin(), file.end(), '\r'), file.end());
    std::string embedded(terminal::schemaV4());
    embedded.erase(std::remove(embedded.begin(), embedded.end(), '\r'), embedded.end());
    CHECK(file == embedded);
}

TEST_CASE("embedded schema matches v5.sql")
{
    const std::filesystem::path sql_path =
        std::filesystem::path(TERMINAL_MARKET_DATA_SCHEMA_DIR) / "v5.sql";
    std::ifstream in(sql_path, std::ios::binary);
    REQUIRE(in);
    std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // MSVC drops CR from raw string literals. The SQL text is what must match.
    file.erase(std::remove(file.begin(), file.end(), '\r'), file.end());
    std::string embedded(terminal::schemaV5());
    embedded.erase(std::remove(embedded.begin(), embedded.end(), '\r'), embedded.end());
    CHECK(file == embedded);
}
