// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "backtest/BacktestJob.h"
#include "backtest/BacktestWorker.h"
#include "catch_amalgamated.hpp"
#include "data/LedgerAnalysis.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include <chrono>
#include <thread>
#include <vector>

namespace {

// 2026-06-01 13:30 UTC; one daily bar per calendar day.
constexpr terminal::UnixSeconds kJobStart = 1'780'320'600;
constexpr terminal::UnixSeconds kJobDay = 86'400;

void seedJobBars(terminal::Store& store, terminal::InstrumentId id, const std::vector<double>& closes)
{
    std::vector<terminal::Bar> bars;
    for (std::size_t day = 0; day < closes.size(); ++day)
    {
        terminal::Bar bar;
        bar.instrument_id = id;
        bar.timeframe_s = terminal::kTimeframe1d;
        bar.ts = kJobStart + (static_cast<terminal::UnixSeconds>(day) * kJobDay);
        bar.open = closes[day];
        bar.high = closes[day];
        bar.low = closes[day];
        bar.close = closes[day];
        bar.volume = 1'000;
        bars.push_back(bar);
    }
    REQUIRE(store.upsertBars(bars).written == static_cast<int>(bars.size()));
}

// Momentum over one bar with no threshold: long while the close keeps rising.
terminal::BacktestRequest risingRequest(std::string symbol, int days)
{
    terminal::BacktestRequest request;
    request.strategy_id = "momentum";
    request.options = {1, 0, 0};
    request.symbol = std::move(symbol);
    request.period = terminal::ChartBarPeriod::Day1;
    request.from = 20260601;
    request.to = terminal::toSessionDate(std::chrono::sys_days{std::chrono::year{2026} / 6 / 1} +
                                         std::chrono::days{days - 1});
    request.config.initial_cash = 10'000.0;
    return request;
}

}  // namespace

TEST_CASE("backtest config and period codes round trip")
{
    terminal::BacktestConfig config;
    config.initial_cash = 25'000.0;
    config.sizing = terminal::BacktestSizing::FixedNotional;
    config.sizing_value = 5'000.0;
    config.commission_per_share = 0.005;
    config.commission_minimum = 1.0;
    config.slippage_bps = 2.5;
    config.stop_loss_pct = 4.0;
    config.allow_short = false;
    config.flatten_at_session_end = true;
    config.close_at_end = false;
    const auto back = terminal::backtestConfigFromJson(terminal::backtestConfigJson(config));
    REQUIRE(back.has_value());
    CHECK(back->initial_cash == 25'000.0);
    CHECK(back->sizing == terminal::BacktestSizing::FixedNotional);
    CHECK(back->sizing_value == 5'000.0);
    CHECK(back->commission_per_share == 0.005);
    CHECK(back->commission_minimum == 1.0);
    CHECK(back->slippage_bps == 2.5);
    REQUIRE(back->stop_loss_pct.has_value());
    CHECK(*back->stop_loss_pct == 4.0);
    CHECK_FALSE(back->take_profit_pct.has_value());
    CHECK_FALSE(back->allow_short);
    CHECK(back->flatten_at_session_end);
    CHECK_FALSE(back->close_at_end);

    const auto defaults = terminal::backtestConfigFromJson("{}");
    REQUIRE(defaults.has_value());
    CHECK(defaults->sizing == terminal::BacktestSizing::PercentEquity);
    CHECK_FALSE(terminal::backtestConfigFromJson(R"({"sizing":"all_in"})").has_value());
    CHECK_FALSE(terminal::backtestConfigFromJson(R"({"initial_cash":"lots"})").has_value());
    CHECK_FALSE(terminal::backtestConfigFromJson("[]").has_value());

    CHECK(terminal::backtestPeriodFromCode("1d") == terminal::ChartBarPeriod::Day1);
    CHECK(terminal::backtestPeriodFromCode("15m") == terminal::ChartBarPeriod::Minute15);
    CHECK_FALSE(terminal::backtestPeriodFromCode("1w").has_value());
}

TEST_CASE("a fill on split-adjusted bars goes back to as-traded terms")
{
    terminal::CorporateAction split;
    split.instrument_id = 4;
    split.ex_ts = 1'000;
    split.type = terminal::CorporateActionType::Split;
    split.split_ratio = 4.0;
    terminal::CorporateAction other = split;
    other.instrument_id = 5;
    const std::vector<terminal::CorporateAction> actions{split, other};

    terminal::TradeFill before;
    before.instrument_id = 4;
    before.ts = 500;
    before.price = 25.0;
    before.quantity = 40.0;
    const auto traded = terminal::unadjustFill(before, actions, 2'000);
    CHECK(traded.price == 100.0);
    CHECK(traded.quantity == 10.0);

    terminal::TradeFill after = before;
    after.ts = 1'000;
    const auto same = terminal::unadjustFill(after, actions, 2'000);
    CHECK(same.price == 25.0);
    CHECK(same.quantity == 40.0);

    // Bars adjusted only through 900 never saw the split, so neither did the fill.
    const auto unsplit = terminal::unadjustFill(before, actions, 900);
    CHECK(unsplit.price == 25.0);
    CHECK(unsplit.quantity == 40.0);
}

TEST_CASE("a split after the tested range leaves the recorded fills as traded")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    // As-traded closes rise 100..119, then a 2-for-1 split goes ex on day 30, after the run.
    std::vector<double> traded;
    for (int day = 0; day < 20; ++day)
    {
        traded.push_back(100.0 + day);
    }
    seedJobBars(store, id, traded);
    terminal::CorporateAction split;
    split.instrument_id = id;
    split.ex_ts = kJobStart + (30 * kJobDay);
    split.type = terminal::CorporateActionType::Split;
    split.split_ratio = 2.0;
    store.upsertCorporateAction(split);

    const auto outcome = terminal::runAndRecordBacktest(store, risingRequest("AAPL", 20));
    REQUIRE(outcome.ok);
    const auto fills = store.queryFills(outcome.recorded.ledger_id);
    REQUIRE(fills.size() == 2);
    CHECK(fills[0].quantity == Catch::Approx(99.0));
    CHECK(fills[0].price == Catch::Approx(102.0));
    CHECK(fills[1].quantity == Catch::Approx(-99.0));
    CHECK(fills[1].price == Catch::Approx(119.0));
}

TEST_CASE("a backtest ledger is measured only through the end of its run")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    // 40 days of closes are stored, but the run covers only the first 20.
    std::vector<double> closes;
    for (int day = 0; day < 40; ++day)
    {
        closes.push_back(100.0 + day);
    }
    seedJobBars(store, id, closes);

    const auto outcome = terminal::runAndRecordBacktest(store, risingRequest("AAPL", 20));
    REQUIRE(outcome.ok);
    const auto run = store.findBacktestRun(outcome.recorded.run_id);
    REQUIRE(run.has_value());
    const auto analysis =
        terminal::analyzeLedger(store, outcome.recorded.ledger_id, id, kJobStart + (400 * kJobDay));
    REQUIRE_FALSE(analysis.curve.empty());
    CHECK(analysis.curve.back().ts == run->ts_end);
    CHECK(analysis.curve.size() == 20);
    REQUIRE_FALSE(analysis.benchmark_marks.empty());
    CHECK(analysis.benchmark_marks.back().ts == run->ts_end);
}

TEST_CASE("a recorded backtest across a split measures the same equity as the engine")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    // Adjusted closes rise 100..119; the 2-for-1 split lands on day 10, so earlier
    // as-traded closes are twice as high.
    std::vector<double> traded;
    for (int day = 0; day < 20; ++day)
    {
        const double adjusted = 100.0 + day;
        traded.push_back(day < 10 ? adjusted * 2.0 : adjusted);
    }
    seedJobBars(store, id, traded);
    terminal::CorporateAction split;
    split.instrument_id = id;
    split.ex_ts = kJobStart + (10 * kJobDay);
    split.type = terminal::CorporateActionType::Split;
    split.split_ratio = 2.0;
    store.upsertCorporateAction(split);

    const auto outcome = terminal::runAndRecordBacktest(store, risingRequest("AAPL", 20));
    REQUIRE(outcome.ok);
    CHECK(outcome.bars == 20);
    // Sized at the first rising close (101): 99 shares, bought at 102, sold at 119.
    CHECK(outcome.final_equity == Catch::Approx(10'000.0 + (99.0 * (119.0 - 102.0))));
    CHECK(outcome.ledger_name == "N-bar momentum AAPL 1d");

    const auto ledger = store.findLedger(outcome.recorded.ledger_id);
    REQUIRE(ledger.has_value());
    CHECK(ledger->kind == terminal::LedgerKind::Backtest);
    const auto fills = store.queryFills(outcome.recorded.ledger_id);
    REQUIRE(fills.size() == 2);
    // The entry was before the split, so it is stored as-traded: half the shares at twice the price.
    CHECK(fills[0].quantity == Catch::Approx(49.5));
    CHECK(fills[0].price == Catch::Approx(204.0));
    CHECK(fills[0].note == "open");
    CHECK(fills[1].quantity == Catch::Approx(-99.0));
    CHECK(fills[1].price == Catch::Approx(119.0));
    CHECK(fills[1].note == "end");
    const auto cash = store.queryCashFlows(outcome.recorded.ledger_id);
    REQUIRE(cash.size() == 1);
    CHECK(cash[0].amount == 10'000.0);

    const auto run = store.findBacktestRun(outcome.recorded.run_id);
    REQUIRE(run.has_value());
    CHECK(run->strategy_id == "momentum");
    CHECK(run->params_json == R"({"length":1,"threshold":0,"direction":"long"})");
    CHECK(run->timeframe_s == terminal::kTimeframe1d);
    CHECK(run->engine_version == terminal::kBacktestEngineVersion);
    CHECK(terminal::backtestConfigFromJson(run->config_json).has_value());

    const auto analysis = terminal::analyzeLedger(store, outcome.recorded.ledger_id, std::nullopt,
                                                  kJobStart + (40 * kJobDay));
    REQUIRE_FALSE(analysis.curve.empty());
    CHECK(analysis.curve.back().equity == Catch::Approx(outcome.final_equity));
    CHECK(analysis.trade_stats.trades == 1);
    CHECK(analysis.trade_stats.net_profit == Catch::Approx(99.0 * 17.0));
}

TEST_CASE("a backtest that cannot run records nothing")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    (void)store.testingInsertInstrument("SPX", terminal::AssetClass::Index);
    seedJobBars(store, id, {100.0});

    auto unknown_strategy = risingRequest("AAPL", 5);
    unknown_strategy.strategy_id = "astrology";
    CHECK(terminal::runAndRecordBacktest(store, unknown_strategy).error == "unknown strategy astrology");
    CHECK(terminal::runAndRecordBacktest(store, risingRequest("NOPE", 5)).error == "NOPE is not in the store");
    CHECK(terminal::runAndRecordBacktest(store, risingRequest("SPX", 5)).error == "SPX is not an equity or ETF");
    const auto thin = terminal::runAndRecordBacktest(store, risingRequest("AAPL", 5));
    CHECK_FALSE(thin.ok);
    CHECK(thin.error == "not enough 1d bars for AAPL in that range");
    auto backwards = risingRequest("AAPL", 5);
    backwards.to = 20260501;
    CHECK(terminal::runAndRecordBacktest(store, backwards).error == "the range ends before it starts");
    CHECK(store.listLedgers().empty());
    CHECK(store.listBacktestRuns().empty());
}

TEST_CASE("the backtest worker runs a request on its own connection")
{
    TempDb tmp;
    {
        terminal::Store store(tmp.path());
        const auto id = store.testingInsertInstrument("AAPL");
        std::vector<double> closes;
        for (int day = 0; day < 15; ++day)
        {
            closes.push_back(50.0 + day);
        }
        seedJobBars(store, id, closes);
    }
    terminal::BacktestWorker worker(tmp.path());
    const auto serial = worker.enqueue(risingRequest("AAPL", 15));
    for (int attempt = 0; attempt < 400 && worker.snapshot().finished_serial < serial; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(worker.snapshot().finished_serial == serial);
    CHECK(worker.idle());
    const auto outcome = worker.outcome(serial);
    REQUIRE(outcome.has_value());
    CHECK(outcome->ok);
    CHECK_FALSE(worker.outcome(serial + 1).has_value());

    const terminal::Store reader(tmp.path(), terminal::StoreMode::Reader);
    CHECK(reader.listBacktestRuns().size() == 1);
}
