// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "chart/CChartLoad.h"
#include "market_data/Store.h"
#include "market_data/Time.h"
#include "market_data/Types.h"

#include <optional>
#include <string>
#include <vector>

namespace {

terminal::Instrument makeAapl(std::optional<std::string> exchange = std::string{"NMS"})
{
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    inst.exchange = std::move(exchange);
    inst.timezone = "America/New_York";
    return inst;
}

terminal::Bar rthBar(terminal::InstrumentId id, terminal::UnixSeconds ts)
{
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1m;
    bar.ts = ts;
    bar.open = 10.0;
    bar.high = 11.0;
    bar.low = 9.0;
    bar.close = 10.5;
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

void ingestRth(terminal::Store& store, terminal::InstrumentId id, terminal::SessionDate session_date, int count)
{
    const std::string naive = terminal::formatSessionDate(session_date) + " 09:30";
    const auto open = terminal::naiveLocalToUtc("America/New_York", naive);
    REQUIRE(open.has_value());
    const auto result = store.ingestSession(
        rthDay(id, *open, count), id, terminal::kTimeframe1m, session_date, terminal::kUsRthExpected1m);
    CHECK(result.bars.written == count);
}

}  // namespace

TEST_CASE("loadChartBars empty symbol is unconfigured")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::CChartSettings settings;
    const auto result = terminal::loadChartBars(store, settings);
    CHECK(result.status == terminal::ChartLoadStatus::Unconfigured);
    CHECK(result.bars.empty());
}

TEST_CASE("loadChartBars trims symbol without mutating settings")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250115, 2);

    terminal::CChartSettings settings;
    settings.symbol = " AAPL ";
    const auto result = terminal::loadChartBars(store, settings);
    CHECK(settings.symbol == " AAPL ");
    CHECK(result.status == terminal::ChartLoadStatus::Ready);
    CHECK(result.bars.size() == 2);
}

TEST_CASE("loadChartBars unknown symbol")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::CChartSettings settings;
    settings.symbol = "ZZZZ";
    const auto result = terminal::loadChartBars(store, settings);
    CHECK(result.status == terminal::ChartLoadStatus::UnknownSymbol);
    CHECK(result.message == "unknown symbol ZZZZ");
}

TEST_CASE("loadChartBars ambiguous symbol fails closed")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    store.upsertInstrument(makeAapl("NMS"));
    store.upsertInstrument(makeAapl("XNAS"));
    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    const auto result = terminal::loadChartBars(store, settings);
    CHECK(result.status == terminal::ChartLoadStatus::AmbiguousSymbol);
    CHECK(result.message == "multiple instruments named AAPL");
}

TEST_CASE("loadChartBars instrument with no coverage is empty")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    store.upsertInstrument(makeAapl());
    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    const auto result = terminal::loadChartBars(store, settings);
    CHECK(result.status == terminal::ChartLoadStatus::Empty);
    CHECK(result.bars.empty());
}

TEST_CASE("loadChartBars skips 0-bar holiday and ranges traded sessions")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250113, 2);
    ingestRth(store, id, 20250114, 2);
    const auto holiday = store.ingestSession({}, id, terminal::kTimeframe1m, 20250115, 0);
    CHECK(holiday.coverage.status == terminal::CoverageStatus::Complete);
    CHECK(holiday.coverage.bar_count == 0);
    ingestRth(store, id, 20250116, 2);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.session_count = 3;
    const auto result = terminal::loadChartBars(store, settings);
    REQUIRE(result.status == terminal::ChartLoadStatus::Ready);
    CHECK(result.sessions_used == 3);
    CHECK(result.first_session == 20250113);
    CHECK(result.last_session == 20250116);
    CHECK(result.bars.size() == 6);

    const auto oldest = terminal::usRthUtcWindow("America/New_York", 20250113);
    const auto newest = terminal::usRthUtcWindow("America/New_York", 20250116);
    CHECK(result.ts_begin == oldest.start);
    CHECK(result.ts_end == newest.end);
}

TEST_CASE("loadChartBars collects error coverage rows with leftover bars")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250117, 2);
    terminal::CoverageDay error;
    error.instrument_id = id;
    error.timeframe_s = terminal::kTimeframe1m;
    error.session_date = 20250117;
    error.bar_count = 2;
    error.expected_count = terminal::kUsRthExpected1m;
    error.status = terminal::CoverageStatus::Error;
    error.ingested_at = 1;
    store.upsertCoverage(error);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.session_count = 1;
    const auto result = terminal::loadChartBars(store, settings);
    REQUIRE(result.status == terminal::ChartLoadStatus::Ready);
    CHECK(result.sessions_used == 1);
    CHECK(result.bars.size() == 2);
}

TEST_CASE("loadChartBars unsupported bar type does not throw")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    store.upsertInstrument(makeAapl());
    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.bar_type = terminal::ChartBarType::Ohlc;
    const auto result = terminal::loadChartBars(store, settings);
    CHECK(result.status == terminal::ChartLoadStatus::Unsupported);
    CHECK(result.bars.empty());
}

TEST_CASE("loadChartBars clamps session_count locally")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250115, 2);

    terminal::CChartSettings zero;
    zero.symbol = "AAPL";
    zero.session_count = 0;
    const auto a = terminal::loadChartBars(store, zero);
    CHECK(zero.session_count == 0);
    CHECK(a.status == terminal::ChartLoadStatus::Ready);
    CHECK(a.sessions_used == 1);

    terminal::CChartSettings huge;
    huge.symbol = "AAPL";
    huge.session_count = 9999;
    const auto b = terminal::loadChartBars(store, huge);
    CHECK(huge.session_count == 9999);
    CHECK(b.status == terminal::ChartLoadStatus::Ready);
}

TEST_CASE("isStoreBusyError matches sqlite locked and busy")
{
    CHECK(terminal::isStoreBusyError("sqlite3_step: database is locked"));
    CHECK(terminal::isStoreBusyError("database is busy"));
    CHECK_FALSE(terminal::isStoreBusyError("constraint failed"));
}
