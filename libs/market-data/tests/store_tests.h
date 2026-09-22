// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Store.h"
#include "market_data/Time.h"
#include "market_data/Types.h"

#include <chrono>
#include <stdexcept>
#include <vector>

namespace {

terminal::Instrument makeAapl(std::optional<std::string> exchange = std::string{"NMS"})
{
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    inst.exchange = std::move(exchange);
    inst.name = "Apple";
    return inst;
}

terminal::Bar makeBar(terminal::InstrumentId id, terminal::UnixSeconds ts, double close = 10.0)
{
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1m;
    bar.ts = ts;
    bar.open = close;
    bar.high = close + 1.0;
    bar.low = close - 1.0;
    bar.close = close;
    bar.volume = 1000.0;
    return bar;
}

terminal::UnixSeconds alignedNowMinus(int minutes)
{
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    const auto ts = now - static_cast<terminal::UnixSeconds>(minutes) * 60;
    return ts - (ts % 60);
}

}  // namespace

TEST_CASE("upsertInstrument inserts then updates name")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id1 = store.upsertInstrument(makeAapl());
    CHECK(id1 > 0);
    auto found = store.findInstrument("aapl", "NMS");
    REQUIRE(found.has_value());
    CHECK(found->id == id1);
    CHECK(found->name == "Apple");

    auto again = makeAapl();
    again.name = "Apple Inc";
    const auto id2 = store.upsertInstrument(again);
    CHECK(id2 == id1);
    found = store.findInstrumentById(id1);
    REQUIRE(found.has_value());
    CHECK(found->name == "Apple Inc");
}

TEST_CASE("NULL exchange and NMS are distinct; empty matches NULL")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id_null = store.upsertInstrument(makeAapl(std::nullopt));
    const auto id_nms = store.upsertInstrument(makeAapl("NMS"));
    CHECK(id_null != id_nms);

    auto empty = makeAapl("");
    const auto id_empty = store.upsertInstrument(empty);
    CHECK(id_empty == id_null);

    CHECK(store.findInstrument("AAPL", std::nullopt)->id == id_null);
    CHECK(store.findInstrument("AAPL", "")->id == id_null);
    CHECK(store.findInstrument("AAPL", "NMS")->id == id_nms);
    const auto both = store.findInstrumentsBySymbol("AAPL");
    REQUIRE(both.size() == 2);
    CHECK(both[0].id == id_null);
    CHECK(both[1].id == id_nms);
}

TEST_CASE("upsertBars last write wins on close")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    const auto ts = alignedNowMinus(5);
    CHECK(store.upsertBars(std::vector<terminal::Bar>{makeBar(id, ts, 10.0)}).written == 1);
    CHECK(store.upsertBars(std::vector<terminal::Bar>{makeBar(id, ts, 12.5)}).written == 1);
    const auto rows = store.queryBars(id, terminal::kTimeframe1m, ts, ts + 60);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].close == 12.5);
}

TEST_CASE("invalid OHLC is rejected")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    auto bar = makeBar(id, alignedNowMinus(5));
    bar.high = 1.0;
    bar.low = 9.0;
    const auto result = store.upsertBars(std::vector<terminal::Bar>{bar});
    CHECK(result.written == 0);
    CHECK(result.rejected == 1);
    CHECK(store.queryBars(id, terminal::kTimeframe1m, 0, 4000000000).empty());
}

TEST_CASE("forming minute is skipped not rejected")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    const auto ts = now - (now % 60);
    const auto result = store.upsertBars(std::vector<terminal::Bar>{makeBar(id, ts)});
    CHECK(result.written == 0);
    CHECK(result.rejected == 0);
    CHECK(store.queryBars(id, terminal::kTimeframe1m, 0, 4000000000).empty());
}

TEST_CASE("queryBars is ascending and exclusive-end")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    const auto t0 = alignedNowMinus(10);
    CHECK(store.upsertBars(std::vector<terminal::Bar>{makeBar(id, t0, 1), makeBar(id, t0 + 60, 2),
                                                   makeBar(id, t0 + 120, 3)})
              .written == 3);
    const auto rows = store.queryBars(id, terminal::kTimeframe1m, t0, t0 + 120);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].ts == t0);
    CHECK(rows[1].ts == t0 + 60);
}

TEST_CASE("FK failure rolls back the whole upsertBars call")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    const auto ts = alignedNowMinus(5);
    const auto good = makeBar(id, ts);
    const auto bad = makeBar(999, ts + 60);
    CHECK_THROWS_AS(store.upsertBars(std::vector<terminal::Bar>{good, bad}), std::runtime_error);
    CHECK(store.queryBars(id, terminal::kTimeframe1m, 0, 4000000000).empty());
}

TEST_CASE("timezone change after bars throws")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto inst = makeAapl();
    const auto id = store.upsertInstrument(inst);
    CHECK(store.upsertBars(std::vector<terminal::Bar>{makeBar(id, alignedNowMinus(5))}).written == 1);
    inst.timezone = "America/Chicago";
    CHECK_THROWS_AS(store.upsertInstrument(inst), std::runtime_error);
    CHECK(store.findInstrumentById(id)->timezone == "America/New_York");
}

TEST_CASE("corporate_action SELECT-merge keeps identity")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    terminal::CorporateAction split;
    split.instrument_id = id;
    split.ex_ts = 1598846400;
    split.type = terminal::CorporateActionType::Split;
    split.split_ratio = 4.0;
    split.source = "mboum";
    store.upsertCorporateAction(split);
    split.currency = "USD";
    store.upsertCorporateAction(split);
    const auto rows = store.queryCorporateActions(id, 0, 2000000000);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].split_ratio == 4.0);
    CHECK(rows[0].currency == "USD");
}

TEST_CASE("queryCoverageSummaries includes names with no coverage")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    auto aapl = makeAapl();
    const auto aapl_id = store.upsertInstrument(aapl);
    terminal::Instrument msft;
    msft.symbol = "MSFT";
    store.upsertInstrument(msft);

    terminal::CoverageDay complete;
    complete.instrument_id = aapl_id;
    complete.timeframe_s = terminal::kTimeframe1m;
    complete.session_date = 20250115;
    complete.bar_count = 390;
    complete.expected_count = 390;
    complete.status = terminal::CoverageStatus::Complete;
    complete.ingested_at = 1;
    store.upsertCoverage(complete);

    terminal::CoverageDay partial = complete;
    partial.session_date = 20250116;
    partial.bar_count = 200;
    partial.status = terminal::CoverageStatus::Partial;
    partial.ingested_at = 2;
    store.upsertCoverage(partial);

    const auto rows = store.queryCoverageSummaries(terminal::kTimeframe1m);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].instrument.symbol == "AAPL");
    CHECK(rows[0].bar_count == 590);
    CHECK(rows[0].session_count == 2);
    CHECK(rows[0].complete_count == 1);
    CHECK(rows[0].partial_count == 1);
    CHECK(rows[0].first_session == 20250115);
    CHECK(rows[0].last_session == 20250116);
    CHECK(rows[0].last_ingested_at == 2);
    CHECK(rows[1].instrument.symbol == "MSFT");
    CHECK(rows[1].session_count == 0);
    CHECK(rows[1].bar_count == 0);

    const auto days = store.queryCoverageDays(aapl_id, terminal::kTimeframe1m);
    REQUIRE(days.size() == 2);
    CHECK(days[0].session_date == 20250116);
    CHECK(days[1].session_date == 20250115);
    CHECK(store.queryCoverageDays(rows[1].instrument.id, terminal::kTimeframe1m).empty());
}

TEST_CASE("ingestDailyRange writes 86400 bars and holiday coverage")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1d;
    bar.ts = terminal::usRthUtcWindow("America/New_York", 20250115).start;
    bar.open = 10;
    bar.high = 11;
    bar.low = 9;
    bar.close = 10;
    bar.volume = 100;
    const auto result = store.ingestDailyRange(std::vector<terminal::Bar>{bar}, id, 20250101, 20250115);
    CHECK(result.bars.written == 1);
    const auto daily = store.queryBars(id, terminal::kTimeframe1d, 0, 4000000000);
    REQUIRE(daily.size() == 1);
    CHECK(daily.front().timeframe_s == terminal::kTimeframe1d);
    CHECK(store.queryBars(id, terminal::kTimeframe1m, 0, 4000000000).empty());

    const auto holiday = store.findCoverage(id, terminal::kTimeframe1d, 20250101);
    REQUIRE(holiday.has_value());
    const terminal::CoverageDay holiday_row = *holiday;
    CHECK(holiday_row.status == terminal::CoverageStatus::Complete);
    CHECK(holiday_row.bar_count == 0);
    CHECK(holiday_row.expected_count == 0);

    const auto session = store.findCoverage(id, terminal::kTimeframe1d, 20250115);
    REQUIRE(session.has_value());
    const terminal::CoverageDay session_row = *session;
    CHECK(session_row.status == terminal::CoverageStatus::Complete);
    CHECK(session_row.bar_count == 1);
    CHECK(store.findCoverage(id, terminal::kTimeframe1m, 20250115).has_value() == false);
}

TEST_CASE("ingestDailyRange rejects the wrong instrument")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.upsertInstrument(makeAapl());
    terminal::Instrument msft;
    msft.symbol = "MSFT";
    const auto msft_id = store.upsertInstrument(msft);
    terminal::Bar bar;
    bar.instrument_id = msft_id;
    bar.timeframe_s = terminal::kTimeframe1d;
    bar.ts = terminal::usRthUtcWindow("America/New_York", 20250115).start;
    bar.open = 1;
    bar.high = 1;
    bar.low = 1;
    bar.close = 1;
    bar.volume = 1;
    const auto result = store.ingestDailyRange(std::vector<terminal::Bar>{bar}, aapl, 20250115, 20250115);
    CHECK(result.bars.written == 0);
    CHECK(result.bars.rejected == 1);
    CHECK(store.queryBars(aapl, terminal::kTimeframe1d, 0, 4000000000).empty());
}
