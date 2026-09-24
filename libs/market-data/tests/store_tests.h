// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Figi.h"
#include "market_data/Store.h"
#include "market_data/Time.h"
#include "market_data/Types.h"

#include <chrono>
#include <stdexcept>
#include <vector>

namespace {

terminal::Instrument makeAapl()
{
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    inst.figi = "BBG000B9XRY4";
    inst.name = "Apple";
    return inst;
}

terminal::Instrument makeStoreInstrument(std::string symbol, std::string figi)
{
    terminal::Instrument inst;
    inst.symbol = std::move(symbol);
    inst.figi = std::move(figi);
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

TEST_CASE("insertInstrument stores the FIGI and opens one listing")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeAapl(), 1000);
    CHECK(id > 0);
    const auto found = store.findOpenListing("aapl");
    REQUIRE(found.has_value());
    CHECK(found->id == id);
    CHECK(found->symbol == "AAPL");
    CHECK(found->figi == "BBG000B9XRY4");
    CHECK(found->name == "Apple");
    CHECK(found->listing_open);
    CHECK(found->created_at == 1000);
    CHECK(store.findInstrumentByFigi("BBG000B9XRY4")->id == id);
    const auto history = store.listingHistory(id);
    REQUIRE(history.size() == 1);
    CHECK(history[0].opened_at == 1000);
    CHECK_FALSE(history[0].closed_at.has_value());
}

TEST_CASE("insertInstrument rejects a second FIGI and a second open symbol")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    (void)store.insertInstrument(makeAapl());
    CHECK_THROWS_AS(store.insertInstrument(makeStoreInstrument("AAPL2", "BBG000B9XRY4")), std::runtime_error);
    CHECK_THROWS_AS(store.insertInstrument(makeStoreInstrument("aapl", terminal::testingFigiFor("x"))),
                    std::runtime_error);
    CHECK(store.listInstruments().size() == 1);
}

TEST_CASE("insertInstrument rejects a bad FIGI and a missing one")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    CHECK_THROWS_AS(store.insertInstrument(makeStoreInstrument("IBM", "BBG000BLNNH7")), std::runtime_error);
    CHECK_THROWS_AS(store.insertInstrument(makeStoreInstrument("IBM", "BBG000BLNAH6")), std::runtime_error);
    terminal::Instrument no_figi;
    no_figi.symbol = "IBM";
    CHECK_THROWS_AS(store.insertInstrument(no_figi), std::runtime_error);
    no_figi.asset_class = terminal::AssetClass::Index;
    CHECK_THROWS_AS(store.insertInstrument(no_figi), std::runtime_error);
    terminal::Instrument crypto;
    crypto.symbol = "BTC-USD";
    crypto.asset_class = terminal::AssetClass::Crypto;
    const auto id = store.insertInstrument(crypto);
    CHECK_FALSE(store.findInstrumentById(id)->figi.has_value());
    CHECK(store.listInstruments().size() == 1);
}

TEST_CASE("relinkSymbol keeps the id and the bars")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeStoreInstrument("FB", "BBG000MM2P62"), 100);
    const auto ts = alignedNowMinus(5);
    REQUIRE(store.upsertBars(std::vector<terminal::Bar>{makeBar(id, ts)}).written == 1);

    store.relinkSymbol(id, "META", 200);
    CHECK(store.queryBars(id, terminal::kTimeframe1m, ts, ts + 60).size() == 1);
    CHECK_FALSE(store.findOpenListing("FB").has_value());
    const auto by_old = store.resolveSymbol("FB");
    REQUIRE(by_old.has_value());
    CHECK(by_old->id == id);
    CHECK(by_old->symbol == "META");
    CHECK(by_old->listing_open);
    CHECK(store.findOpenListing("META")->id == id);

    const auto history = store.listingHistory(id);
    REQUIRE(history.size() == 2);
    CHECK(history[0].symbol == "FB");
    CHECK(history[0].closed_at == 200);
    CHECK(history[0].close_reason == terminal::ListingCloseReason::Renamed);
    CHECK(history[1].symbol == "META");
    CHECK_FALSE(history[1].closed_at.has_value());
}

TEST_CASE("a closed ticker can open on a new instrument")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto old_id = store.insertInstrument(makeStoreInstrument("TWTR", "BBG000H6HNW3"), 100);
    store.closeListing(old_id, 200, terminal::ListingCloseReason::Delisted);
    const auto retired = store.findInstrumentById(old_id);
    REQUIRE(retired.has_value());
    CHECK_FALSE(retired->listing_open);
    CHECK(retired->symbol == "TWTR");
    CHECK(retired->delisted_at == 200);
    CHECK(store.resolveSymbol("TWTR")->id == old_id);

    const auto new_id = store.insertInstrument(makeStoreInstrument("TWTR", terminal::testingFigiFor("new")), 300);
    CHECK(new_id != old_id);
    CHECK(store.resolveSymbol("TWTR")->id == new_id);
    CHECK(store.findOpenListing("TWTR")->id == new_id);
    CHECK(store.findInstrumentByFigi("BBG000H6HNW3")->id == old_id);
    const auto closed = store.latestClosedListing("TWTR");
    REQUIRE(closed.has_value());
    CHECK(closed->instrument_id == old_id);
    CHECK(closed->close_reason == terminal::ListingCloseReason::Delisted);
}

TEST_CASE("two relinks in the same second both succeed")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeStoreInstrument("AAA", terminal::testingFigiFor("AAA")), 500);
    store.relinkSymbol(id, "BBB", 500);
    store.relinkSymbol(id, "CCC", 500);
    CHECK(store.findOpenListing("CCC")->id == id);
    CHECK(store.listingHistory(id).size() == 3);
}

TEST_CASE("openListing refuses a symbol that is open elsewhere")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto a = store.testingInsertInstrument("AAA");
    const auto b = store.testingInsertInstrument("BBB");
    store.closeListing(b, 10, terminal::ListingCloseReason::Manual);
    CHECK_THROWS_AS(store.openListing(b, "aaa", 20), std::runtime_error);
    CHECK_THROWS_AS(store.openListing(a, "CCC", 20), std::runtime_error);
    store.openListing(b, "CCC", 20);
    CHECK(store.findOpenListing("CCC")->id == b);
}

TEST_CASE("attachFigi fills only a NULL FIGI")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument crypto;
    crypto.symbol = "BTC-USD";
    crypto.asset_class = terminal::AssetClass::Crypto;
    const auto id = store.insertInstrument(crypto);
    const auto figi = terminal::testingFigiFor("BTC");
    store.attachFigi(id, figi);
    store.attachFigi(id, figi);
    CHECK(store.findInstrumentById(id)->figi == figi);
    CHECK_THROWS_AS(store.attachFigi(id, terminal::testingFigiFor("ETH")), std::runtime_error);
    const auto aapl = store.insertInstrument(makeAapl());
    CHECK_THROWS_AS(store.attachFigi(aapl, terminal::testingFigiFor("ETH")), std::runtime_error);
    terminal::Instrument other;
    other.symbol = "ETH-USD";
    other.asset_class = terminal::AssetClass::Crypto;
    const auto eth = store.insertInstrument(other);
    CHECK_THROWS_AS(store.attachFigi(eth, figi), std::runtime_error);
    CHECK_THROWS_AS(store.attachFigi(eth, "BBG000BLNNH7"), std::runtime_error);
}

TEST_CASE("applyListingChanges closes before opening and reports a blocked open")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto a = store.testingInsertInstrument("AAA");
    const auto b = store.testingInsertInstrument("BBB");
    const auto c = store.testingInsertInstrument("CCC");
    using Kind = terminal::ListingChange::Kind;
    const std::vector<terminal::ListingChange> swap = {
        {.kind = Kind::Close, .instrument_id = a, .reason = terminal::ListingCloseReason::Renamed, .symbol = {}},
        {.kind = Kind::Close, .instrument_id = b, .reason = terminal::ListingCloseReason::Renamed, .symbol = {}},
        {.kind = Kind::Open, .instrument_id = a, .reason = {}, .symbol = "BBB"},
        {.kind = Kind::Open, .instrument_id = b, .reason = {}, .symbol = "CCC"},
    };
    const auto skipped = store.applyListingChanges(swap, 50);
    REQUIRE(skipped.size() == 1);
    CHECK(skipped[0] == 3);
    CHECK(store.findOpenListing("BBB")->id == a);
    CHECK(store.findOpenListing("CCC")->id == c);
    CHECK_FALSE(store.findInstrumentById(b)->listing_open);
    CHECK_FALSE(store.findInstrumentById(b)->verified_at.has_value());
}

TEST_CASE("upsertBars last write wins on close")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeAapl());
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
    const auto id = store.insertInstrument(makeAapl());
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
    const auto id = store.insertInstrument(makeAapl());
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
    const auto id = store.insertInstrument(makeAapl());
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
    const auto id = store.insertInstrument(makeAapl());
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
    const auto id = store.insertInstrument(inst);
    inst.name = "Apple Inc";
    store.updateDescriptive(id, inst);
    CHECK(store.findInstrumentById(id)->name == "Apple Inc");
    CHECK(store.upsertBars(std::vector<terminal::Bar>{makeBar(id, alignedNowMinus(5))}).written == 1);
    inst.timezone = "America/Chicago";
    CHECK_THROWS_AS(store.updateDescriptive(id, inst), std::runtime_error);
    CHECK(store.findInstrumentById(id)->timezone == "America/New_York");
}

TEST_CASE("corporate_action SELECT-merge keeps identity")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeAapl());
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
    const auto aapl_id = store.insertInstrument(makeAapl());
    (void)store.testingInsertInstrument("MSFT");

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
    const auto id = store.insertInstrument(makeAapl());
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
    const auto aapl = store.insertInstrument(makeAapl());
    const auto msft_id = store.testingInsertInstrument("MSFT");
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

TEST_CASE("listing symbols are uppercased on insert, relink, and reopen")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeStoreInstrument("meta", "BBG000MM2P62"));
    CHECK(store.findInstrumentById(id)->symbol == "META");
    store.relinkSymbol(id, "fb", 100);
    CHECK(store.findInstrumentById(id)->symbol == "FB");
    store.closeListing(id, 200, terminal::ListingCloseReason::Manual);
    store.openListing(id, "brk/b", 300);
    CHECK(store.findInstrumentById(id)->symbol == "BRK.B");
    CHECK(terminal::canonicalListingSymbol("$spx") == "$SPX");
    CHECK(terminal::canonicalListingSymbol("brk.b") == "BRK.B");
}

TEST_CASE("reopening a delisted instrument clears delisted_at")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeStoreInstrument("TWTR", "BBG000H6HNW3"), 100);
    store.closeListing(id, 200, terminal::ListingCloseReason::Delisted);
    CHECK(store.findInstrumentById(id)->delisted_at == 200);
    store.openListing(id, "TWTR", 300);
    const auto row = store.findInstrumentById(id);
    CHECK(row->listing_open);
    CHECK_FALSE(row->delisted_at.has_value());
}
