// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartTrades.h"

#include "market_data/Adjust.h"
#include "market_data/Time.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace terminal {

std::vector<ChartTradeMarker> chartTradeMarkers(std::span<const Bar> bars,
                                               std::span<const TradeFill> fills,
                                               InstrumentId instrument_id,
                                               std::span<const CorporateAction> actions)
{
    std::vector<ChartTradeMarker> markers;
    if (bars.empty())
    {
        return markers;
    }
    const UnixSeconds last_ts = bars.back().ts;
    for (const TradeFill& fill : fills)
    {
        if (fill.kind == TradeAssetKind::Option || fill.instrument_id != instrument_id)
        {
            continue;
        }
        // The newest bar that opened at or before the fill.
        const auto after = std::ranges::upper_bound(bars, fill.ts, {}, &Bar::ts);
        if (after == bars.begin())
        {
            continue;
        }
        const auto index = static_cast<std::size_t>(std::distance(bars.begin(), after) - 1);
        if (fill.ts > barCloseTime(bars[index]))
        {
            continue;
        }
        const double factor = splitFactorBetween(actions, instrument_id, fill.ts, last_ts);
        ChartTradeMarker marker;
        marker.bar_index = static_cast<int>(index);
        marker.price = fill.price / factor;
        marker.quantity = fill.quantity * factor;
        marker.ts = fill.ts;
        marker.note = fill.note.value_or(std::string{});
        markers.push_back(std::move(marker));
    }
    return markers;
}

}  // namespace terminal
