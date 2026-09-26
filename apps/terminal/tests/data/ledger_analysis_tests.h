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
