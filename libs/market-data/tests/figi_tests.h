// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "FakeOpenFigi.h"
#include "catch_amalgamated.hpp"
#include "market_data/Figi.h"

#include <set>
#include <string>

TEST_CASE("FIGI check digit matches OpenFIGI's own values")
{
    CHECK(terminal::isValidFigi("BBG000BLNNH6"));  // IBM composite, OpenFIGI's mapping example
    CHECK(terminal::isValidFigi("BBG000B9XRY4"));  // AAPL
    CHECK(terminal::isValidFigi("BBG000MM2P62"));  // META
    CHECK(terminal::isValidFigi("BBG000BLNNV0"));  // IBM venue FIGI
    CHECK(terminal::isValidFigi("BBG000H4FSM0"));  // S&P 500 index
    CHECK(terminal::figiCheckDigit("BBG000BLNNH") == '6');
    CHECK_FALSE(terminal::figiCheckDigit("BBG000BLNN").has_value());
    CHECK_FALSE(terminal::figiCheckDigit("bbg000blnnh").has_value());
}

TEST_CASE("FIGI shape rejects each malformed form")
{
    CHECK_FALSE(terminal::isValidFigi("BBG000BLNNH7"));   // bad check digit
    CHECK_FALSE(terminal::isValidFigi("BBG000BLNAH6"));   // vowel
    CHECK_FALSE(terminal::isValidFigi("BSG000BLNNH6"));   // reserved prefix
    CHECK_FALSE(terminal::isValidFigi("BBX000BLNNH6"));   // position 3 is not G
    CHECK_FALSE(terminal::isValidFigi("bbg000blnnh6"));   // lowercase
    CHECK_FALSE(terminal::isValidFigi("BBG000BLNNH"));    // 11 characters
    CHECK_FALSE(terminal::isValidFigi("BBG000BLNNHX"));   // letter in the check position
    CHECK_FALSE(terminal::isFigiShape("BBG000BLNNH "));
}

TEST_CASE("every FIGI in the OpenFIGI fixtures passes the check digit")
{
    std::set<std::string> seen;
    for (const char* stem : {"forward_equity", "index_shapes", "reverse", "delisted_forward", "delisted_reverse"})
    {
        const nlohmann::json response = FakeOpenFigi::fixture(std::string(stem) + ".response.json");
        for (const nlohmann::json& element : response)
        {
            if (!element.contains("data"))
            {
                continue;
            }
            for (const nlohmann::json& hit : element["data"])
            {
                for (const char* key : {"figi", "compositeFIGI", "shareClassFIGI"})
                {
                    if (hit.contains(key) && hit[key].is_string())
                    {
                        const std::string figi = hit[key].get<std::string>();
                        INFO(figi);
                        CHECK(terminal::isValidFigi(figi));
                        seen.insert(figi);
                    }
                }
            }
        }
    }
    CHECK(seen.size() == 182);
}

TEST_CASE("testingFigiFor is stable, valid, and distinct")
{
    CHECK(terminal::testingFigiFor("AAPL") == terminal::testingFigiFor("AAPL"));
    CHECK(terminal::testingFigiFor("AAPL") != terminal::testingFigiFor("MSFT"));
    CHECK(terminal::isValidFigi(terminal::testingFigiFor("AAPL")));
    CHECK(terminal::isValidFigi(terminal::testingFigiFor("$SPX")));
    CHECK(terminal::testingFigiFor("AAPL").starts_with("ZZG"));
}
