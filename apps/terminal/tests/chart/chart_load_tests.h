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

myapp::Instrument makeAapl(std::optional<std::string> exchange = std::string{"NMS"})
{
    myapp::Instrument inst;
    inst.symbol = "AAPL";
    inst.exchange = std::move(exchange);
    inst.timezone = "America/New_York";
    return inst;
}

myapp::Bar rthBar(myapp::InstrumentId id, myapp::UnixSeconds ts)
{
    myapp::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = myapp::kTimeframe1m;
    bar.ts = ts;
    bar.open = 10.0;
    bar.high = 11.0;
    bar.low = 9.0;
    bar.close = 10.5;
    bar.volume = 100.0;
    return bar;
}

std::vector<myapp::Bar> rthDay(myapp::InstrumentId id, myapp::UnixSeconds open_ts, int count)
{
    std::vector<myapp::Bar> bars;
    bars.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        bars.push_back(rthBar(id, open_ts + static_cast<myapp::UnixSeconds>(i) * 60));
    }
    return bars;
}

void ingestRth(myapp::Store& store, myapp::InstrumentId id, myapp::SessionDate session_date, int count)
{
    const std::string naive = myapp::formatSessionDate(session_date) + " 09:30";
    const auto open = myapp::naiveLocalToUtc("America/New_York", naive);
    REQUIRE(open.has_value());
    const auto result = store.ingestSession(
        rthDay(id, *open, count), id, myapp::kTimeframe1m, session_date, myapp::kUsRthExpected1m);
    CHECK(result.bars.written == count);
}

}  // namespace

TEST_CASE("loadChartBars empty symbol is unconfigured")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    myapp::CChartSettings settings;
    const auto result = myapp::loadChartBars(store, settings);
    CHECK(result.status == myapp::ChartLoadStatus::Unconfigured);
    CHECK(result.bars.empty());
}

TEST_CASE("loadChartBars trims symbol without mutating settings")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250115, 2);

    myapp::CChartSettings settings;
    settings.symbol = " AAPL ";
    const auto result = myapp::loadChartBars(store, settings);
    CHECK(settings.symbol == " AAPL ");
    CHECK(result.status == myapp::ChartLoadStatus::Ready);
    CHECK(result.bars.size() == 2);
}

TEST_CASE("loadChartBars unknown symbol")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    myapp::CChartSettings settings;
    settings.symbol = "ZZZZ";
    const auto result = myapp::loadChartBars(store, settings);
    CHECK(result.status == myapp::ChartLoadStatus::UnknownSymbol);
    CHECK(result.message == "unknown symbol ZZZZ");
}

TEST_CASE("loadChartBars ambiguous symbol fails closed")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    store.upsertInstrument(makeAapl("NMS"));
    store.upsertInstrument(makeAapl("XNAS"));
    myapp::CChartSettings settings;
    settings.symbol = "AAPL";
    const auto result = myapp::loadChartBars(store, settings);
    CHECK(result.status == myapp::ChartLoadStatus::AmbiguousSymbol);
    CHECK(result.message == "multiple instruments named AAPL");
}

TEST_CASE("loadChartBars instrument with no coverage is empty")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    store.upsertInstrument(makeAapl());
    myapp::CChartSettings settings;
    settings.symbol = "AAPL";
    const auto result = myapp::loadChartBars(store, settings);
    CHECK(result.status == myapp::ChartLoadStatus::Empty);
    CHECK(result.bars.empty());
}

TEST_CASE("loadChartBars skips 0-bar holiday and ranges traded sessions")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250113, 2);
    ingestRth(store, id, 20250114, 2);
    const auto holiday = store.ingestSession({}, id, myapp::kTimeframe1m, 20250115, 0);
    CHECK(holiday.coverage.status == myapp::CoverageStatus::Complete);
    CHECK(holiday.coverage.bar_count == 0);
    ingestRth(store, id, 20250116, 2);

    myapp::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.session_count = 3;
    const auto result = myapp::loadChartBars(store, settings);
    REQUIRE(result.status == myapp::ChartLoadStatus::Ready);
    CHECK(result.sessions_used == 3);
    CHECK(result.first_session == 20250113);
    CHECK(result.last_session == 20250116);
    CHECK(result.bars.size() == 6);

    const auto oldest = myapp::usRthUtcWindow("America/New_York", 20250113);
    const auto newest = myapp::usRthUtcWindow("America/New_York", 20250116);
    CHECK(result.ts_begin == oldest.start);
    CHECK(result.ts_end == newest.end);
}

TEST_CASE("loadChartBars collects error coverage rows with leftover bars")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250117, 2);
    myapp::CoverageDay error;
    error.instrument_id = id;
    error.timeframe_s = myapp::kTimeframe1m;
    error.session_date = 20250117;
    error.bar_count = 2;
    error.expected_count = myapp::kUsRthExpected1m;
    error.status = myapp::CoverageStatus::Error;
    error.ingested_at = 1;
    store.upsertCoverage(error);

    myapp::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.session_count = 1;
    const auto result = myapp::loadChartBars(store, settings);
    REQUIRE(result.status == myapp::ChartLoadStatus::Ready);
    CHECK(result.sessions_used == 1);
    CHECK(result.bars.size() == 2);
}

TEST_CASE("loadChartBars unsupported period does not throw")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    store.upsertInstrument(makeAapl());
    myapp::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = myapp::ChartBarPeriod::Minute5;
    const auto result = myapp::loadChartBars(store, settings);
    CHECK(result.status == myapp::ChartLoadStatus::Unsupported);
    CHECK(result.bars.empty());
}

TEST_CASE("loadChartBars clamps session_count locally")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250115, 2);

    myapp::CChartSettings zero;
    zero.symbol = "AAPL";
    zero.session_count = 0;
    const auto a = myapp::loadChartBars(store, zero);
    CHECK(zero.session_count == 0);
    CHECK(a.status == myapp::ChartLoadStatus::Ready);
    CHECK(a.sessions_used == 1);

    myapp::CChartSettings huge;
    huge.symbol = "AAPL";
    huge.session_count = 9999;
    const auto b = myapp::loadChartBars(store, huge);
    CHECK(huge.session_count == 9999);
    CHECK(b.status == myapp::ChartLoadStatus::Ready);
}

TEST_CASE("isStoreBusyError matches sqlite locked and busy")
{
    CHECK(myapp::isStoreBusyError("sqlite3_step: database is locked"));
    CHECK(myapp::isStoreBusyError("database is busy"));
    CHECK_FALSE(myapp::isStoreBusyError("constraint failed"));
}
