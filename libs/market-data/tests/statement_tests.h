// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "TempDb.h"
#include "catch_amalgamated.hpp"
#include "market_data/Store.h"
#include "market_data/Types.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

terminal::Instrument makeStatementAapl()
{
    terminal::Instrument inst;
    inst.symbol = "AAPL";
    inst.figi = "BBG000B9XRY4";
    inst.name = "Apple";
    return inst;
}

terminal::StatementSnapshot makeIncomeSnapshot(terminal::InstrumentId id,
                                               terminal::StatementTimeframe timeframe,
                                               terminal::UnixSeconds fetched_at)
{
    terminal::StatementSnapshot snapshot;
    snapshot.instrument_id = id;
    snapshot.statement = terminal::StatementKind::Income;
    snapshot.timeframe = timeframe;
    snapshot.source = "mboum";
    snapshot.fetched_at = fetched_at;
    return snapshot;
}

terminal::StatementCell makeStatementCell(terminal::InstrumentId id,
                                          terminal::StatementKind statement,
                                          terminal::StatementTimeframe timeframe,
                                          std::string line_item,
                                          std::string period_end,
                                          terminal::StatementValue value)
{
    terminal::StatementCell cell;
    cell.instrument_id = id;
    cell.statement = statement;
    cell.timeframe = timeframe;
    cell.line_item = std::move(line_item);
    cell.period_end = std::move(period_end);
    cell.value = std::move(value);
    return cell;
}

}  // namespace

TEST_CASE("statement cells keep integer, real, text, and TTM")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeStatementAapl());
    const auto snapshot =
        makeIncomeSnapshot(id, terminal::StatementTimeframe::Annually, 1'700'000'000);
    const std::vector<terminal::StatementCell> cells = {
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "revenue",
                          "2025-09-27",
                          std::int64_t{416161000000}),
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "epsdil",
                          "2025-09-27",
                          7.46),
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "fiscalYear",
                          "2025-09-27",
                          std::string{"2025"}),
        makeStatementCell(id,
                          terminal::StatementKind::Balance,
                          terminal::StatementTimeframe::Annually,
                          "cashneq",
                          "TTM",
                          std::int64_t{39544000000}),
    };
    store.replaceStatement(snapshot, std::span(cells.data(), 3));
    terminal::StatementSnapshot balance = snapshot;
    balance.statement = terminal::StatementKind::Balance;
    store.replaceStatement(balance, std::span(cells.data() + 3, 1));

    const auto revenue = store.queryStatementLine(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually, "revenue");
    REQUIRE(revenue.size() == 1);
    REQUIRE(std::holds_alternative<std::int64_t>(revenue[0].value));
    CHECK(std::get<std::int64_t>(revenue[0].value) == 416161000000);

    const auto eps = store.queryStatementLine(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually, "epsdil");
    REQUIRE(eps.size() == 1);
    REQUIRE(std::holds_alternative<double>(eps[0].value));
    CHECK(std::get<double>(eps[0].value) == 7.46);

    const auto year = store.queryStatementLine(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually, "fiscalYear");
    REQUIRE(year.size() == 1);
    REQUIRE(std::holds_alternative<std::string>(year[0].value));
    CHECK(std::get<std::string>(year[0].value) == "2025");

    const auto cash = store.queryStatementPeriod(
        id, terminal::StatementKind::Balance, terminal::StatementTimeframe::Annually, "TTM");
    REQUIRE(cash.size() == 1);
    CHECK(cash[0].line_item == "cashneq");
    CHECK(std::get<std::int64_t>(cash[0].value) == 39544000000);

    const auto fetched = store.findStatementSnapshot(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually);
    REQUIRE(fetched.has_value());
    CHECK(fetched->source == "mboum");
    CHECK(fetched->fetched_at == 1'700'000'000);
}

TEST_CASE("annual and quarterly values stay distinct on the same period end")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeStatementAapl());

    const auto annual = makeIncomeSnapshot(id, terminal::StatementTimeframe::Annually, 10);
    const auto quarter = makeIncomeSnapshot(id, terminal::StatementTimeframe::Quarterly, 11);
    const terminal::StatementCell annual_revenue =
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "revenue",
                          "2025-09-27",
                          std::int64_t{416161000000});
    const terminal::StatementCell quarter_revenue =
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Quarterly,
                          "revenue",
                          "2025-09-27",
                          std::int64_t{102466000000});
    store.replaceStatement(annual, std::span(&annual_revenue, 1));
    store.replaceStatement(quarter, std::span(&quarter_revenue, 1));

    const auto annual_line = store.queryStatementLine(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually, "revenue");
    const auto quarter_line = store.queryStatementLine(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Quarterly, "revenue");
    REQUIRE(annual_line.size() == 1);
    REQUIRE(quarter_line.size() == 1);
    CHECK(std::get<std::int64_t>(annual_line[0].value) == 416161000000);
    CHECK(std::get<std::int64_t>(quarter_line[0].value) == 102466000000);
}

TEST_CASE("replacing a statement drops omitted cells and orders periods")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeStatementAapl());
    auto snapshot = makeIncomeSnapshot(id, terminal::StatementTimeframe::Annually, 10);
    const std::vector<terminal::StatementCell> first = {
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "revenue",
                          "2025-09-27",
                          std::int64_t{3}),
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "revenue",
                          "2024-09-28",
                          std::int64_t{2}),
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "revenue",
                          "TTM",
                          std::int64_t{4}),
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "gp",
                          "2025-09-27",
                          std::int64_t{1}),
    };
    store.replaceStatement(snapshot, first);

    const auto ordered = store.queryStatementLine(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually, "revenue");
    REQUIRE(ordered.size() == 3);
    CHECK(ordered[0].period_end == "2024-09-28");
    CHECK(ordered[1].period_end == "2025-09-27");
    CHECK(ordered[2].period_end == "TTM");

    snapshot.fetched_at = 20;
    store.replaceStatement(snapshot, std::span(first.data(), 1));
    const auto after = store.queryStatementCells(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually);
    REQUIRE(after.size() == 1);
    CHECK(after[0].line_item == "revenue");
    CHECK(after[0].period_end == "2025-09-27");
    const auto fetched = store.findStatementSnapshot(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually);
    REQUIRE(fetched.has_value());
    CHECK(fetched->fetched_at == 20);

    const auto period = store.queryStatementPeriod(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually, "2025-09-27");
    REQUIRE(period.size() == 1);
    CHECK(period[0].line_item == "revenue");
}

TEST_CASE("a failed statement replace leaves the previous grid")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto id = store.insertInstrument(makeStatementAapl());
    const auto snapshot = makeIncomeSnapshot(id, terminal::StatementTimeframe::Annually, 10);
    const terminal::StatementCell good =
        makeStatementCell(id,
                          terminal::StatementKind::Income,
                          terminal::StatementTimeframe::Annually,
                          "revenue",
                          "2025-09-27",
                          std::int64_t{5});
    store.replaceStatement(snapshot, std::span(&good, 1));

    terminal::StatementCell bad = good;
    bad.period_end = "2025-13-01";
    CHECK_THROWS_AS(store.replaceStatement(snapshot, std::span(&bad, 1)), std::runtime_error);

    bad = good;
    bad.line_item = " revenue";
    CHECK_THROWS_AS(store.replaceStatement(snapshot, std::span(&bad, 1)), std::runtime_error);

    bad = good;
    bad.value = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS_AS(store.replaceStatement(snapshot, std::span(&bad, 1)), std::runtime_error);

    bad = good;
    bad.value = std::string{};
    CHECK_THROWS_AS(store.replaceStatement(snapshot, std::span(&bad, 1)), std::runtime_error);

    const std::vector<terminal::StatementCell> dup = {good, good};
    CHECK_THROWS_AS(store.replaceStatement(snapshot, dup), std::runtime_error);

    terminal::StatementCell other = good;
    other.statement = terminal::StatementKind::Balance;
    CHECK_THROWS_AS(store.replaceStatement(snapshot, std::span(&other, 1)), std::runtime_error);

    const auto still = store.queryStatementLine(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually, "revenue");
    REQUIRE(still.size() == 1);
    CHECK(std::get<std::int64_t>(still[0].value) == 5);
    const auto fetched = store.findStatementSnapshot(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Annually);
    REQUIRE(fetched.has_value());
    CHECK(fetched->fetched_at == 10);
}

TEST_CASE("statement replace rejects an unknown instrument and accepts an empty grid")
{
    TempDb tmp;
    terminal::Store store(tmp.path());
    const auto missing = makeIncomeSnapshot(99, terminal::StatementTimeframe::Quarterly, 1);
    CHECK_THROWS_AS(store.replaceStatement(missing, {}), std::runtime_error);

    const auto id = store.insertInstrument(makeStatementAapl());
    const auto snapshot = makeIncomeSnapshot(id, terminal::StatementTimeframe::Trailing, 7);
    store.replaceStatement(snapshot, {});
    const auto fetched = store.findStatementSnapshot(
        id, terminal::StatementKind::Income, terminal::StatementTimeframe::Trailing);
    REQUIRE(fetched.has_value());
    CHECK(fetched->fetched_at == 7);
    CHECK(store
              .queryStatementCells(
                  id, terminal::StatementKind::Income, terminal::StatementTimeframe::Trailing)
              .empty());
}
