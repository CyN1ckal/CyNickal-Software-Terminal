// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "data/StatementSheet.h"

#include <vector>

namespace {

[[nodiscard]] terminal::StatementCell financialSheetCell(std::string line_item,
                                                         std::string period_end,
                                                         terminal::StatementValue value)
{
    terminal::StatementCell cell;
    cell.instrument_id = 1;
    cell.statement = terminal::StatementKind::Income;
    cell.timeframe = terminal::StatementTimeframe::Annually;
    cell.line_item = std::move(line_item);
    cell.period_end = std::move(period_end);
    cell.value = std::move(value);
    return cell;
}

}  // namespace

TEST_CASE("statement sheet keeps the four newest periods, newest first")
{
    std::vector<terminal::StatementCell> cells;
    const char* years[] = {"2020-09-26", "2021-09-25", "2022-09-24", "2023-09-30", "2024-09-28",
                           "2025-09-27"};
    for (const char* year : years)
    {
        cells.push_back(financialSheetCell("revenue", year, std::int64_t{100}));
        cells.push_back(financialSheetCell("aaa", year, std::int64_t{1}));
    }
    cells.push_back(financialSheetCell("gp", "2020-09-26", std::int64_t{7}));
    cells.push_back(financialSheetCell("fiscalYear", "2025-09-27", std::string{"2025"}));
    cells.push_back(financialSheetCell("fiscalQuarter", "2025-09-27", std::string{"4"}));
    cells.push_back(financialSheetCell("revenue", "TTM", std::int64_t{9}));
    cells.push_back(financialSheetCell("epsdil", "2025-09-27", 6.08));

    const terminal::StatementSheet sheet = terminal::buildStatementSheet(cells);
    CHECK(sheet.period_count == 4);
    CHECK(sheet.periods[0] == "2025-09-27");
    CHECK(sheet.periods[1] == "2024-09-28");
    CHECK(sheet.periods[2] == "2023-09-30");
    CHECK(sheet.periods[3] == "2022-09-24");
    REQUIRE(sheet.rows.size() == 3);
    CHECK(sheet.rows[0].line_item == "revenue");
    CHECK(sheet.rows[1].line_item == "epsdil");
    CHECK(sheet.rows[2].line_item == "aaa");
    CHECK(sheet.rows[0].label == "Revenue");
    CHECK(sheet.rows[1].label == "Diluted EPS");
    CHECK(sheet.rows[2].label == "Aaa");
    CHECK(sheet.rows[0].slots[0].present);
    CHECK(std::get<std::int64_t>(sheet.rows[0].slots[0].value) == 100);
    CHECK(sheet.rows[0].slots[3].present);
    CHECK_FALSE(sheet.rows[1].slots[1].present);
    CHECK(std::get<double>(sheet.rows[1].slots[0].value) == 6.08);
}

TEST_CASE("statement sheet leaves unused period columns empty")
{
    const terminal::StatementCell cells[] = {
        financialSheetCell("revenue", "2025-06-28", std::int64_t{10}),
        financialSheetCell("revenue", "2025-03-29", std::int64_t{9}),
    };
    const terminal::StatementSheet sheet = terminal::buildStatementSheet(cells);
    CHECK(sheet.period_count == 2);
    CHECK(sheet.periods[0] == "2025-06-28");
    CHECK(sheet.periods[1] == "2025-03-29");
    CHECK(sheet.periods[2].empty());
    REQUIRE(sheet.rows.size() == 1);
    CHECK(sheet.rows[0].slots[0].present);
    CHECK(sheet.rows[0].slots[1].present);
    CHECK_FALSE(sheet.rows[0].slots[2].present);
}

TEST_CASE("statement line labels use analysis names")
{
    CHECK(terminal::statementLineLabel("revenue") == "Revenue");
    CHECK(terminal::statementLineLabel("cor") == "Cost of Revenue");
    CHECK(terminal::statementLineLabel("gp") == "Gross Profit");
    CHECK(terminal::statementLineLabel("opinc") == "Operating Income");
    CHECK(terminal::statementLineLabel("netinccmn") == "Net Income to Common");
    CHECK(terminal::statementLineLabel("epsdil") == "Diluted EPS");
    CHECK(terminal::statementLineLabel("cashneq") == "Cash and Equivalents");
    CHECK(terminal::statementLineLabel("defferedTaxAssets") == "Deferred Tax Assets");
    CHECK(terminal::statementLineLabel("fcfMargin") == "Free Cash Flow Margin");
    CHECK(terminal::statementLineLabel("accountsReceivableCM") == "Accounts Receivable (Capital Markets)");
    CHECK(terminal::statementLineLabel("ncfo") == "Operating Cash Flow");
    CHECK(terminal::statementLineLabel("workingcapital") == "Working Capital");
    CHECK(terminal::statementLineLabel("someNewLineCF") == "Some New Line CF");
}

TEST_CASE("statement values format as grouped dollars, trimmed reals, and text")
{
    CHECK(terminal::formatStatementValue(std::int64_t{416161000000}) == "416,161,000,000");
    CHECK(terminal::formatStatementValue(std::int64_t{-1234}) == "-1,234");
    CHECK(terminal::formatStatementValue(6.08) == "6.08");
    CHECK(terminal::formatStatementValue(1000.5) == "1,000.5");
    CHECK(terminal::formatStatementValue(std::string{"2025"}) == "2025");
    CHECK(terminal::formatStatementValue(terminal::StatementValue{}).empty());
}
