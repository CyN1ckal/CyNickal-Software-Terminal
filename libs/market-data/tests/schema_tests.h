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

TEST_CASE("open empty path applies schema v3")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    CHECK(store.userVersion() == 3);
    CHECK(store.foreignKeysEnabled());
    const auto names = store.tableNames();
    REQUIRE(names.size() == 9);
    CHECK(names[0] == "bar");
    CHECK(names[1] == "corporate_action");
    CHECK(names[2] == "coverage_day");
    CHECK(names[3] == "instrument");
    CHECK(names[4] == "option_expiry");
    CHECK(names[5] == "option_quote");
    CHECK(names[6] == "option_underlying");
    CHECK(names[7] == "statement_cell");
    CHECK(names[8] == "statement_snapshot");
}

TEST_CASE("second open is a no-op migrate")
{
    TempDb tmp;
    {
        terminal::Store first(tmp.path());
        CHECK(first.userVersion() == 3);
    }
    terminal::Store second(tmp.path());
    CHECK(second.userVersion() == 3);
    CHECK(second.tableNames().size() == 9);
}

TEST_CASE("user_version 1 gains statement and option tables")
{
    TempDb tmp;
    terminal::Store::testingCreateSchemaV1(tmp.path());
    terminal::Store store(tmp.path());
    CHECK(store.userVersion() == 3);
    const auto names = store.tableNames();
    REQUIRE(names.size() == 9);
    CHECK(names[4] == "option_expiry");
    CHECK(names[7] == "statement_cell");
    CHECK(names[8] == "statement_snapshot");
}

TEST_CASE("user_version 2 gains option tables")
{
    TempDb tmp;
    terminal::Store::testingCreateSchemaV2(tmp.path());
    terminal::Store store(tmp.path());
    CHECK(store.userVersion() == 3);
    const auto names = store.tableNames();
    REQUIRE(names.size() == 9);
    CHECK(names[4] == "option_expiry");
    CHECK(names[5] == "option_quote");
    CHECK(names[6] == "option_underlying");
}

TEST_CASE("user_version 99 is refused")
{
    TempDb tmp;
    {
        terminal::Store store(tmp.path());
        CHECK(store.userVersion() == 3);
    }
    terminal::Store::testingSetUserVersion(tmp.path(), 99);
    CHECK_THROWS_AS(terminal::Store(tmp.path()), std::runtime_error);
}

TEST_CASE("user_version 1 missing tables is refused")
{
    TempDb tmp;
    terminal::Store::testingSetUserVersion(tmp.path(), 1);
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

TEST_CASE("embedded schema matches v1.sql")
{
    const std::filesystem::path sql_path =
        std::filesystem::path(TERMINAL_MARKET_DATA_SCHEMA_DIR) / "v1.sql";
    std::ifstream in(sql_path, std::ios::binary);
    REQUIRE(in);
    std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // MSVC drops CR from raw string literals. The SQL text is what must match.
    file.erase(std::remove(file.begin(), file.end(), '\r'), file.end());
    std::string embedded(terminal::schemaV1());
    embedded.erase(std::remove(embedded.begin(), embedded.end(), '\r'), embedded.end());
    CHECK(file == embedded);
}

TEST_CASE("user_version 2 missing tables is refused")
{
    TempDb tmp;
    terminal::Store::testingCreateSchemaV1(tmp.path());
    terminal::Store::testingSetUserVersion(tmp.path(), 2);
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

TEST_CASE("embedded schema matches v2.sql")
{
    const std::filesystem::path sql_path =
        std::filesystem::path(TERMINAL_MARKET_DATA_SCHEMA_DIR) / "v2.sql";
    std::ifstream in(sql_path, std::ios::binary);
    REQUIRE(in);
    std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    file.erase(std::remove(file.begin(), file.end(), '\r'), file.end());
    std::string embedded(terminal::schemaV2());
    embedded.erase(std::remove(embedded.begin(), embedded.end(), '\r'), embedded.end());
    CHECK(file == embedded);
}

TEST_CASE("embedded schema matches v3.sql")
{
    const std::filesystem::path sql_path =
        std::filesystem::path(TERMINAL_MARKET_DATA_SCHEMA_DIR) / "v3.sql";
    std::ifstream in(sql_path, std::ios::binary);
    REQUIRE(in);
    std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    file.erase(std::remove(file.begin(), file.end(), '\r'), file.end());
    std::string embedded(terminal::schemaV3());
    embedded.erase(std::remove(embedded.begin(), embedded.end(), '\r'), embedded.end());
    CHECK(file == embedded);
}
