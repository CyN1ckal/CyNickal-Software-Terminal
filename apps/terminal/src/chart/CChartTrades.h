// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <span>
#include <string>
#include <vector>

namespace terminal {

// One ledger fill placed on a chart. bar_index is the bar the fill traded in, and
// price is in that chart's split-adjusted units. quantity keeps the fill's sign.
struct ChartTradeMarker
{
    int bar_index{};
    double price{};
    double quantity{};
    UnixSeconds ts{};
    std::string note;
};

// The instrument's share fills, placed on the newest bar that opened at or before
// each fill and whose close (end of the RTH session for a daily bar) is not before it;
// fills outside every bar are dropped, and so are option fills. Chart bars are split
// adjusted up to the last bar (adjustBarsForSplits), so each price is divided by every
// split that went ex after the fill and no later than the last bar. bars are in time order.
[[nodiscard]] std::vector<ChartTradeMarker> chartTradeMarkers(std::span<const Bar> bars,
                                                             std::span<const TradeFill> fills,
                                                             InstrumentId instrument_id,
                                                             std::span<const CorporateAction> actions);

}  // namespace terminal
