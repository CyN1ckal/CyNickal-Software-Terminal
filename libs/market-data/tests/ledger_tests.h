// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Figi.h"
#include "market_data/Store.h"
#include "market_data/Types.h"

#include "../private/Sqlite.h"

#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string ledgerFigiOf(const terminal::Store& store, terminal::InstrumentId id)
{
    const auto instrument = store.findInstrumentById(id);
    REQUIRE(instrument.has_value());
    REQUIRE(instrument->figi.has_value());
    return *instrument->figi;
}

terminal::TradeFill makeShareFill(terminal::TradeAssetKind kind,
                                  std::string figi,
                                  terminal::UnixSeconds ts,
                                  double quantity,
                                  double price)
{
    terminal::TradeFill fill;
    fill.kind = kind;
    fill.figi = std::move(figi);
    fill.ts = ts;
    fill.quantity = quantity;
    fill.price = price;
    return fill;
}

terminal::TradeFill makeOptionFill(std::string figi, terminal::UnixSeconds ts, double quantity, double price)
{
    terminal::TradeFill fill;
    fill.kind = terminal::TradeAssetKind::Option;
    fill.figi = std::move(figi);
    fill.expiration = 20261016;
    fill.expiration_type = terminal::OptionExpirationType::Monthly;
    fill.strike = 5800;
    fill.right = terminal::OptionRight::Put;
    fill.ts = ts;
    fill.quantity = quantity;
    fill.price = price;
    return fill;
}

terminal::LedgerCashFlow makeCashFlow(terminal::UnixSeconds ts, double amount)
{
    terminal::LedgerCashFlow flow;
    flow.ts = ts;
    flow.amount = amount;
    return flow;
}

terminal::BacktestRun makeRun(std::string figi)
{
    terminal::BacktestRun run;
    run.strategy_id = "ma_cross";
    run.params_json = R"({"fast":10,"slow":30})";
    run.config_json = R"({"initial_cash":10000})";
    run.figi = std::move(figi);
    run.timeframe_s = terminal::kTimeframe1d;
    run.ts_begin = 1'700'000'000;
    run.ts_end = 1'710'000'000;
    run.engine_version = 1;
    return run;
}

std::int64_t ledgerCountRows(const std::filesystem::path& path, std::string_view table)
{
    terminal::SqliteDb db(path);
    terminal::SqliteStmt stmt(db.handle(), "SELECT COUNT(*) FROM " + std::string(table));
    if (!stmt.stepRow())
    {
        throw std::runtime_error("COUNT returned no row");
    }
    const auto count = stmt.columnInt64(0);
    stmt.reset();
    return count;
}

// instrument_listing is ON DELETE RESTRICT too. Drop it when the fill is the row under test.
void ledgerDeleteListings(const std::filesystem::path& path, terminal::InstrumentId id)
{
    terminal::SqliteDb db(path);
    db.exec("PRAGMA foreign_keys = ON");
    terminal::SqliteStmt stmt(db.handle(), "DELETE FROM instrument_listing WHERE instrument_id = ?");
    stmt.bindInt64(1, id);
    stmt.stepDone();
    stmt.reset();
}

}  // namespace

TEST_CASE("ledgers create, list, rename, and reject a duplicate manual name")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    CHECK_THROWS_WITH(store.createLedger(""), "ledger name is empty");
    CHECK_THROWS_WITH(store.createLedger(" Cash"), "ledger name is empty");

    const auto zeta = store.createLedger("zeta");
    const auto alpha = store.createLedger("Alpha");
    CHECK_THROWS_WITH(store.createLedger("ALPHA"), "ledger name already exists");

    const auto listed = store.listLedgers();
    REQUIRE(listed.size() == 2);
    CHECK(listed[0].id == alpha);
    CHECK(listed[0].kind == terminal::LedgerKind::Manual);
    CHECK(listed[1].id == zeta);
    CHECK(listed[0].created_at == listed[0].updated_at);

    store.renameLedger(alpha, "alpha");
    const auto renamed = store.findLedger(alpha);
    REQUIRE(renamed.has_value());
    CHECK(renamed->name == "alpha");
    CHECK_THROWS_WITH(store.renameLedger(zeta, "ALPHA"), "ledger name already exists");
    CHECK_THROWS_WITH(store.renameLedger(999, "Other"), "ledger not found");
    CHECK_THROWS_WITH(store.deleteLedger(999), "ledger not found");
    CHECK_THROWS_WITH(store.queryFills(999), "ledger not found");
    CHECK_THROWS_WITH(store.queryCashFlows(999), "ledger not found");
    CHECK_FALSE(store.findLedger(999).has_value());
    CHECK(store.queryFills(alpha).empty());
    CHECK(store.queryCashFlows(alpha).empty());
}

TEST_CASE("appendFills round-trips shares and options by FIGI in time order")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spy = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    const auto spx = store.testingInsertInstrument("SPX", terminal::AssetClass::Index);
    const auto ledger = store.createLedger("Account");

    auto buy = makeShareFill(terminal::TradeAssetKind::Equity, ledgerFigiOf(store, aapl), 2'000, 10, 150.25);
    buy.fees = 1.5;
    buy.note = "opening";
    buy.external_id = "T-1";
    auto sell = makeShareFill(terminal::TradeAssetKind::Equity, ledgerFigiOf(store, aapl), 3'000, -4, 155);
    auto etf = makeShareFill(terminal::TradeAssetKind::Etf, ledgerFigiOf(store, spy), 1'000, 3, 500);
    auto put = makeOptionFill(ledgerFigiOf(store, spx), 2'000, -1, 42.5);
    const std::vector<terminal::TradeFill> fills{buy, sell, etf, put};

    const auto result = store.appendFills(ledger, fills);
    CHECK(result.written == 4);
    CHECK(result.skipped == 0);

    const auto rows = store.queryFills(ledger);
    REQUIRE(rows.size() == 4);
    // ts, then id: etf (1000), buy (2000), put (2000, later id), sell (3000).
    CHECK(rows[0].kind == terminal::TradeAssetKind::Etf);
    CHECK(rows[0].instrument_id == spy);
    CHECK(rows[0].symbol == "SPY");
    CHECK(rows[0].listing_open);

    CHECK(rows[1].kind == terminal::TradeAssetKind::Equity);
    CHECK(rows[1].figi == ledgerFigiOf(store, aapl));
    CHECK(rows[1].instrument_id == aapl);
    CHECK(rows[1].symbol == "AAPL");
    CHECK(rows[1].ts == 2'000);
    CHECK(rows[1].quantity == 10);
    CHECK(rows[1].price == 150.25);
    CHECK(rows[1].fees == 1.5);
    CHECK(rows[1].note == "opening");
    CHECK(rows[1].external_id == "T-1");
    CHECK_FALSE(rows[1].expiration.has_value());

    CHECK(rows[2].kind == terminal::TradeAssetKind::Option);
    CHECK(rows[2].instrument_id == spx);
    CHECK(rows[2].expiration == 20261016);
    CHECK(rows[2].expiration_type == terminal::OptionExpirationType::Monthly);
    CHECK(rows[2].strike == 5800);
    CHECK(rows[2].right == terminal::OptionRight::Put);
    CHECK(rows[2].quantity == -1);
    CHECK(rows[1].id < rows[2].id);

    CHECK(rows[3].quantity == -4);
    CHECK_FALSE(rows[3].note.has_value());
    CHECK_FALSE(rows[3].external_id.has_value());

    const auto after = store.findLedger(ledger);
    REQUIRE(after.has_value());
    CHECK(after->updated_at >= after->created_at);
}

TEST_CASE("appendFills rejects the whole batch on one bad fill")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto spy = store.testingInsertInstrument("SPY", terminal::AssetClass::Etf);
    const auto ledger = store.createLedger("Account");
    const auto good = makeShareFill(terminal::TradeAssetKind::Equity, ledgerFigiOf(store, aapl), 1'000, 1, 100);

    const auto rejects = [&](terminal::TradeFill bad, const char* message) {
        const std::vector<terminal::TradeFill> fills{good, std::move(bad)};
        CHECK_THROWS_WITH(store.appendFills(ledger, fills), message);
        CHECK(store.queryFills(ledger).empty());
    };

    auto no_figi = good;
    no_figi.figi.reset();
    rejects(no_figi, "trade fill figi is missing");

    auto bad_figi = good;
    bad_figi.figi = "not-a-figi";
    rejects(bad_figi, "trade fill figi is invalid");

    auto unknown = good;
    unknown.figi = terminal::testingFigiFor("NOPE");
    rejects(unknown, "trade fill figi was not found");

    auto wrong_kind = good;
    wrong_kind.figi = ledgerFigiOf(store, spy);
    rejects(wrong_kind, "trade fill kind does not match its instrument");

    auto share_with_strike = good;
    share_with_strike.strike = 100;
    rejects(share_with_strike, "trade fill kind does not match its fields");

    auto option_missing_right = makeOptionFill(ledgerFigiOf(store, aapl), 1'000, 1, 2);
    option_missing_right.right.reset();
    rejects(option_missing_right, "trade fill kind does not match its fields");

    auto option_bad_date = makeOptionFill(ledgerFigiOf(store, aapl), 1'000, 1, 2);
    option_bad_date.expiration = 20261399;
    rejects(option_bad_date, "trade fill option identity is invalid");

    auto zero = good;
    zero.quantity = 0;
    rejects(zero, "trade fill quantity is zero or not finite");

    auto nan_price = good;
    nan_price.price = std::numeric_limits<double>::quiet_NaN();
    rejects(nan_price, "trade fill price is negative or not finite");

    auto negative_fees = good;
    negative_fees.fees = -0.01;
    rejects(negative_fees, "trade fill fees are negative or not finite");

    auto negative_ts = good;
    negative_ts.ts = -1;
    rejects(negative_ts, "trade fill time is negative");

    auto untrimmed = good;
    untrimmed.external_id = " T-1";
    rejects(untrimmed, "trade fill external_id is empty or untrimmed");

    // A zero price is a worthless expiry or a delisted share and is allowed.
    auto worthless = good;
    worthless.price = 0;
    CHECK(store.appendFills(ledger, std::span<const terminal::TradeFill>(&worthless, 1)).written == 1);
}

TEST_CASE("appendFills skips an external_id the ledger already has")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto first = store.createLedger("First");
    const auto second = store.createLedger("Second");
    const auto figi = ledgerFigiOf(store, aapl);

    auto a = makeShareFill(terminal::TradeAssetKind::Equity, figi, 1'000, 1, 100);
    a.external_id = "A";
    auto b = makeShareFill(terminal::TradeAssetKind::Equity, figi, 2'000, 1, 101);
    b.external_id = "B";
    const auto untagged = makeShareFill(terminal::TradeAssetKind::Equity, figi, 3'000, 1, 102);

    const std::vector<terminal::TradeFill> batch{a, b, a, untagged};
    const auto once = store.appendFills(first, batch);
    CHECK(once.written == 3);
    CHECK(once.skipped == 1);

    const auto twice = store.appendFills(first, batch);
    CHECK(twice.written == 1);
    CHECK(twice.skipped == 3);
    CHECK(store.queryFills(first).size() == 4);

    // external_id is unique per ledger, not across ledgers.
    const auto other = store.appendFills(second, batch);
    CHECK(other.written == 3);
    CHECK(other.skipped == 1);
}

TEST_CASE("fills and cash flows delete only from their own ledger")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto mine = store.createLedger("Mine");
    const auto theirs = store.createLedger("Theirs");
    const auto fill = makeShareFill(terminal::TradeAssetKind::Equity, ledgerFigiOf(store, aapl), 1'000, 5, 10);
    (void)store.appendFills(mine, std::span<const terminal::TradeFill>(&fill, 1));

    auto deposit = makeCashFlow(500, 10'000);
    deposit.note = "wire";
    deposit.external_id = "D-1";
    const std::vector<terminal::LedgerCashFlow> flows{deposit, makeCashFlow(4'000, -2'500), deposit};
    const auto cash = store.appendCashFlows(mine, flows);
    CHECK(cash.written == 2);
    CHECK(cash.skipped == 1);

    const auto cash_rows = store.queryCashFlows(mine);
    REQUIRE(cash_rows.size() == 2);
    CHECK(cash_rows[0].ts == 500);
    CHECK(cash_rows[0].amount == 10'000);
    CHECK(cash_rows[0].note == "wire");
    CHECK(cash_rows[0].external_id == "D-1");
    CHECK(cash_rows[1].amount == -2'500);

    const auto zero = makeCashFlow(1, 0);
    CHECK_THROWS_WITH(store.appendCashFlows(mine, std::span<const terminal::LedgerCashFlow>(&zero, 1)),
                      "cash flow amount is zero or not finite");

    const auto fill_id = store.queryFills(mine).at(0).id;
    CHECK_THROWS_WITH(store.deleteFill(theirs, fill_id), "trade fill not found");
    CHECK_THROWS_WITH(store.deleteCashFlow(theirs, cash_rows[0].id), "cash flow not found");
    store.deleteFill(mine, fill_id);
    store.deleteCashFlow(mine, cash_rows[0].id);
    CHECK(store.queryFills(mine).empty());
    REQUIRE(store.queryCashFlows(mine).size() == 1);
    CHECK_THROWS_WITH(store.deleteFill(mine, fill_id), "trade fill not found");
}

TEST_CASE("recordBacktestRun writes a read-only ledger with its run")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto figi = ledgerFigiOf(store, aapl);
    const auto manual = store.createLedger("MA cross");

    auto entry = makeShareFill(terminal::TradeAssetKind::Equity, figi, 1'700'100'000, 50, 100);
    entry.fees = 1;
    const auto exit = makeShareFill(terminal::TradeAssetKind::Equity, figi, 1'700'900'000, -50, 110);
    const std::vector<terminal::TradeFill> fills{entry, exit};
    const std::vector<terminal::LedgerCashFlow> cash{makeCashFlow(1'700'000'000, 10'000)};

    // A backtest may reuse a manual ledger's name, and backtest names repeat.
    const auto first = store.recordBacktestRun("MA cross", makeRun(figi), fills, cash);
    const auto second = store.recordBacktestRun("MA cross", makeRun(figi), {}, cash);
    CHECK(first.ledger_id != second.ledger_id);

    const auto ledgers = store.listLedgers();
    REQUIRE(ledgers.size() == 3);
    CHECK(ledgers[0].id == manual);
    CHECK(ledgers[1].kind == terminal::LedgerKind::Backtest);
    CHECK(ledgers[2].kind == terminal::LedgerKind::Backtest);

    const auto run = store.findBacktestRun(first.run_id);
    REQUIRE(run.has_value());
    CHECK(run->ledger_id == first.ledger_id);
    CHECK(run->strategy_id == "ma_cross");
    CHECK(run->params_json == R"({"fast":10,"slow":30})");
    CHECK(run->config_json == R"({"initial_cash":10000})");
    CHECK(run->figi == figi);
    CHECK(run->instrument_id == aapl);
    CHECK(run->symbol == "AAPL");
    CHECK(run->timeframe_s == terminal::kTimeframe1d);
    CHECK(run->ts_begin == 1'700'000'000);
    CHECK(run->ts_end == 1'710'000'000);
    CHECK(run->engine_version == 1);
    CHECK(run->created_at > 0);

    const auto by_ledger = store.findBacktestRunForLedger(first.ledger_id);
    REQUIRE(by_ledger.has_value());
    CHECK(by_ledger->id == first.run_id);
    CHECK_FALSE(store.findBacktestRunForLedger(manual).has_value());
    CHECK_FALSE(store.findBacktestRun(999).has_value());

    const auto runs = store.listBacktestRuns();
    REQUIRE(runs.size() == 2);
    CHECK(runs[0].id == second.run_id);
    CHECK(runs[1].id == first.run_id);

    const auto rows = store.queryFills(first.ledger_id);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].quantity == 50);
    CHECK(rows[0].fees == 1);
    CHECK(rows[1].quantity == -50);
    REQUIRE(store.queryCashFlows(first.ledger_id).size() == 1);

    CHECK_THROWS_WITH(store.appendFills(first.ledger_id, fills), "backtest ledger is read-only");
    CHECK_THROWS_WITH(store.appendCashFlows(first.ledger_id, cash), "backtest ledger is read-only");
    CHECK_THROWS_WITH(store.deleteFill(first.ledger_id, rows[0].id), "backtest ledger is read-only");
    CHECK_THROWS_WITH(store.renameLedger(first.ledger_id, "Other"), "backtest ledger is read-only");
    CHECK(store.queryFills(first.ledger_id).size() == 2);
}

TEST_CASE("recordBacktestRun rejects a bad run and writes nothing")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto msft = store.testingInsertInstrument("MSFT");
    const auto spx = store.testingInsertInstrument("SPX", terminal::AssetClass::Index);
    const auto figi = ledgerFigiOf(store, aapl);
    const auto fill = makeShareFill(terminal::TradeAssetKind::Equity, figi, 1'700'100'000, 1, 100);
    const std::vector<terminal::TradeFill> fills{fill};

    const auto rejects = [&](std::string_view name,
                             const terminal::BacktestRun& run,
                             std::span<const terminal::TradeFill> run_fills,
                             const char* message) {
        CHECK_THROWS_WITH(store.recordBacktestRun(name, run, run_fills, {}), message);
        CHECK(store.listLedgers().empty());
        CHECK(store.listBacktestRuns().empty());
    };

    rejects("", makeRun(figi), fills, "ledger name is empty");

    auto no_strategy = makeRun(figi);
    no_strategy.strategy_id = "";
    rejects("Run", no_strategy, fills, "backtest strategy_id is empty or untrimmed");

    auto bad_json = makeRun(figi);
    bad_json.params_json = "{fast:10";
    rejects("Run", bad_json, fills, "backtest params or config is not valid JSON");

    auto backwards = makeRun(figi);
    backwards.ts_end = backwards.ts_begin;
    rejects("Run", backwards, fills, "backtest range, timeframe, or engine version is invalid");

    auto no_figi = makeRun(figi);
    no_figi.figi.reset();
    rejects("Run", no_figi, fills, "backtest figi is missing or invalid");

    rejects("Run", makeRun(ledgerFigiOf(store, spx)), {}, "backtest instrument is not an equity or ETF");

    const auto other = makeShareFill(terminal::TradeAssetKind::Equity, ledgerFigiOf(store, msft), 1'700'100'000, 1, 10);
    const std::vector<terminal::TradeFill> mixed{fill, other};
    rejects("Run", makeRun(figi), mixed, "backtest fill is not a share of the run's instrument");

    const auto option = makeOptionFill(figi, 1'700'100'000, 1, 2);
    rejects("Run", makeRun(figi), std::span<const terminal::TradeFill>(&option, 1),
            "backtest fill is not a share of the run's instrument");

    CHECK(ledgerCountRows(tmp.path(), "trade_fill") == 0);
}

TEST_CASE("deleteLedger cascades its rows and fills restrict the instrument")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto aapl = store.testingInsertInstrument("AAPL");
    const auto msft = store.testingInsertInstrument("MSFT");
    const auto aapl_figi = ledgerFigiOf(store, aapl);
    const auto kept = store.createLedger("Kept");
    const auto dropped = store.createLedger("Dropped");

    const auto keep_fill = makeShareFill(terminal::TradeAssetKind::Equity, ledgerFigiOf(store, msft), 1'000, 1, 10);
    (void)store.appendFills(kept, std::span<const terminal::TradeFill>(&keep_fill, 1));
    const auto drop_fill = makeShareFill(terminal::TradeAssetKind::Equity, aapl_figi, 1'000, 1, 10);
    (void)store.appendFills(dropped, std::span<const terminal::TradeFill>(&drop_fill, 1));
    const auto flow = makeCashFlow(1, 100);
    (void)store.appendCashFlows(dropped, std::span<const terminal::LedgerCashFlow>(&flow, 1));
    const auto backtest = store.recordBacktestRun(
        "Run", makeRun(aapl_figi), std::span<const terminal::TradeFill>(&drop_fill, 1), {});

    // A fill and a run both hold AAPL.
    ledgerDeleteListings(tmp.path(), aapl);
    CHECK_THROWS_AS(terminal::Store::testingDeleteInstrument(tmp.path(), aapl), std::runtime_error);

    store.deleteLedger(dropped);
    CHECK_FALSE(store.findLedger(dropped).has_value());
    CHECK_THROWS_AS(terminal::Store::testingDeleteInstrument(tmp.path(), aapl), std::runtime_error);

    store.deleteLedger(backtest.ledger_id);
    CHECK(store.listBacktestRuns().empty());
    CHECK(ledgerCountRows(tmp.path(), "ledger_cash_flow") == 0);
    CHECK(ledgerCountRows(tmp.path(), "trade_fill") == 1);
    CHECK(store.queryFills(kept).size() == 1);

    terminal::Store::testingDeleteInstrument(tmp.path(), aapl);
    CHECK_FALSE(store.findInstrumentById(aapl).has_value());
}

TEST_CASE("a deleted ledger's id is never given to a later ledger")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    (void)store.createLedger("First");
    const auto newest = store.createLedger("Second");
    store.deleteLedger(newest);
    const auto next = store.createLedger("Third");
    CHECK(next > newest);
}

TEST_CASE("findLatestCoverage returns the newest session with bars")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.testingInsertInstrument("AAPL");
    CHECK_FALSE(store.findLatestCoverage(id, terminal::kTimeframe1d).has_value());
    const auto day = [&](terminal::SessionDate date, int bars, terminal::UnixSeconds last_ts) {
        terminal::CoverageDay row;
        row.instrument_id = id;
        row.timeframe_s = terminal::kTimeframe1d;
        row.session_date = date;
        row.bar_count = bars;
        if (bars > 0)
        {
            row.first_ts = last_ts;
            row.last_ts = last_ts;
        }
        row.status = terminal::CoverageStatus::Complete;
        store.upsertCoverage(row);
    };
    day(20260921, 1, 1'790'000'000);
    day(20260922, 1, 1'790'086'400);
    day(20260923, 0, 0);
    const auto latest = store.findLatestCoverage(id, terminal::kTimeframe1d);
    REQUIRE(latest.has_value());
    CHECK(latest->session_date == 20260922);
    CHECK(latest->last_ts == 1'790'086'400);
    CHECK_FALSE(store.findLatestCoverage(id, terminal::kTimeframe1m).has_value());
}
