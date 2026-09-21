#pragma once

#include "catch_amalgamated.hpp"
#include "market_data/Time.h"

#include <chrono>
#include <stdexcept>

TEST_CASE("naive 2025-05-21 14:40 New York is 18:40 UTC")
{
    const auto ts = myapp::naiveLocalToUtc("America/New_York", "2025-05-21 14:40");
    REQUIRE(ts.has_value());
    const std::chrono::sys_seconds utc{std::chrono::seconds{*ts}};
    const auto tod = std::chrono::hh_mm_ss{utc - std::chrono::floor<std::chrono::days>(utc)};
    CHECK(tod.hours().count() == 18);
    CHECK(tod.minutes().count() == 40);
}

TEST_CASE("spring DST gap is nullopt; missing zone throws")
{
    CHECK_FALSE(myapp::naiveLocalToUtc("America/New_York", "2025-03-09 02:30").has_value());
    CHECK_THROWS_AS(myapp::naiveLocalToUtc("", "2025-03-09 09:30"), std::runtime_error);
    CHECK_THROWS_AS(myapp::naiveLocalToUtc("Not/AZone", "2025-03-09 09:30"), std::runtime_error);
}

TEST_CASE("fall overlap uses earliest (EDT)")
{
    const auto ts = myapp::naiveLocalToUtc("America/New_York", "2025-11-02 01:30");
    REQUIRE(ts.has_value());
    const std::chrono::sys_seconds utc{std::chrono::seconds{*ts}};
    const auto tod = std::chrono::hh_mm_ss{utc - std::chrono::floor<std::chrono::days>(utc)};
    CHECK(tod.hours().count() == 5);
    CHECK(tod.minutes().count() == 30);
}

TEST_CASE("session window is 23h or 25h around DST")
{
    const auto spring = myapp::sessionUtcWindow("America/New_York", 20250309);
    CHECK(spring.end - spring.start == 23 * 3600);
    const auto fall = myapp::sessionUtcWindow("America/New_York", 20251102);
    CHECK(fall.end - fall.start == 25 * 3600);
}

TEST_CASE("invalid civil session_date throws")
{
    CHECK_THROWS_AS(myapp::sessionUtcWindow("America/New_York", 20250231), std::runtime_error);
}

TEST_CASE("isUsRthLocal is [09:30, 16:00)")
{
    using namespace std::chrono;
    CHECK(myapp::isUsRthLocal(hh_mm_ss<seconds>{hours{9} + minutes{30}}));
    CHECK(myapp::isUsRthLocal(hh_mm_ss<seconds>{hours{15} + minutes{59}}));
    CHECK_FALSE(myapp::isUsRthLocal(hh_mm_ss<seconds>{hours{9} + minutes{29}}));
    CHECK_FALSE(myapp::isUsRthLocal(hh_mm_ss<seconds>{hours{16}}));
}

TEST_CASE("money and US date parsers")
{
    CHECK(myapp::parseMoneyAmount("$0.050") == 0.05);
    CHECK(myapp::parseMoneyAmount("1,234.56") == 1234.56);
    CHECK_FALSE(myapp::parseMoneyAmount("").has_value());
    const auto ex = myapp::parseUsDateToUtcMidnight("09/27/24");
    REQUIRE(ex.has_value());
    CHECK(myapp::parseRfc3339Utc("2020-08-31T04:00:00.000Z").has_value());
    CHECK_FALSE(myapp::parseRfc3339Utc("nope").has_value());
}
