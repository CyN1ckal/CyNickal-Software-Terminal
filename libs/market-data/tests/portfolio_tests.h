// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Figi.h"
#include "market_data/Store.h"
#include "market_data/Types.h"

#include "../private/Sqlite.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

terminal::PortfolioHolding makeShare(terminal::PortfolioAssetKind kind, std::string figi, double quantity)
{
    terminal::PortfolioHolding holding;
    holding.kind = kind;
    holding.figi = std::move(figi);
    holding.quantity = quantity;
    return holding;
}

terminal::PortfolioHolding makeCash(double quantity)
{
    terminal::PortfolioHolding holding;
    holding.kind = terminal::PortfolioAssetKind::Cash;
    holding.quantity = quantity;
    return holding;
}

terminal::PortfolioHolding makeOption(std::string figi,
                                      terminal::SessionDate expiration,
                                      double strike,
                                      terminal::OptionRight right,
                                      double quantity)
{
    terminal::PortfolioHolding holding;
    holding.kind = terminal::PortfolioAssetKind::Option;
    holding.figi = std::move(figi);
    holding.expiration = expiration;
    holding.expiration_type = terminal::OptionExpirationType::Weekly;
    holding.strike = strike;
    holding.right = right;
    holding.quantity = quantity;
    return holding;
}

std::string figiOf(const terminal::Store& store, terminal::InstrumentId id)
{
    const auto instrument = store.findInstrumentById(id);
    REQUIRE(instrument.has_value());
    REQUIRE(instrument->figi.has_value());
    return *instrument->figi;
}

void replaceOne(terminal::Store& store, terminal::PortfolioId id, const terminal::PortfolioHolding& holding)
{
    const std::span<const terminal::PortfolioHolding> span(&holding, 1);
    store.replaceHoldings(id, span);
}

// instrument_listing is ON DELETE RESTRICT too. Drop it when the holding is the row under test.
void deleteListings(const std::filesystem::path& path, terminal::InstrumentId id)
{
    terminal::SqliteDb db(path);
    db.exec("PRAGMA foreign_keys = ON");
    terminal::SqliteStmt stmt(db.handle(), "DELETE FROM instrument_listing WHERE instrument_id = ?");
    stmt.bindInt64(1, id);
    stmt.stepDone();
    stmt.reset();
}

std::int64_t countInstrument(const std::filesystem::path& path, terminal::InstrumentId id)
{
    terminal::SqliteDb db(path);
    terminal::SqliteStmt stmt(db.handle(), "SELECT COUNT(*) FROM instrument WHERE id = ?");
    stmt.bindInt64(1, id);
    if (!stmt.stepRow())
    {
        throw std::runtime_error("COUNT returned no row");
    }
    const auto count = stmt.columnInt64(0);
    stmt.reset();
    return count;
}

}  // namespace

TEST_CASE("portfolios create, list, rename, and reject a duplicate name")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    CHECK_THROWS_WITH(store.createPortfolio(""), "portfolio name is empty");
    CHECK_THROWS_WITH(store.createPortfolio("  "), "portfolio name is empty");
    CHECK_THROWS_WITH(store.createPortfolio(" Alpha"), "portfolio name is empty");
    CHECK_THROWS_WITH(store.createPortfolio("Alpha "), "portfolio name is empty");

    const auto zeta = store.createPortfolio("zeta");
    const auto alpha = store.createPortfolio("Alpha");
    const auto beta = store.createPortfolio("beta");
    CHECK_THROWS_WITH(store.createPortfolio("ALPHA"), "portfolio name already exists");

    const auto listed = store.listPortfolios();
    REQUIRE(listed.size() == 3);
    CHECK(listed[0].id == alpha);
    CHECK(listed[0].name == "Alpha");
    CHECK(listed[1].id == beta);
    CHECK(listed[1].name == "beta");
    CHECK(listed[2].id == zeta);
    CHECK(listed[2].name == "zeta");
    CHECK(listed[0].created_at == listed[0].updated_at);
    CHECK(listed[0].updated_at >= listed[0].created_at);

    const auto created = listed[0].created_at;
    store.renamePortfolio(alpha, "alpha");
    const auto renamed = store.findPortfolio(alpha);
    REQUIRE(renamed.has_value());
    CHECK(renamed->name == "alpha");
    CHECK(renamed->created_at == created);
    CHECK(renamed->updated_at >= created);

    CHECK_THROWS_WITH(store.renamePortfolio(beta, "ALPHA"), "portfolio name already exists");
    const auto beta_row = store.findPortfolio(beta);
    REQUIRE(beta_row.has_value());
    CHECK(beta_row->name == "beta");
    CHECK_THROWS_WITH(store.renamePortfolio(alpha, " alpha"), "portfolio name is empty");
    CHECK_THROWS_WITH(store.renamePortfolio(999, "Other"), "portfolio not found");
    CHECK_THROWS_WITH(store.deletePortfolio(999), "portfolio not found");
    CHECK_THROWS_WITH(store.queryHoldings(999), "portfolio not found");
    const std::span<const terminal::PortfolioHolding> none;
    CHECK_THROWS_WITH(store.replaceHoldings(999, none), "portfolio not found");
    CHECK_FALSE(store.findPortfolio(999).has_value());
    CHECK(store.queryHoldings(alpha).empty());
}

TEST_CASE("replaceHoldings round-trips FIGI, symbol, and cash")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto equity_id = store.testingInsertInstrument("AAPL");
    const auto etf_id = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    const auto index_id = store.testingInsertInstrument("SPX", terminal::AssetClass::Index);
    const auto equity_figi = figiOf(store, equity_id);
    const auto etf_figi = figiOf(store, etf_id);
    const auto index_figi = figiOf(store, index_id);

    const auto book = store.createPortfolio("Book");
    const auto created = store.findPortfolio(book);
    REQUIRE(created.has_value());

    auto share = makeShare(terminal::PortfolioAssetKind::Equity, equity_figi, 10.5);
    share.instrument_id = 999999;
    share.symbol = "NOPE";
    share.listing_open = false;
    auto equity_option = makeOption(equity_figi, 20260918, 100.0, terminal::OptionRight::Call, 1);
    equity_option.vendor_symbol = "AAPL|20260918|100C";
    const auto etf = makeShare(terminal::PortfolioAssetKind::Etf, etf_figi, 2);
    const auto index_option = makeOption(index_figi, 20260925, 5000.0, terminal::OptionRight::Put, -3);
    const auto cash = makeCash(1000);
    const std::vector<terminal::PortfolioHolding> holdings = {share, equity_option, etf, index_option, cash};
    store.replaceHoldings(book, holdings);

    const auto after = store.findPortfolio(book);
    REQUIRE(after.has_value());
    CHECK(after->created_at == created->created_at);
    CHECK(after->updated_at >= created->created_at);
    CHECK(after->updated_at >= created->updated_at);

    store.replaceHoldings(book, holdings);
    const auto rows = store.queryHoldings(book);
    REQUIRE(rows.size() == 5);

    CHECK(rows[0].kind == terminal::PortfolioAssetKind::Cash);
    CHECK_FALSE(rows[0].figi.has_value());
    CHECK_FALSE(rows[0].instrument_id.has_value());
    CHECK_FALSE(rows[0].symbol.has_value());
    CHECK_FALSE(rows[0].listing_open);
    CHECK(rows[0].quantity == 1000);

    CHECK(rows[1].kind == terminal::PortfolioAssetKind::Equity);
    REQUIRE(rows[1].figi.has_value());
    CHECK(*rows[1].figi == equity_figi);
    REQUIRE(rows[1].instrument_id.has_value());
    CHECK(*rows[1].instrument_id == equity_id);
    REQUIRE(rows[1].symbol.has_value());
    CHECK(*rows[1].symbol == "AAPL");
    CHECK(rows[1].listing_open);
    CHECK_FALSE(rows[1].expiration.has_value());
    CHECK_FALSE(rows[1].vendor_symbol.has_value());
    CHECK(rows[1].quantity == 10.5);

    CHECK(rows[2].kind == terminal::PortfolioAssetKind::Etf);
    REQUIRE(rows[2].figi.has_value());
    CHECK(*rows[2].figi == etf_figi);
    REQUIRE(rows[2].instrument_id.has_value());
    CHECK(*rows[2].instrument_id == etf_id);
    REQUIRE(rows[2].symbol.has_value());
    CHECK(*rows[2].symbol == "SPY");
    CHECK(rows[2].quantity == 2);

    CHECK(rows[3].kind == terminal::PortfolioAssetKind::Option);
    REQUIRE(rows[3].figi.has_value());
    CHECK(*rows[3].figi == equity_figi);
    REQUIRE(rows[3].instrument_id.has_value());
    CHECK(*rows[3].instrument_id == equity_id);
    REQUIRE(rows[3].symbol.has_value());
    CHECK(*rows[3].symbol == "AAPL");
    CHECK(rows[3].expiration == 20260918);
    CHECK(rows[3].expiration_type == terminal::OptionExpirationType::Weekly);
    CHECK(rows[3].strike == 100.0);
    CHECK(rows[3].right == terminal::OptionRight::Call);
    REQUIRE(rows[3].vendor_symbol.has_value());
    CHECK(*rows[3].vendor_symbol == "AAPL|20260918|100C");
    CHECK(rows[3].quantity == 1);

    CHECK(rows[4].kind == terminal::PortfolioAssetKind::Option);
    REQUIRE(rows[4].figi.has_value());
    CHECK(*rows[4].figi == index_figi);
    REQUIRE(rows[4].instrument_id.has_value());
    CHECK(*rows[4].instrument_id == index_id);
    REQUIRE(rows[4].symbol.has_value());
    CHECK(*rows[4].symbol == "SPX");
    CHECK(rows[4].expiration == 20260925);
    CHECK(rows[4].strike == 5000.0);
    CHECK(rows[4].right == terminal::OptionRight::Put);
    CHECK_FALSE(rows[4].vendor_symbol.has_value());
    CHECK(rows[4].quantity == -3);

    const auto touched = store.findPortfolio(book);
    REQUIRE(touched.has_value());
    store.replaceHoldings(book, {});
    CHECK(store.queryHoldings(book).empty());
    const auto cleared = store.findPortfolio(book);
    REQUIRE(cleared.has_value());
    CHECK(cleared->created_at == created->created_at);
    CHECK(cleared->updated_at >= touched->updated_at);
}

TEST_CASE("replaceHoldings rejects a bad holding and keeps the book")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto book = store.createPortfolio("Book");
    const auto equity_id = store.testingInsertInstrument("AAPL");
    const auto etf_id = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    const auto future_id = store.testingInsertInstrument("ES", terminal::AssetClass::Future);
    const auto equity_figi = figiOf(store, equity_id);
    const auto etf_figi = figiOf(store, etf_id);
    const auto future_figi = figiOf(store, future_id);
    const auto kept = makeShare(terminal::PortfolioAssetKind::Equity, equity_figi, 7);
    replaceOne(store, book, kept);

    const auto expect_kept = [&]() {
        const auto rows = store.queryHoldings(book);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].quantity == 7);
    };

    terminal::PortfolioHolding missing;
    missing.kind = terminal::PortfolioAssetKind::Equity;
    missing.quantity = 1;
    CHECK_THROWS_WITH(replaceOne(store, book, missing), "portfolio holding figi is missing");
    expect_kept();

    CHECK_THROWS_WITH(replaceOne(store, book, makeShare(terminal::PortfolioAssetKind::Equity, "BBG000BLNNH7", 1)),
                      "portfolio holding figi is invalid");
    expect_kept();

    CHECK_THROWS_WITH(
        replaceOne(store, book, makeShare(terminal::PortfolioAssetKind::Equity, terminal::testingFigiFor("missing"), 1)),
        "portfolio holding figi was not found");
    expect_kept();

    CHECK_THROWS_WITH(replaceOne(store, book, makeShare(terminal::PortfolioAssetKind::Equity, etf_figi, 1)),
                      "portfolio holding kind does not match its instrument");
    expect_kept();

    CHECK_THROWS_WITH(replaceOne(store, book, makeShare(terminal::PortfolioAssetKind::Equity, future_figi, 1)),
                      "portfolio holding kind does not match its instrument");
    CHECK_THROWS_WITH(
        replaceOne(store, book, makeOption(future_figi, 20260925, 100.0, terminal::OptionRight::Call, 1)),
        "portfolio holding kind does not match its instrument");
    expect_kept();

    CHECK_THROWS_WITH(replaceOne(store, book, makeShare(terminal::PortfolioAssetKind::Equity, equity_figi, 0)),
                      "portfolio holding quantity is zero");
    CHECK_THROWS_WITH(replaceOne(store,
                                 book,
                                 makeShare(terminal::PortfolioAssetKind::Equity,
                                           equity_figi,
                                           std::numeric_limits<double>::quiet_NaN())),
                      "portfolio holding quantity is not finite");
    expect_kept();

    const std::vector<terminal::PortfolioHolding> dup_shares = {kept, kept};
    CHECK_THROWS_WITH(store.replaceHoldings(book, dup_shares), "duplicate portfolio holding");
    const std::vector<terminal::PortfolioHolding> dup_cash = {makeCash(1), makeCash(-2)};
    CHECK_THROWS_WITH(store.replaceHoldings(book, dup_cash), "duplicate portfolio holding");
    auto option_a = makeOption(equity_figi, 20260925, 100.0, terminal::OptionRight::Call, 1);
    auto option_b = option_a;
    option_b.quantity = 2;
    option_b.vendor_symbol = "OTHER";
    const std::vector<terminal::PortfolioHolding> dup_options = {option_a, option_b};
    CHECK_THROWS_WITH(store.replaceHoldings(book, dup_options), "duplicate portfolio holding");
    option_a.vendor_symbol = "SAME";
    option_b = makeOption(equity_figi, 20260925, 110.0, terminal::OptionRight::Call, 1);
    option_b.vendor_symbol = "SAME";
    const std::vector<terminal::PortfolioHolding> dup_vendor = {option_a, option_b};
    CHECK_THROWS_WITH(store.replaceHoldings(book, dup_vendor), "duplicate portfolio holding");
    expect_kept();

    auto struck = kept;
    struck.strike = 10;
    CHECK_THROWS_WITH(replaceOne(store, book, struck), "portfolio holding kind does not match its fields");
    auto cash_figi = makeCash(5);
    cash_figi.figi = equity_figi;
    CHECK_THROWS_WITH(replaceOne(store, book, cash_figi), "portfolio holding kind does not match its fields");
    auto partial = makeOption(equity_figi, 20260925, 100.0, terminal::OptionRight::Call, 1);
    partial.right.reset();
    CHECK_THROWS_WITH(replaceOne(store, book, partial), "portfolio holding kind does not match its fields");
    CHECK_THROWS_WITH(replaceOne(store, book, makeOption(equity_figi, 20260231, 100.0, terminal::OptionRight::Call, 1)),
                      "portfolio option identity is invalid");
    CHECK_THROWS_WITH(replaceOne(store, book, makeOption(equity_figi, 20260925, 0, terminal::OptionRight::Call, 1)),
                      "portfolio option identity is invalid");
    auto bad_vendor = makeOption(equity_figi, 20260925, 100.0, terminal::OptionRight::Call, 1);
    bad_vendor.vendor_symbol = " SYM";
    CHECK_THROWS_WITH(replaceOne(store, book, bad_vendor), "portfolio option identity is invalid");
    expect_kept();
}

TEST_CASE("relinkSymbol leaves the holding on the FIGI")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    terminal::Instrument fb;
    fb.symbol = "FB";
    fb.figi = "BBG000MM2P62";
    const auto id = store.insertInstrument(fb, 100);
    const auto book = store.createPortfolio("Meta");
    const auto holding = makeShare(terminal::PortfolioAssetKind::Equity, "BBG000MM2P62", 4);
    replaceOne(store, book, holding);

    store.relinkSymbol(id, "META", 200);
    const auto rows = store.queryHoldings(book);
    REQUIRE(rows.size() == 1);
    REQUIRE(rows[0].instrument_id.has_value());
    CHECK(*rows[0].instrument_id == id);
    REQUIRE(rows[0].figi.has_value());
    CHECK(*rows[0].figi == "BBG000MM2P62");
    REQUIRE(rows[0].symbol.has_value());
    CHECK(*rows[0].symbol == "META");
    CHECK(rows[0].listing_open);
    CHECK(rows[0].quantity == 4);
}

TEST_CASE("testingDeleteInstrument is restricted while a holding exists")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    const auto book = store.createPortfolio("Book");
    replaceOne(store, book, makeShare(terminal::PortfolioAssetKind::Equity, figiOf(store, id), 1));
    deleteListings(tmp.path(), id);

    CHECK_THROWS_AS(terminal::Store::testingDeleteInstrument(tmp.path(), id), std::runtime_error);
    const auto still = store.queryHoldings(book);
    REQUIRE(still.size() == 1);
    CHECK(still[0].quantity == 1);
    CHECK(countInstrument(tmp.path(), id) == 1);

    store.replaceHoldings(book, {});
    terminal::Store::testingDeleteInstrument(tmp.path(), id);
    CHECK(countInstrument(tmp.path(), id) == 0);
}

TEST_CASE("vendor writes do not change holding quantity")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    const auto book = store.createPortfolio("Book");
    replaceOne(store, book, makeShare(terminal::PortfolioAssetKind::Equity, figiOf(store, id), 12.5));

    terminal::Bar bar;
    bar.instrument_id = id;
    bar.timeframe_s = terminal::kTimeframe1d;
    bar.ts = 1'600'000'000;
    bar.open = 10;
    bar.high = 11;
    bar.low = 9;
    bar.close = 10.5;
    bar.volume = 100;
    REQUIRE(store.upsertBars(std::vector<terminal::Bar>{bar}).written == 1);

    terminal::StatementSnapshot snapshot;
    snapshot.instrument_id = id;
    snapshot.statement = terminal::StatementKind::Income;
    snapshot.timeframe = terminal::StatementTimeframe::Annually;
    snapshot.fetched_at = 10;
    terminal::StatementCell cell;
    cell.instrument_id = id;
    cell.statement = terminal::StatementKind::Income;
    cell.timeframe = terminal::StatementTimeframe::Annually;
    cell.line_item = "revenue";
    cell.period_end = "2025-09-27";
    cell.value = std::int64_t{1};
    store.replaceStatement(snapshot, std::span<const terminal::StatementCell>(&cell, 1));

    terminal::OptionQuote quote;
    quote.instrument_id = id;
    quote.expiration = 20260925;
    quote.expiration_type = terminal::OptionExpirationType::Weekly;
    quote.vendor_symbol = "AAPL|20260925|110C";
    quote.strike = 110;
    quote.right = terminal::OptionRight::Call;
    terminal::OptionQuoteBatch batch;
    batch.expiration = 20260925;
    batch.expiration_type = terminal::OptionExpirationType::Weekly;
    batch.quotes = {quote};
    terminal::OptionChainWrite write;
    write.instrument_id = id;
    write.fetched_at = 10;
    write.batches = {batch};
    store.replaceOptionChain(write);

    const auto rows = store.queryHoldings(book);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].quantity == 12.5);
    CHECK(store.queryBars(id, terminal::kTimeframe1d, bar.ts, bar.ts + 1).size() == 1);
    CHECK(store.findStatementSnapshot(id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually)
              .has_value());
    CHECK(store.queryOptionQuotes(id, 20260925, terminal::OptionExpirationType::Weekly).size() == 1);
}

TEST_CASE("deletePortfolio cascades only that book")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto equity_id = store.testingInsertInstrument("AAPL");
    const auto etf_id = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    const auto alpha = store.createPortfolio("Alpha");
    const auto beta = store.createPortfolio("Beta");
    const auto created = store.findPortfolio(alpha);
    REQUIRE(created.has_value());
    replaceOne(store, alpha, makeShare(terminal::PortfolioAssetKind::Equity, figiOf(store, equity_id), 3));
    replaceOne(store, beta, makeShare(terminal::PortfolioAssetKind::Etf, figiOf(store, etf_id), 8));

    const auto replaced = store.findPortfolio(alpha);
    REQUIRE(replaced.has_value());
    CHECK(replaced->created_at == created->created_at);
    CHECK(replaced->updated_at >= created->created_at);

    store.deletePortfolio(alpha);
    CHECK_THROWS_WITH(store.queryHoldings(alpha), "portfolio not found");
    const auto left = store.queryHoldings(beta);
    REQUIRE(left.size() == 1);
    REQUIRE(left[0].instrument_id.has_value());
    CHECK(*left[0].instrument_id == etf_id);
    CHECK(left[0].quantity == 8);
    const auto books = store.listPortfolios();
    REQUIRE(books.size() == 1);
    CHECK(books[0].id == beta);

    deleteListings(tmp.path(), equity_id);
    terminal::Store::testingDeleteInstrument(tmp.path(), equity_id);
    CHECK(countInstrument(tmp.path(), equity_id) == 0);
    deleteListings(tmp.path(), etf_id);
    CHECK_THROWS_AS(terminal::Store::testingDeleteInstrument(tmp.path(), etf_id), std::runtime_error);
    CHECK(store.queryHoldings(beta).size() == 1);
}
