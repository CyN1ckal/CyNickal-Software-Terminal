// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Store.h"
#include "market_data/Time.h"
#include "market_data/Types.h"

#include <stdexcept>
#include <vector>

namespace {

terminal::Bar rthBar(terminal::InstrumentId id, terminal::UnixSeconds ts)
{
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1m;
    bar.ts = ts;
    bar.open = 10.0;
    bar.high = 11.0;
    bar.low = 9.0;
    bar.close = 10.0;
    bar.volume = 100.0;
    return bar;
}

std::vector<terminal::Bar> rthDay(terminal::InstrumentId id, terminal::UnixSeconds open_ts, int count)
{
    std::vector<terminal::Bar> bars;
    bars.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        bars.push_back(rthBar(id, open_ts + static_cast<terminal::UnixSeconds>(i) * 60));
    }
    return bars;
}

}  // namespace

TEST_CASE("390 RTH bars are complete")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    const auto id = store.upsertInstrument(inst);
    const auto open = terminal::naiveLocalToUtc("America/New_York", "2025-01-15 09:30");
    REQUIRE(open.has_value());
    const auto result = store.ingestSession(
        rthDay(id, *open, 390), id, terminal::kTimeframe1m, 20250115, terminal::kUsRthExpected1m);
    CHECK(result.bars.written == 390);
    CHECK(result.coverage.status == terminal::CoverageStatus::Complete);
    CHECK(result.coverage.bar_count == 390);
    CHECK(store.queryIncompleteCoverage(id, terminal::kTimeframe1m).empty());
}

TEST_CASE("200 bars are partial and listed as a hole")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    const auto id = store.upsertInstrument(inst);
    const auto open = terminal::naiveLocalToUtc("America/New_York", "2025-01-15 09:30");
    REQUIRE(open.has_value());
    const auto result = store.ingestSession(
        rthDay(id, *open, 200), id, terminal::kTimeframe1m, 20250115, terminal::kUsRthExpected1m);
    CHECK(result.coverage.status == terminal::CoverageStatus::Partial);
    CHECK(result.coverage.bar_count == 200);
    const auto holes = store.queryIncompleteCoverage(id, terminal::kTimeframe1m);
    REQUIRE(holes.size() == 1);
    CHECK(holes[0].session_date == 20250115);
}

TEST_CASE("zero bars with expected 390 is missing")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    const auto id = store.upsertInstrument(inst);
    const auto result =
        store.ingestSession({}, id, terminal::kTimeframe1m, 20250115, terminal::kUsRthExpected1m);
    CHECK(result.coverage.status == terminal::CoverageStatus::Missing);
    CHECK(result.coverage.bar_count == 0);
}

TEST_CASE("holiday 0/0 is complete")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    const auto id = store.upsertInstrument(inst);
    const auto result = store.ingestSession({}, id, terminal::kTimeframe1m, 20250101, 0);
    CHECK(result.coverage.status == terminal::CoverageStatus::Complete);
    CHECK(result.coverage.bar_count == 0);
    CHECK(store.queryIncompleteCoverage(id, terminal::kTimeframe1m).empty());
}

TEST_CASE("session_still_open forces partial")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    const auto id = store.upsertInstrument(inst);
    const auto open = terminal::naiveLocalToUtc("America/New_York", "2025-01-15 09:30");
    REQUIRE(open.has_value());
    const auto result = store.ingestSession(
        rthDay(id, *open, 390), id, terminal::kTimeframe1m, 20250115, terminal::kUsRthExpected1m, true);
    CHECK(result.coverage.status == terminal::CoverageStatus::Partial);
}

TEST_CASE("previous session bar is rejected")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    const auto id = store.upsertInstrument(inst);
    const auto prior = terminal::naiveLocalToUtc("America/New_York", "2025-01-14 09:30");
    const auto open = terminal::naiveLocalToUtc("America/New_York", "2025-01-15 09:30");
    REQUIRE(prior.has_value());
    REQUIRE(open.has_value());
    auto bars = rthDay(id, *open, 1);
    bars.push_back(rthBar(id, *prior));
    const auto result =
        store.ingestSession(bars, id, terminal::kTimeframe1m, 20250115, terminal::kUsRthExpected1m);
    CHECK(result.bars.written == 1);
    CHECK(result.bars.rejected == 1);
    CHECK(store.queryBars(id, terminal::kTimeframe1m, *prior, *prior + 60).empty());
}

TEST_CASE("invalid session_date rolls back bars and coverage")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    const auto id = store.upsertInstrument(inst);
    const auto open = terminal::naiveLocalToUtc("America/New_York", "2025-01-15 09:30");
    REQUIRE(open.has_value());
    CHECK_THROWS_AS(store.ingestSession(rthDay(id, *open, 10),
                                        id,
                                        terminal::kTimeframe1m,
                                        20250231,
                                        terminal::kUsRthExpected1m),
                    std::runtime_error);
    CHECK(store.queryBars(id, terminal::kTimeframe1m, 0, 4000000000).empty());
    CHECK_FALSE(store.findCoverage(id, terminal::kTimeframe1m, 20250231).has_value());
    CHECK_FALSE(store.findCoverage(id, terminal::kTimeframe1m, 20250115).has_value());
}
