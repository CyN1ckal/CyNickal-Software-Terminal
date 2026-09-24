// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "IngestDefaults.h"
#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "data/PortfolioFetch.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include <chrono>
#include <string>
#include <vector>

namespace {

constexpr terminal::SessionDate kFetchToday = 20260924;

[[nodiscard]] terminal::SessionDate dailyFetchFrom(terminal::SessionDate today)
{
    const auto ymd = terminal::sessionDateToYmd(today);
    return terminal::toSessionDate(std::chrono::sys_days{ymd} -
                                   std::chrono::days{terminal::kIngestDefaultDailyDays});
}

std::string figiOf(const terminal::Store& store, terminal::InstrumentId id)
{
    const auto instrument = store.findInstrumentById(id);
    REQUIRE(instrument.has_value());
    REQUIRE(instrument->figi.has_value());
    return *instrument->figi;
}

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
                                      terminal::OptionExpirationType expiration_type,
                                      double strike,
                                      terminal::OptionRight right,
                                      double quantity)
{
    terminal::PortfolioHolding holding;
    holding.kind = terminal::PortfolioAssetKind::Option;
    holding.figi = std::move(figi);
    holding.expiration = expiration;
    holding.expiration_type = expiration_type;
    holding.strike = strike;
    holding.right = right;
    holding.quantity = quantity;
    return holding;
}

void writeCoverage(terminal::Store& store, terminal::InstrumentId id, int timeframe_s, int bar_count)
{
    terminal::CoverageDay day;
    day.instrument_id = id;
    day.timeframe_s = timeframe_s;
    day.session_date = 20260923;
    day.bar_count = bar_count;
    day.status = bar_count > 0 ? terminal::CoverageStatus::Complete : terminal::CoverageStatus::Missing;
    day.ingested_at = 1;
    store.upsertCoverage(day);
}

terminal::OptionQuote makeQuote(terminal::InstrumentId id,
                                terminal::SessionDate expiration,
                                terminal::OptionExpirationType type,
                                double strike,
                                terminal::OptionRight right)
{
    terminal::OptionQuote quote;
    quote.instrument_id = id;
    quote.expiration = expiration;
    quote.expiration_type = type;
    quote.strike = strike;
    quote.right = right;
    quote.vendor_symbol = std::string(type == terminal::OptionExpirationType::Weekly ? "W" : "M") +
                          std::to_string(expiration) + "-" + std::to_string(static_cast<int>(strike)) +
                          (right == terminal::OptionRight::Call ? "C" : "P");
    quote.bid = 1.0;
    quote.ask = 1.2;
    quote.mid = 1.1;
    quote.last = 1.1;
    quote.implied_vol = 0.2;
    quote.days_to_expiration = 1;
    return quote;
}

void replaceQuotes(terminal::Store& store,
                   terminal::InstrumentId id,
                   terminal::SessionDate expiration,
                   terminal::OptionExpirationType type,
                   std::vector<terminal::OptionQuote> quotes)
{
    terminal::OptionChainWrite write;
    write.instrument_id = id;
    write.fetched_at = 1;
    terminal::OptionQuoteBatch batch;
    batch.expiration = expiration;
    batch.expiration_type = type;
    batch.quotes = std::move(quotes);
    write.batches.push_back(std::move(batch));
    store.replaceOptionChain(write);
}

void checkDailyJob(const terminal::IngestWorker::Job& job, std::string_view symbol)
{
    CHECK(job.symbol == symbol);
    CHECK(job.timeframe_s == terminal::kTimeframe1d);
    CHECK(job.from == dailyFetchFrom(kFetchToday));
    CHECK(job.to == kFetchToday);
    CHECK_FALSE(job.options);
    CHECK_FALSE(job.splits_only);
    CHECK_FALSE(job.statements);
    CHECK(job.option_expiration == 0);
}

void checkOptionJob(const terminal::IngestWorker::Job& job,
                    std::string_view symbol,
                    terminal::SessionDate expiration)
{
    CHECK(job.symbol == symbol);
    CHECK(job.options);
    CHECK(job.option_expiration == expiration);
    CHECK(job.from == 0);
    CHECK(job.to == 0);
    CHECK(job.timeframe_s == terminal::kTimeframe1m);
    CHECK_FALSE(job.statements);
    CHECK_FALSE(job.splits_only);
}

}  // namespace

TEST_CASE("open equity and etf listings with no positive daily bars plan a daily job")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spy = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    const auto book = store.createPortfolio("Spots");
    const std::vector<terminal::PortfolioHolding> holdings = {
        makeShare(terminal::PortfolioAssetKind::Equity, figiOf(store, aapl), 10),
        makeShare(terminal::PortfolioAssetKind::Etf, figiOf(store, spy), 2),
        makeCash(100),
    };
    store.replaceHoldings(book, holdings);

    const auto open = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(open.size() == 2);
    checkDailyJob(open[0], "AAPL");
    checkDailyJob(open[1], "SPY");

    writeCoverage(store, aapl, terminal::kTimeframe1d, 0);
    writeCoverage(store, spy, terminal::kTimeframe1m, 390);
    const auto still_open = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(still_open.size() == 2);
    checkDailyJob(still_open[0], "AAPL");
    checkDailyJob(still_open[1], "SPY");

    writeCoverage(store, aapl, terminal::kTimeframe1d, 1);
    const auto spy_only = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(spy_only.size() == 1);
    checkDailyJob(spy_only[0], "SPY");

    writeCoverage(store, spy, terminal::kTimeframe1d, 1);
    CHECK(terminal::portfolioFetchJobs(store, book, kFetchToday).empty());
}

TEST_CASE("relinkSymbol plans the new ticker")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    const auto book = store.createPortfolio("Rename");
    const std::vector<terminal::PortfolioHolding> holdings = {
        makeShare(terminal::PortfolioAssetKind::Equity, figiOf(store, id), 1),
    };
    store.replaceHoldings(book, holdings);

    const auto before = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(before.size() == 1);
    checkDailyJob(before[0], "AAPL");

    const auto at = store.findInstrumentById(id);
    REQUIRE(at.has_value());
    store.relinkSymbol(id, "META", at->created_at);
    const auto after = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(after.size() == 1);
    checkDailyJob(after[0], "META");
    CHECK(after[0].symbol != "AAPL");
}

TEST_CASE("a closed listing plans no fetch")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    const auto book = store.createPortfolio("Closed");
    const std::vector<terminal::PortfolioHolding> holdings = {
        makeShare(terminal::PortfolioAssetKind::Equity, figiOf(store, id), 4),
    };
    store.replaceHoldings(book, holdings);
    REQUIRE(terminal::portfolioFetchJobs(store, book, kFetchToday).size() == 1);

    const auto at = store.findInstrumentById(id);
    REQUIRE(at.has_value());
    store.closeListing(id, at->created_at, terminal::ListingCloseReason::Delisted);

    CHECK(terminal::portfolioFetchJobs(store, book, kFetchToday).empty());
    const auto rows = store.queryHoldings(book);
    REQUIRE(rows.size() == 1);
    CHECK_FALSE(rows[0].listing_open);
    REQUIRE(rows[0].symbol.has_value());
    CHECK(*rows[0].symbol == "AAPL");
    CHECK(rows[0].quantity == 4);
}

TEST_CASE("an option without a matching quote plans one job per underlying and expiration")
{
    constexpr terminal::SessionDate kNear = 20260925;
    constexpr terminal::SessionDate kFar = 20261016;
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto spx = store.testingInsertInstrument("$SPX", terminal::AssetClass::Index);
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spx_figi = figiOf(store, spx);
    const auto aapl_figi = figiOf(store, aapl);
    const auto book = store.createPortfolio("Options");

    std::vector<terminal::PortfolioHolding> holdings = {
        makeOption(spx_figi, kNear, terminal::OptionExpirationType::Weekly, 3200.0, terminal::OptionRight::Call, 1),
        makeOption(spx_figi, kNear, terminal::OptionExpirationType::Weekly, 3300.0, terminal::OptionRight::Put, -2),
        makeOption(aapl_figi, kNear, terminal::OptionExpirationType::Weekly, 100.0, terminal::OptionRight::Call, 3),
        makeCash(50),
    };
    store.replaceHoldings(book, holdings);

    const auto open = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(open.size() == 2);
    checkOptionJob(open[0], "$SPX", kNear);
    checkOptionJob(open[1], "AAPL", kNear);

    replaceQuotes(store, spx, kNear, terminal::OptionExpirationType::Weekly,
                  {makeQuote(spx, kNear, terminal::OptionExpirationType::Weekly, 3200.0, terminal::OptionRight::Call)});
    const auto put_still_open = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(put_still_open.size() == 2);
    checkOptionJob(put_still_open[0], "$SPX", kNear);
    checkOptionJob(put_still_open[1], "AAPL", kNear);

    replaceQuotes(store, spx, kNear, terminal::OptionExpirationType::Weekly,
                  {makeQuote(spx, kNear, terminal::OptionExpirationType::Weekly, 3200.0, terminal::OptionRight::Call),
                   makeQuote(spx, kNear, terminal::OptionExpirationType::Weekly, 3300.0, terminal::OptionRight::Put)});
    const auto aapl_only = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(aapl_only.size() == 1);
    checkOptionJob(aapl_only[0], "AAPL", kNear);

    replaceQuotes(store, aapl, kNear, terminal::OptionExpirationType::Weekly,
                  {makeQuote(aapl, kNear, terminal::OptionExpirationType::Weekly, 100.0, terminal::OptionRight::Call)});
    CHECK(terminal::portfolioFetchJobs(store, book, kFetchToday).empty());

    holdings.push_back(makeOption(spx_figi, kFar, terminal::OptionExpirationType::Monthly, 3200.0,
                                  terminal::OptionRight::Call, 1));
    store.replaceHoldings(book, holdings);
    const auto far = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(far.size() == 1);
    checkOptionJob(far[0], "$SPX", kFar);

    holdings.push_back(makeOption(spx_figi, kNear, terminal::OptionExpirationType::Monthly, 3200.0,
                                  terminal::OptionRight::Call, 1));
    store.replaceHoldings(book, holdings);
    const auto both_dates = terminal::portfolioFetchJobs(store, book, kFetchToday);
    REQUIRE(both_dates.size() == 2);
    checkOptionJob(both_dates[0], "$SPX", kNear);
    checkOptionJob(both_dates[1], "$SPX", kFar);
}

TEST_CASE("cash plans no fetch")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto book = store.createPortfolio("Cash");
    const std::vector<terminal::PortfolioHolding> holdings = {makeCash(-15)};
    store.replaceHoldings(book, holdings);
    CHECK(terminal::portfolioFetchJobs(store, book, kFetchToday).empty());
}
