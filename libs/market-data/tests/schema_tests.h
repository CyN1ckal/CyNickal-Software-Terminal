// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Schema.h"
#include "market_data/Store.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

TEST_CASE("open empty path applies schema v1")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    CHECK(store.userVersion() == 1);
    CHECK(store.foreignKeysEnabled());
    const auto names = store.tableNames();
    REQUIRE(names.size() == 4);
    CHECK(names[0] == "bar");
    CHECK(names[1] == "corporate_action");
    CHECK(names[2] == "coverage_day");
    CHECK(names[3] == "instrument");
}

TEST_CASE("second open is a no-op migrate")
{
    TempDb tmp;
    {
        terminal::Store first(tmp.path());
        CHECK(first.userVersion() == 1);
    }
    terminal::Store second(tmp.path());
    CHECK(second.userVersion() == 1);
    CHECK(second.tableNames().size() == 4);
}

TEST_CASE("user_version 99 is refused")
{
    TempDb tmp;
    {
        terminal::Store store(tmp.path());
        CHECK(store.userVersion() == 1);
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
    const std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(file == terminal::schemaV1());
}
