#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Store.h"
#include "market_data/Types.h"

#include <chrono>
#include <stdexcept>
#include <vector>

namespace {

myapp::Instrument makeAapl(std::optional<std::string> exchange = std::string{"NMS"})
{
    myapp::Instrument inst;
    inst.symbol = "AAPL";
    inst.exchange = std::move(exchange);
    inst.name = "Apple";
    return inst;
}

myapp::Bar makeBar(myapp::InstrumentId id, myapp::UnixSeconds ts, double close = 10.0)
{
    myapp::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = myapp::kTimeframe1m;
    bar.ts = ts;
    bar.open = close;
    bar.high = close + 1.0;
    bar.low = close - 1.0;
    bar.close = close;
    bar.volume = 1000.0;
    return bar;
}

myapp::UnixSeconds alignedNowMinus(int minutes)
{
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    const auto ts = now - static_cast<myapp::UnixSeconds>(minutes) * 60;
    return ts - (ts % 60);
}

}  // namespace

TEST_CASE("upsertInstrument inserts then updates name")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
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
    myapp::Store store(tmp.path());
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
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    const auto ts = alignedNowMinus(5);
    CHECK(store.upsertBars(std::vector<myapp::Bar>{makeBar(id, ts, 10.0)}).written == 1);
    CHECK(store.upsertBars(std::vector<myapp::Bar>{makeBar(id, ts, 12.5)}).written == 1);
    const auto rows = store.queryBars(id, myapp::kTimeframe1m, ts, ts + 60);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].close == 12.5);
}

TEST_CASE("invalid OHLC is rejected")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    auto bar = makeBar(id, alignedNowMinus(5));
    bar.high = 1.0;
    bar.low = 9.0;
    const auto result = store.upsertBars(std::vector<myapp::Bar>{bar});
    CHECK(result.written == 0);
    CHECK(result.rejected == 1);
    CHECK(store.queryBars(id, myapp::kTimeframe1m, 0, 4000000000).empty());
}

TEST_CASE("forming minute is skipped not rejected")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    const auto ts = now - (now % 60);
    const auto result = store.upsertBars(std::vector<myapp::Bar>{makeBar(id, ts)});
    CHECK(result.written == 0);
    CHECK(result.rejected == 0);
    CHECK(store.queryBars(id, myapp::kTimeframe1m, 0, 4000000000).empty());
}

TEST_CASE("queryBars is ascending and exclusive-end")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    const auto t0 = alignedNowMinus(10);
    CHECK(store.upsertBars(std::vector<myapp::Bar>{makeBar(id, t0, 1), makeBar(id, t0 + 60, 2),
                                                   makeBar(id, t0 + 120, 3)})
              .written == 3);
    const auto rows = store.queryBars(id, myapp::kTimeframe1m, t0, t0 + 120);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].ts == t0);
    CHECK(rows[1].ts == t0 + 60);
}

TEST_CASE("FK failure rolls back the whole upsertBars call")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    const auto ts = alignedNowMinus(5);
    const auto good = makeBar(id, ts);
    const auto bad = makeBar(999, ts + 60);
    CHECK_THROWS_AS(store.upsertBars(std::vector<myapp::Bar>{good, bad}), std::runtime_error);
    CHECK(store.queryBars(id, myapp::kTimeframe1m, 0, 4000000000).empty());
}

TEST_CASE("timezone change after bars throws")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    auto inst = makeAapl();
    const auto id = store.upsertInstrument(inst);
    CHECK(store.upsertBars(std::vector<myapp::Bar>{makeBar(id, alignedNowMinus(5))}).written == 1);
    inst.timezone = "America/Chicago";
    CHECK_THROWS_AS(store.upsertInstrument(inst), std::runtime_error);
    CHECK(store.findInstrumentById(id)->timezone == "America/New_York");
}

TEST_CASE("corporate_action SELECT-merge keeps identity")
{
    TempDb tmp;
    myapp::Store store(tmp.path());
    const auto id = store.upsertInstrument(makeAapl());
    myapp::CorporateAction split;
    split.instrument_id = id;
    split.ex_ts = 1598846400;
    split.type = myapp::CorporateActionType::Split;
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
