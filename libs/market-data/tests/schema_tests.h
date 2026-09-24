// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Schema.h"
#include "market_data/Store.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

TEST_CASE("open empty path applies schema v4")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    CHECK(store.userVersion() == 4);
    CHECK(store.foreignKeysEnabled());
    const auto names = store.tableNames();
    REQUIRE(names.size() == 10);
    CHECK(names[0] == "bar");
    CHECK(names[1] == "corporate_action");
    CHECK(names[2] == "coverage_day");
    CHECK(names[3] == "instrument");
    CHECK(names[4] == "instrument_listing");
    CHECK(names[5] == "option_expiry");
    CHECK(names[6] == "option_quote");
    CHECK(names[7] == "option_underlying");
    CHECK(names[8] == "statement_cell");
    CHECK(names[9] == "statement_snapshot");
    const auto views = store.viewNames();
    REQUIRE(views.size() == 1);
    CHECK(views[0] == "instrument_current");
}

TEST_CASE("second open does not re-run the baseline")
{
    TempDb tmp;
    {
        terminal::Store first(tmp.path());
        CHECK(first.userVersion() == 4);
        (void)first.testingInsertInstrument("AAPL");
    }
    terminal::Store second(tmp.path());
    CHECK(second.userVersion() == 4);
    CHECK(second.tableNames().size() == 10);
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
        CHECK(store.userVersion() == 4);
    }
    terminal::Store::testingSetUserVersion(tmp.path(), 99);
    CHECK_THROWS_AS(terminal::Store(tmp.path()), std::runtime_error);
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
