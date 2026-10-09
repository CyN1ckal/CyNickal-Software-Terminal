// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "data/LedgerAnalysis.h"
#include "market_data/Store.h"

#include <span>
#include <string>
#include <vector>

namespace {

constexpr terminal::UnixSeconds kAnalysisDay = 86'400;
// 2026-06-01 13:30 UTC, a Monday session open.
constexpr terminal::UnixSeconds kAnalysisStart = 1'780'320'600;

std::string analysisFigi(const terminal::Store& store, terminal::InstrumentId id)
{
    const auto instrument = store.findInstrumentById(id);
    REQUIRE(instrument.has_value());
    REQUIRE(instrument->figi.has_value());
    return *instrument->figi;
}

// One daily bar per day for `days` days, closing at start_close + day * step.
void seedDailyCloses(terminal::Store& store, terminal::InstrumentId id, int days, double start_close, double step)
{
    std::vector<terminal::Bar> bars;
    for (int day = 0; day < days; ++day)
    {
        terminal::Bar bar;
        bar.instrument_id = id;
        bar.timeframe_s = terminal::kTimeframe1d;
        bar.ts = kAnalysisStart + (day * kAnalysisDay);
        bar.close = start_close + (day * step);
        bar.open = bar.close;
        bar.high = bar.close;
        bar.low = bar.close;
        bar.volume = 1;
        bars.push_back(bar);
    }
    REQUIRE(store.upsertBars(bars).written == days);
    terminal::CoverageDay coverage;
    coverage.instrument_id = id;
    coverage.timeframe_s = terminal::kTimeframe1d;
    coverage.session_date = 20260601;
    coverage.last_ts = bars.back().ts;
    coverage.first_ts = bars.front().ts;
    coverage.bar_count = days;
    coverage.status = terminal::CoverageStatus::Complete;
    coverage.ingested_at = 1'790'000'000;
    store.upsertCoverage(coverage);
}

terminal::TradeFill analysisFill(std::string figi, terminal::UnixSeconds ts, double quantity, double price)
{
    terminal::TradeFill fill;
    fill.kind = terminal::TradeAssetKind::Equity;
    fill.figi = std::move(figi);
    fill.ts = ts;
    fill.quantity = quantity;
    fill.price = price;
    return fill;
}

void seedClosePrices(terminal::Store& store, terminal::InstrumentId id, std::span<const double> closes)
{
    std::vector<terminal::Bar> bars;
    int day = 0;
    for (const double close : closes)
    {
        terminal::Bar bar;
        bar.instrument_id = id;
        bar.timeframe_s = terminal::kTimeframe1d;
        bar.ts = kAnalysisStart + (day * kAnalysisDay);
        bar.close = close;
        bar.open = bar.close;
        bar.high = bar.close;
        bar.low = bar.close;
        bar.volume = 1;
        bars.push_back(bar);
        ++day;
    }
    REQUIRE(store.upsertBars(bars).written == static_cast<int>(closes.size()));
}

// A funded share ledger whose daily closes span `days` sessions from kAnalysisStart.
terminal::LedgerId fundedShareLedger(terminal::Store& store, terminal::InstrumentId id, int days)
{
    seedDailyCloses(store, id, days, 50.0, 0.0);
    const auto ledger = store.createLedger("Account");
    terminal::LedgerCashFlow deposit;
    deposit.ts = kAnalysisStart - 3'600;
    deposit.amount = 10'000.0;
    (void)store.appendCashFlows(ledger, std::span<const terminal::LedgerCashFlow>(&deposit, 1));
    const std::vector<terminal::TradeFill> fills{
        analysisFill(analysisFigi(store, id), kAnalysisStart + 3'600, 10, 50.0),
    };
    (void)store.appendFills(ledger, fills);
    return ledger;
}

void insertDividend(terminal::Store& store, terminal::InstrumentId id, terminal::UnixSeconds ex_ts, double amount)
{
    terminal::CorporateAction action;
    action.instrument_id = id;
    action.ex_ts = ex_ts;
    action.type = terminal::CorporateActionType::Dividend;
    action.amount = amount;
    store.upsertCorporateAction(action);
}

void insertSplit(terminal::Store& store, terminal::InstrumentId id, terminal::UnixSeconds ex_ts, double ratio)
{
    terminal::CorporateAction action;
    action.instrument_id = id;
    action.ex_ts = ex_ts;
    action.type = terminal::CorporateActionType::Split;
    action.split_ratio = ratio;
    store.upsertCorporateAction(action);
}

}  // namespace

TEST_CASE("a ledger is evaluated at each stored close and matches its own books")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spy = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    seedDailyCloses(store, aapl, 10, 100.0, 1.0);
    seedDailyCloses(store, spy, 10, 500.0, 5.0);

    const auto ledger = store.createLedger("Account");
    terminal::LedgerCashFlow deposit;
    deposit.ts = kAnalysisStart - 3'600;
    deposit.amount = 10'000.0;
    (void)store.appendCashFlows(ledger, std::span<const terminal::LedgerCashFlow>(&deposit, 1));
    const std::vector<terminal::TradeFill> fills{
        analysisFill(analysisFigi(store, aapl), kAnalysisStart + 3'600, 20, 100.0),
        analysisFill(analysisFigi(store, aapl), kAnalysisStart + (4 * kAnalysisDay) + 3'600, -10, 104.0),
    };
    (void)store.appendFills(ledger, fills);

    const auto analysis = terminal::analyzeLedger(store, ledger, spy, kAnalysisStart + (30 * kAnalysisDay));
    // Ten closes, each known at the end of its session.
    REQUIRE(analysis.curve.size() == 10);
    CHECK(analysis.curve.front().ts == kAnalysisStart + terminal::kUsRthDurationS);
    CHECK(analysis.unpriced_symbols.empty());
    REQUIRE(analysis.received_at.has_value());
    CHECK(*analysis.received_at == 1'790'000'000);

    // 10 shares still held at the last close of 109; 10 sold at 104 for a 40 gain.
    const double expected_profit = 40.0 + (10.0 * (109.0 - 100.0));
    CHECK(analysis.curve.back().equity == Catch::Approx(10'000.0 + expected_profit));
    CHECK(analysis.curve_stats.net_pnl == Catch::Approx(expected_profit));
    CHECK(analysis.trade_stats.trades == 1);
    CHECK(analysis.trade_stats.net_profit == Catch::Approx(40.0));
    REQUIRE(analysis.round_trips.size() == 1);

    // SPY rose from 500 to 545 over the same ten closes.
    REQUIRE(analysis.benchmark_return.has_value());
    CHECK(*analysis.benchmark_return == Catch::Approx(0.09));
    CHECK(analysis.benchmark_marks.size() == 10);
}

TEST_CASE("a ledger without closes is evaluated at its own activity and names the gap")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto msft = store.testingInsertInstrument("MSFT");
    const auto ledger = store.createLedger("Unpriced");
    CHECK(terminal::analyzeLedger(store, ledger, std::nullopt, kAnalysisStart).curve.empty());

    const std::vector<terminal::TradeFill> fills{
        analysisFill(analysisFigi(store, msft), kAnalysisStart, 5, 400.0),
        analysisFill(analysisFigi(store, msft), kAnalysisStart + kAnalysisDay, 5, 410.0),
    };
    (void)store.appendFills(ledger, fills);
    const auto analysis = terminal::analyzeLedger(store, ledger, std::nullopt, kAnalysisStart + (5 * kAnalysisDay));
    REQUIRE(analysis.unpriced_symbols.size() == 1);
    CHECK(analysis.unpriced_symbols[0] == "MSFT");
    REQUIRE(analysis.curve.size() == 2);
    // Held at cost: no profit, and no benchmark was asked for.
    CHECK(analysis.curve_stats.net_pnl == Catch::Approx(0.0));
    CHECK_FALSE(analysis.benchmark_return.has_value());
    CHECK_FALSE(analysis.received_at.has_value());
}

TEST_CASE("history fetches cover traded shares without bars and the benchmark")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spy = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    seedDailyCloses(store, spy, 3, 500.0, 1.0);

    terminal::TradeFill aapl_fill;
    aapl_fill.kind = terminal::TradeAssetKind::Equity;
    aapl_fill.instrument_id = aapl;
    aapl_fill.symbol = "AAPL";
    aapl_fill.listing_open = true;
    aapl_fill.quantity = 1;
    terminal::TradeFill spy_fill = aapl_fill;
    spy_fill.kind = terminal::TradeAssetKind::Etf;
    spy_fill.instrument_id = spy;
    spy_fill.symbol = "SPY";
    terminal::TradeFill option = aapl_fill;
    option.kind = terminal::TradeAssetKind::Option;
    const std::vector<terminal::TradeFill> fills{aapl_fill, spy_fill, aapl_fill, option};

    // SPY already has closes; AAPL does not; options have no history to fetch.
    const auto missing = terminal::ledgerHistoryFetchJobs(store, fills, "SPY", 20260924);
    REQUIRE(missing.size() == 1);
    CHECK(missing[0].symbol == "AAPL");
    CHECK(missing[0].timeframe_s == terminal::kTimeframe1d);

    const auto with_new_benchmark = terminal::ledgerHistoryFetchJobs(store, fills, "QQQ", 20260924);
    REQUIRE(with_new_benchmark.size() == 2);
    CHECK(with_new_benchmark[1].symbol == "QQQ");

    const auto refresh = terminal::ledgerHistoryFetchJobs(store, fills, "SPY", 20260924, false);
    CHECK(refresh.size() == 2);
    CHECK(terminal::ledgerHistoryFetchJobs(store, fills, "", 20260924, false).size() == 2);
}

TEST_CASE("benchmark return over a no-split window equals price return plus dividend/start")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spy = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    // Closes 100, 110, 120. The dividend's ex is the second session's open, after the
    // first close and before the last. Cash is not reinvested, so the curve compounds
    // to (last + dividend) / first - 1, which is the price return plus dividend/start.
    seedDailyCloses(store, spy, 3, 100.0, 10.0);
    insertDividend(store, spy, kAnalysisStart + kAnalysisDay, 2.0);
    const auto ledger = fundedShareLedger(store, aapl, 3);

    const auto analysis = terminal::analyzeLedger(store, ledger, spy, kAnalysisStart + (30 * kAnalysisDay));
    REQUIRE(analysis.benchmark_marks.size() == 3);
    const double first = 100.0;
    const double last = 120.0;
    const double dividend = 2.0;
    const double price_return = last / first - 1.0;
    REQUIRE(analysis.benchmark_return.has_value());
    CHECK(*analysis.benchmark_return == Catch::Approx(price_return + (dividend / first)));
    CHECK(*analysis.benchmark_return == Catch::Approx((last + dividend) / first - 1.0));
}

TEST_CASE("a split after the dividend scales the benchmark dividend the way prices are scaled")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spy = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    // As-traded closes 400, 400, 110. A 4-for-1 on the third session's open divides the
    // earlier prices by 4 and leaves the ex-date close alone: 100, 100, 110. The dividend
    // is as-traded cash, so one adjusted share receives 8/4.
    const std::vector<double> closes{400.0, 400.0, 110.0};
    seedClosePrices(store, spy, closes);
    constexpr double kSplit = 4.0;
    insertSplit(store, spy, kAnalysisStart + (2 * kAnalysisDay), kSplit);
    insertDividend(store, spy, kAnalysisStart + kAnalysisDay, 8.0);
    const auto ledger = fundedShareLedger(store, aapl, 3);

    const auto analysis = terminal::analyzeLedger(store, ledger, spy, kAnalysisStart + (30 * kAnalysisDay));
    REQUIRE(analysis.benchmark_marks.size() == 3);
    CHECK(analysis.benchmark_marks.front().price == Catch::Approx(100.0));
    CHECK(analysis.benchmark_marks[1].price == Catch::Approx(100.0));
    CHECK(analysis.benchmark_marks.back().price == Catch::Approx(110.0));
    const double adjusted_dividend = 8.0 / kSplit;
    REQUIRE(analysis.benchmark_return.has_value());
    CHECK(*analysis.benchmark_return == Catch::Approx((110.0 + adjusted_dividend) / 100.0 - 1.0));
}
