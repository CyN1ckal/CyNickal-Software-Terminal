// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <array>
#include <span>
#include <string>
#include <vector>

namespace terminal {

// Columns of one statement grid. The four newest fiscal period ends, most recent first.
inline constexpr int kStatementSheetPeriods = 4;

struct StatementSheetSlot
{
    bool present{false};
    StatementValue value;
};

struct StatementSheetRow
{
    std::string line_item;
    std::array<StatementSheetSlot, kStatementSheetPeriods> slots{};
};

struct StatementSheet
{
    std::array<std::string, kStatementSheetPeriods> periods{};
    int period_count{0};
    std::vector<StatementSheetRow> rows;
};

// TTM and the fiscalYear / fiscalQuarter label rows are omitted.
// Known income summary lines stay above the remaining vendor keys.
[[nodiscard]] StatementSheet buildStatementSheet(std::span<const StatementCell> cells);

[[nodiscard]] std::string formatStatementValue(const StatementValue& value);

}  // namespace terminal
