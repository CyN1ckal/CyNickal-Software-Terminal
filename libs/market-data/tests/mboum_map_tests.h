// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "market_data/MboumMap.h"
#include "market_data/Time.h"

TEST_CASE("mapV3Bar converts naive NY datetime")
{
    terminal::Instrument inst;
    inst.id = 1;
    inst.timezone = "America/New_York";
    terminal::MboumV3BarRow row;
    row.datetime = "2025-05-21 14:40";
    row.open = 201.98;
    row.high = 202.36;
    row.low = 201.95;
    row.close = 202.18;
    row.volume = 282557;
    const auto now = terminal::naiveLocalToUtc("America/New_York", "2025-05-22 12:00");
    REQUIRE(now.has_value());
    const auto bar = terminal::mapV3Bar(inst, row, *now);
    REQUIRE(bar.has_value());
    CHECK(bar->ts == *terminal::naiveLocalToUtc("America/New_York", "2025-05-21 14:40"));
    CHECK(bar->close == 202.18);
}

TEST_CASE("mapV3Bar forming and non-RTH are nullopt")
{
    terminal::Instrument inst;
    inst.id = 1;
    inst.timezone = "America/New_York";
    terminal::MboumV3BarRow row;
    row.datetime = "2025-05-21 14:40";
    row.open = 1;
    row.high = 1;
    row.low = 1;
    row.close = 1;
    row.volume = 1;
    const auto ts = terminal::naiveLocalToUtc("America/New_York", "2025-05-21 14:40");
    REQUIRE(ts.has_value());
    CHECK_FALSE(terminal::mapV3Bar(inst, row, *ts).has_value());

    row.datetime = "2025-05-21 16:00";
    const auto later = terminal::naiveLocalToUtc("America/New_York", "2025-05-22 12:00");
    REQUIRE(later.has_value());
    CHECK_FALSE(terminal::mapV3Bar(inst, row, *later).has_value());
}

TEST_CASE("mapV2Bar prefers timestamp_unix")
{
    terminal::Instrument inst;
    inst.id = 1;
    inst.timezone = "America/New_York";
    const auto unix_ts = terminal::naiveLocalToUtc("America/New_York", "2025-06-04 15:50");
    REQUIRE(unix_ts.has_value());
    terminal::MboumV2BarRow row;
    row.timestamp = "not-a-time";
    row.timestamp_unix = unix_ts;
    row.open = 203.09;
    row.high = 203.09;
    row.low = 202.79;
    row.close = 202.84;
    row.volume = 185027;
    const auto now = *unix_ts + 86400;
    const auto bar = terminal::mapV2Bar(inst, row, now);
    REQUIRE(bar.has_value());
    CHECK(bar->ts == *unix_ts);
}

TEST_CASE("mapSplit 4-for-1 and zero old_share_worth")
{
    terminal::MboumSplitRow row;
    row.startdatetime = "2020-08-31T04:00:00.000Z";
    row.old_share_worth = 1;
    row.share_worth = 4;
    const auto split = terminal::mapSplit(7, row);
    REQUIRE(split.has_value());
    CHECK(split->split_ratio == 4.0);
    CHECK(split->type == terminal::CorporateActionType::Split);
    row.old_share_worth = 0;
    CHECK_FALSE(terminal::mapSplit(7, row).has_value());
}

TEST_CASE("mapDividend parses $0.050")
{
    terminal::MboumDividendRow row;
    row.ex_date = "09/27/24";
    row.amount = "$0.050";
    const auto div = terminal::mapDividend(3, row);
    REQUIRE(div.has_value());
    CHECK(div->amount == 0.05);
    CHECK(div->type == terminal::CorporateActionType::Dividend);
}
