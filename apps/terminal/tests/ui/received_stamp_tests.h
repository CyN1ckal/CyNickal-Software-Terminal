// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "market_data/Time.h"
#include "ui/ReceivedStamp.h"

TEST_CASE("received stamp is a UTC date and time")
{
    CHECK(terminal::formatReceivedUtc(0) == "1970-01-01 00:00:00 UTC");
    const auto ts = terminal::parseRfc3339Utc("2020-08-31T04:00:05.000Z");
    REQUIRE(ts.has_value());
    CHECK(terminal::formatReceivedUtc(*ts) == "2020-08-31 04:00:05 UTC");
}
