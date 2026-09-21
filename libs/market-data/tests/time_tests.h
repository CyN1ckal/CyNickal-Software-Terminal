// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "market_data/Time.h"

#include <chrono>
#include <stdexcept>

TEST_CASE("naive 2025-05-21 14:40 New York is 18:40 UTC")
{
    const auto ts = terminal::naiveLocalToUtc("America/New_York", "2025-05-21 14:40");
    REQUIRE(ts.has_value());
    const std::chrono::sys_seconds utc{std::chrono::seconds{*ts}};
    const auto tod = std::chrono::hh_mm_ss{utc - std::chrono::floor<std::chrono::days>(utc)};
    CHECK(tod.hours().count() == 18);
    CHECK(tod.minutes().count() == 40);
}

TEST_CASE("spring DST gap is nullopt; missing zone throws")
{
    CHECK_FALSE(terminal::naiveLocalToUtc("America/New_York", "2025-03-09 02:30").has_value());
    CHECK_THROWS_AS(terminal::naiveLocalToUtc("", "2025-03-09 09:30"), std::runtime_error);
    CHECK_THROWS_AS(terminal::naiveLocalToUtc("Not/AZone", "2025-03-09 09:30"), std::runtime_error);
}

TEST_CASE("fall overlap uses earliest (EDT)")
{
    const auto ts = terminal::naiveLocalToUtc("America/New_York", "2025-11-02 01:30");
    REQUIRE(ts.has_value());
    const std::chrono::sys_seconds utc{std::chrono::seconds{*ts}};
    const auto tod = std::chrono::hh_mm_ss{utc - std::chrono::floor<std::chrono::days>(utc)};
    CHECK(tod.hours().count() == 5);
    CHECK(tod.minutes().count() == 30);
}

TEST_CASE("session window is 23h or 25h around DST")
{
    const auto spring = terminal::sessionUtcWindow("America/New_York", 20250309);
    CHECK(spring.end - spring.start == 23 * 3600);
    const auto fall = terminal::sessionUtcWindow("America/New_York", 20251102);
    CHECK(fall.end - fall.start == 25 * 3600);
}

TEST_CASE("invalid civil session_date throws")
{
    CHECK_THROWS_AS(terminal::sessionUtcWindow("America/New_York", 20250231), std::runtime_error);
}

TEST_CASE("parseSessionDate accepts YYYYMMDD and YYYY-MM-DD")
{
    CHECK(terminal::parseSessionDate("20250115") == 20250115);
    CHECK(terminal::parseSessionDate("2025-01-15") == 20250115);
    CHECK(terminal::formatSessionDate(20250115) == "2025-01-15");
    CHECK_THROWS_AS(terminal::parseSessionDate("2025-02-31"), std::runtime_error);
    CHECK_THROWS_AS(terminal::parseSessionDate("15"), std::runtime_error);
}

TEST_CASE("isUsRthLocal is [09:30, 16:00)")
{
    using namespace std::chrono;
    CHECK(terminal::isUsRthLocal(hh_mm_ss<seconds>{hours{9} + minutes{30}}));
    CHECK(terminal::isUsRthLocal(hh_mm_ss<seconds>{hours{15} + minutes{59}}));
    CHECK_FALSE(terminal::isUsRthLocal(hh_mm_ss<seconds>{hours{9} + minutes{29}}));
    CHECK_FALSE(terminal::isUsRthLocal(hh_mm_ss<seconds>{hours{16}}));
}

TEST_CASE("usRthUtcWindow is 09:30-16:00 local")
{
    const auto open = terminal::naiveLocalToUtc("America/New_York", "2025-01-15 09:30");
    const auto close = terminal::naiveLocalToUtc("America/New_York", "2025-01-15 16:00");
    REQUIRE(open.has_value());
    REQUIRE(close.has_value());
    const auto rth = terminal::usRthUtcWindow("America/New_York", 20250115);
    CHECK(rth.start == *open);
    CHECK(rth.end == *close);
    CHECK(terminal::isUsRthAt("America/New_York", *open));
    CHECK_FALSE(terminal::isUsRthAt("America/New_York", *close));
}

TEST_CASE("money and US date parsers")
{
    CHECK(terminal::parseMoneyAmount("$0.050") == 0.05);
    CHECK(terminal::parseMoneyAmount("1,234.56") == 1234.56);
    CHECK_FALSE(terminal::parseMoneyAmount("").has_value());
    const auto ex = terminal::parseUsDateToUtcMidnight("09/27/24");
    REQUIRE(ex.has_value());
    CHECK(terminal::parseRfc3339Utc("2020-08-31T04:00:00.000Z").has_value());
    CHECK_FALSE(terminal::parseRfc3339Utc("nope").has_value());
}
