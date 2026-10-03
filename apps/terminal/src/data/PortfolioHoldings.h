// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstddef>
#include <span>
#include <vector>

namespace terminal {

// What aggregateHolding did with the incoming position.
enum class HoldingAggregateAction : std::uint8_t
{
    Added,
    Combined,
    Removed,
};

struct HoldingAggregate
{
    HoldingAggregateAction action{HoldingAggregateAction::Added};
    // Index of the line that was added or combined. For Removed, the index of
    // the line that was erased, before the erase.
    std::size_t index{0};
};

// One line per position. A share line is one instrument, whether it was entered
// as equity or ETF. An option line is one contract: underlying, expiration,
// expiration type, strike, and right. Cash is one line.
//
// The same position adds its quantity to the existing line and stays where that
// line already sits. A total of zero drops the line, because a flat position is
// not a holding. A strike within 0.0001 is the same contract.
[[nodiscard]] HoldingAggregate aggregateHolding(std::vector<PortfolioHolding>& holdings,
                                                PortfolioHolding incoming);

// Folds every line of `holdings`, first occurrence kept, later copies combined.
[[nodiscard]] std::vector<PortfolioHolding> aggregateHoldings(std::span<const PortfolioHolding> holdings);

// True when both lines are the same position. Quantity is ignored.
[[nodiscard]] bool sameHolding(const PortfolioHolding& held, const PortfolioHolding& incoming);

// What retargetHolding did with the line.
enum class HoldingRetargetAction : std::uint8_t
{
    Updated,
    Combined,
    Removed,
    Rejected,
};

struct HoldingRetarget
{
    HoldingRetargetAction action{HoldingRetargetAction::Rejected};
    // Updated and Combined: the line that now holds the position.
    // Removed: the first index erased, before either erase. Rejected: `index`.
    std::size_t index{0};
};

// Replaces the instrument on `holdings[index]` and keeps that line's quantity.
// A share takes the resolved class, equity or ETF. An option keeps its contract
// and takes the new underlying. A line already holding that position absorbs the
// quantity. Cash, a missing FIGI, an out-of-range index, or an asset class the
// line cannot hold leaves the book unchanged.
[[nodiscard]] HoldingRetarget retargetHolding(std::vector<PortfolioHolding>& holdings, std::size_t index,
                                              const Instrument& instrument);

}  // namespace terminal
