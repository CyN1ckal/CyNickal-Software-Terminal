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

void ingestDaily(terminal::Store& store, terminal::InstrumentId id, terminal::SessionDate session_date)
{
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1d;
    bar.ts = terminal::usRthUtcWindow("America/New_York", session_date).start;
    bar.open = 10.0;
    bar.high = 11.0;
    bar.low = 9.0;
    bar.close = 10.5;
    bar.volume = 1000.0;
    const auto result = store.ingestDailyRange(std::vector<terminal::Bar>{bar}, id, session_date, session_date);
    CHECK(result.bars.written == 1);
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
    settings.intraday_session_count = 3;
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
    settings.intraday_session_count = 1;
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
    zero.intraday_session_count = 0;
    const auto a = terminal::loadChartBars(store, zero);
    CHECK(zero.intraday_session_count == 0);
    CHECK(a.status == terminal::ChartLoadStatus::Ready);
    CHECK(a.sessions_used == 1);

    terminal::CChartSettings huge;
    huge.symbol = "AAPL";
    huge.intraday_session_count = 9999;
    const auto b = terminal::loadChartBars(store, huge);
    CHECK(huge.intraday_session_count == 9999);
    CHECK(b.status == terminal::ChartLoadStatus::Ready);
}

TEST_CASE("loadChartBars Day1 reads stored daily bars")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestDaily(store, id, 20250113);
    ingestDaily(store, id, 20250114);
    ingestDaily(store, id, 20250116);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = terminal::ChartBarPeriod::Day1;
    settings.historical_session_count = 3;
    const auto result = terminal::loadChartBars(store, settings);
    REQUIRE(result.status == terminal::ChartLoadStatus::Ready);
    CHECK(result.sessions_used == 3);
    REQUIRE(result.bars.size() == 3);
    CHECK(result.bars.front().timeframe_s == terminal::kTimeframe1d);
    CHECK(result.bars.front().ts == terminal::usRthUtcWindow("America/New_York", 20250113).start);
    CHECK(result.bars.back().ts == terminal::usRthUtcWindow("America/New_York", 20250116).start);
}

TEST_CASE("loadChartBars Day1 prefers stored daily over 1m composite")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250115, 10);
    ingestDaily(store, id, 20250115);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = terminal::ChartBarPeriod::Day1;
    settings.historical_session_count = 1;
    const auto result = terminal::loadChartBars(store, settings);
    REQUIRE(result.status == terminal::ChartLoadStatus::Ready);
    REQUIRE(result.bars.size() == 1);
    CHECK(result.bars.front().timeframe_s == terminal::kTimeframe1d);
    CHECK(result.bars.front().volume == 1000.0);
}

TEST_CASE("loadChartBars Day1 does not composite 1m bars")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestRth(store, id, 20250115, 5);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = terminal::ChartBarPeriod::Day1;
    settings.historical_session_count = 1;
    const auto result = terminal::loadChartBars(store, settings);
    CHECK(result.status == terminal::ChartLoadStatus::Empty);
    CHECK(result.bars.empty());
}

TEST_CASE("chartMaxSessionCount is 2520 for daily")
{
    CHECK(terminal::chartMaxSessionCount(terminal::ChartBarPeriod::Day1) == 2520);
    CHECK(terminal::chartMaxSessionCount(terminal::ChartBarPeriod::Minute1) ==
          terminal::kChartMaxSessionCount);
}

TEST_CASE("chart session counts default to 2 weeks intraday and 5 years historical")
{
    terminal::CChartSettings settings;
    CHECK(settings.intraday_session_count == 14);
    CHECK(settings.historical_session_count == 1260);
    CHECK(terminal::chartSessionCount(settings) == 14);
    settings.period = terminal::ChartBarPeriod::Day1;
    CHECK(terminal::chartSessionCount(settings) == 1260);
    CHECK(settings.intraday_session_count == 14);
}

TEST_CASE("loadChartBars Day1 uses historical_session_count not intraday")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestDaily(store, id, 20250113);
    ingestDaily(store, id, 20250114);
    ingestDaily(store, id, 20250116);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = terminal::ChartBarPeriod::Day1;
    settings.intraday_session_count = 14;
    settings.historical_session_count = 1;
    const auto result = terminal::loadChartBars(store, settings);
    REQUIRE(result.status == terminal::ChartLoadStatus::Ready);
    CHECK(result.sessions_used == 1);
    CHECK(result.bars.size() == 1);
    CHECK(result.last_session == 20250116);
}

TEST_CASE("clampV1Limits clamps intraday and historical independently")
{
    terminal::CChartSettings settings;
    settings.intraday_session_count = 0;
    settings.historical_session_count = 99999;
    terminal::clampV1Limits(settings);
    CHECK(settings.intraday_session_count == 1);
    CHECK(settings.historical_session_count == terminal::kChartMaxHistoricalSessionCount);
    settings.intraday_session_count = 99999;
    settings.historical_session_count = 0;
    terminal::clampV1Limits(settings);
    CHECK(settings.intraday_session_count == terminal::kChartMaxIntradaySessionCount);
    CHECK(settings.historical_session_count == 1);
}

TEST_CASE("isStoreBusyError matches sqlite locked and busy")
{
    CHECK(terminal::isStoreBusyError("sqlite3_step: database is locked"));
    CHECK(terminal::isStoreBusyError("database is busy"));
    CHECK_FALSE(terminal::isStoreBusyError("constraint failed"));
}

TEST_CASE("chartDownloadRequest asks for a symbol that has no bars")
{
    constexpr terminal::SessionDate kToday = 20260921;
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::CChartSettings settings;
    settings.symbol = "qqq";
    settings.period = terminal::ChartBarPeriod::Minute15;
    const auto missing = terminal::chartDownloadRequest(store, settings, kToday);
    REQUIRE(missing.has_value());
    const terminal::ChartDownloadRequest request = missing.value_or(terminal::ChartDownloadRequest{});
    CHECK(request.symbol == "QQQ");
    CHECK(request.timeframe_s == terminal::kTimeframe1m);
    CHECK(request.to == kToday);

    const auto id = store.upsertInstrument(makeAapl());
    settings.symbol = "AAPL";
    const auto no_bars = terminal::chartDownloadRequest(store, settings, kToday);
    REQUIRE(no_bars.has_value());
    const terminal::ChartDownloadRequest no_bars_request = no_bars.value_or(terminal::ChartDownloadRequest{});
    CHECK(no_bars_request.timeframe_s == terminal::kTimeframe1m);

    ingestRth(store, id, 20250115, 2);
    CHECK_FALSE(terminal::chartDownloadRequest(store, settings, kToday).has_value());

    settings.period = terminal::ChartBarPeriod::Day1;
    const auto daily = terminal::chartDownloadRequest(store, settings, kToday);
    REQUIRE(daily.has_value());
    const terminal::ChartDownloadRequest daily_request = daily.value_or(terminal::ChartDownloadRequest{});
    CHECK(daily_request.symbol == "AAPL");
    CHECK(daily_request.timeframe_s == terminal::kTimeframe1d);
}

TEST_CASE("chartDownloadRequest uses daily bars for a daily chart and 1m for intraday")
{
    constexpr terminal::SessionDate kToday = 20260921;
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    ingestDaily(store, id, 20250116);

    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    settings.period = terminal::ChartBarPeriod::Day1;
    CHECK_FALSE(terminal::chartDownloadRequest(store, settings, kToday).has_value());

    settings.period = terminal::ChartBarPeriod::Minute15;
    const auto intraday = terminal::chartDownloadRequest(store, settings, kToday);
    REQUIRE(intraday.has_value());
    const terminal::ChartDownloadRequest intraday_request =
        intraday.value_or(terminal::ChartDownloadRequest{});
    CHECK(intraday_request.timeframe_s == terminal::kTimeframe1m);
}

TEST_CASE("chartDownloadRequest does not download an ambiguous symbol")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    store.upsertInstrument(makeAapl("NMS"));
    store.upsertInstrument(makeAapl("XNAS"));
    terminal::CChartSettings settings;
    settings.symbol = "AAPL";
    CHECK_FALSE(terminal::chartDownloadRequest(store, settings, 20260921).has_value());

    settings.symbol.clear();
    CHECK_FALSE(terminal::chartDownloadRequest(store, settings, 20260921).has_value());
}
