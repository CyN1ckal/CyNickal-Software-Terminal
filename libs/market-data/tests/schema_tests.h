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
    myapp::Store store(tmp.path());
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
        myapp::Store first(tmp.path());
        CHECK(first.userVersion() == 1);
    }
    myapp::Store second(tmp.path());
    CHECK(second.userVersion() == 1);
    CHECK(second.tableNames().size() == 4);
}

TEST_CASE("user_version 99 is refused")
{
    TempDb tmp;
    {
        myapp::Store store(tmp.path());
        CHECK(store.userVersion() == 1);
    }
    myapp::Store::testingSetUserVersion(tmp.path(), 99);
    CHECK_THROWS_AS(myapp::Store(tmp.path()), std::runtime_error);
}

TEST_CASE("embedded schema matches v1.sql")
{
    const std::filesystem::path sql_path =
        std::filesystem::path(MYAPP_MARKET_DATA_SCHEMA_DIR) / "v1.sql";
    std::ifstream in(sql_path, std::ios::binary);
    REQUIRE(in);
    const std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(file == myapp::schemaV1());
}
