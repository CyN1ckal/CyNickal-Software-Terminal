// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "data/LedgerMarks.h"
#include "market_data/Store.h"

#include <vector>

namespace {

void seedClose(terminal::Store& store, terminal::InstrumentId id, int timeframe_s, terminal::UnixSeconds ts,
               double close, terminal::UnixSeconds ingested_at)
{
    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = timeframe_s;
    bar.ts = ts;
    bar.open = close;
    bar.high = close;
    bar.low = close;
    bar.close = close;
    bar.volume = 1;
    REQUIRE(store.upsertBars(std::vector<terminal::Bar>{bar}).written == 1);
    terminal::CoverageDay day;
    day.instrument_id = id;
    day.timeframe_s = timeframe_s;
    day.session_date = 20260924;
    day.first_ts = ts;
    day.last_ts = ts;
    day.bar_count = 1;
    day.status = terminal::CoverageStatus::Complete;
    day.ingested_at = ingested_at;
    store.upsertCoverage(day);
}

}  // namespace

TEST_CASE("a share mark is the newer of the daily and minute close")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    CHECK_FALSE(terminal::latestShareMark(store, id).has_value());

    seedClose(store, id, terminal::kTimeframe1d, 1'790'000'000, 200.0, 1'790'100'000);
    auto mark = terminal::latestShareMark(store, id);
    REQUIRE(mark.has_value());
    CHECK(mark->price == 200.0);
    CHECK(mark->as_of == 1'790'000'000);
    CHECK(mark->received_at == 1'790'100'000);

    seedClose(store, id, terminal::kTimeframe1m, 1'790'000'040, 201.5, 1'790'000'100);
    mark = terminal::latestShareMark(store, id);
    REQUIRE(mark.has_value());
    CHECK(mark->price == 201.5);
    CHECK(mark->as_of == 1'790'000'040);
}

TEST_CASE("an option mark is the stored chain's last print for that contract")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    terminal::OptionQuote quote;
    quote.instrument_id = id;
    quote.expiration = 20261016;
    quote.expiration_type = terminal::OptionExpirationType::Monthly;
    quote.vendor_symbol = "AAPL|20261016|200P";
    quote.strike = 200;
    quote.right = terminal::OptionRight::Put;
    quote.last = 4.35;
    quote.fetched_at = 1'790'000'000;
    terminal::OptionQuoteBatch batch;
    batch.expiration = quote.expiration;
    batch.expiration_type = quote.expiration_type;
    batch.quotes = {quote};
    terminal::OptionChainWrite write;
    write.instrument_id = id;
    write.fetched_at = quote.fetched_at;
    write.batches = {batch};
    store.replaceOptionChain(write);

    terminal::PositionKey key;
    key.instrument_id = id;
    key.kind = terminal::TradeAssetKind::Option;
    key.expiration = 20261016;
    key.expiration_type = terminal::OptionExpirationType::Monthly;
    key.strike = 200;
    key.right = terminal::OptionRight::Put;
    const auto mark = terminal::latestOptionMark(store, key);
    REQUIRE(mark.has_value());
    CHECK(mark->price == 4.35);
    CHECK(mark->received_at == 1'790'000'000);

    key.right = terminal::OptionRight::Call;
    CHECK_FALSE(terminal::latestOptionMark(store, key).has_value());
    key.kind = terminal::TradeAssetKind::Equity;
    CHECK_FALSE(terminal::latestOptionMark(store, key).has_value());
}

TEST_CASE("open ledger positions plan the same fetches as holdings")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spy = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    seedClose(store, spy, terminal::kTimeframe1d, 1'790'000'000, 500.0, 1'790'100'000);

    terminal::TradeFill buy;
    buy.kind = terminal::TradeAssetKind::Equity;
    buy.instrument_id = aapl;
    buy.symbol = "AAPL";
    buy.listing_open = true;
    buy.ts = 100;
    buy.quantity = 10;
    buy.price = 150;
    terminal::TradeFill etf = buy;
    etf.kind = terminal::TradeAssetKind::Etf;
    etf.instrument_id = spy;
    etf.symbol = "SPY";
    terminal::TradeFill put = buy;
    put.kind = terminal::TradeAssetKind::Option;
    put.expiration = 20261016;
    put.expiration_type = terminal::OptionExpirationType::Monthly;
    put.strike = 140;
    put.right = terminal::OptionRight::Put;
    put.quantity = 1;
    put.price = 2;
    const std::vector<terminal::TradeFill> fills{buy, etf, put};
    const auto book = terminal::matchLots(fills, {}, 1'000);
    REQUIRE(book.positions.size() == 3);

    const auto holdings = terminal::positionsAsHoldings(book.positions, fills);
    REQUIRE(holdings.size() == 3);
    CHECK(holdings[0].kind == terminal::PortfolioAssetKind::Equity);
    CHECK(holdings[0].symbol == "AAPL");
    CHECK(holdings[0].listing_open);
    CHECK(holdings[0].quantity == 10);
    CHECK(holdings[1].kind == terminal::PortfolioAssetKind::Option);
    CHECK(holdings[1].strike == 140);
    CHECK(holdings[1].right == terminal::OptionRight::Put);
    CHECK(holdings[2].kind == terminal::PortfolioAssetKind::Etf);

    // SPY already has a daily bar, so only AAPL's bars and AAPL's chain are missing.
    const auto missing = terminal::ledgerFetchJobs(store, book.positions, fills, 20260924);
    REQUIRE(missing.size() == 2);
    CHECK(missing[0].symbol == "AAPL");
    CHECK(missing[0].timeframe_s == terminal::kTimeframe1d);
    CHECK(missing[1].options);
    CHECK(missing[1].option_expiration == 20261016);
    const auto refresh = terminal::ledgerFetchJobs(store, book.positions, fills, 20260924, false);
    CHECK(refresh.size() == 3);
}
