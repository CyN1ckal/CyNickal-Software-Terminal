// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartTrades.h"

#include <vector>

namespace {

constexpr terminal::InstrumentId kTradesId = 3;
// 2026-06-01 13:30 UTC, the 09:30 open in New York.
constexpr terminal::UnixSeconds kTradesStart = 1'780'320'600;

std::vector<terminal::Bar> tradesDailyBars(int count)
{
    std::vector<terminal::Bar> bars;
    for (int day = 0; day < count; ++day)
    {
        terminal::Bar bar;
        bar.instrument_id = kTradesId;
        bar.timeframe_s = terminal::kTimeframe1d;
        bar.ts = kTradesStart + (static_cast<terminal::UnixSeconds>(day) * 86'400);
        bar.open = bar.high = bar.low = bar.close = 50.0;
        bars.push_back(bar);
    }
    return bars;
}

terminal::TradeFill tradesFill(terminal::UnixSeconds ts, double quantity, double price)
{
    terminal::TradeFill fill;
    fill.kind = terminal::TradeAssetKind::Equity;
    fill.instrument_id = kTradesId;
    fill.ts = ts;
    fill.quantity = quantity;
    fill.price = price;
    return fill;
}

}  // namespace

TEST_CASE("fills land on the bar whose session they traded in")
{
    const auto bars = tradesDailyBars(5);
    auto noted = tradesFill(kTradesStart + (2 * 86'400) + 3'600, 10, 51.0);
    noted.note = "breakout";
    auto other = noted;
    other.instrument_id = kTradesId + 1;
    auto option = noted;
    option.kind = terminal::TradeAssetKind::Option;
    const std::vector<terminal::TradeFill> fills{
        tradesFill(kTradesStart - 60, 1, 50.0),                                             // before the first bar
        noted,                                                                               // mid-session, day 2
        tradesFill(kTradesStart + (3 * 86'400) + terminal::kUsRthDurationS, -10, 52.0),     // at day 3's close
        tradesFill(kTradesStart + (3 * 86'400) + terminal::kUsRthDurationS + 60, -1, 52.0), // after hours
        tradesFill(kTradesStart + (4 * 86'400) + terminal::kUsRthDurationS + 60, -1, 52.0), // after the last bar
        other,
        option,
    };
    const auto markers = terminal::chartTradeMarkers(bars, fills, kTradesId, {});
    REQUIRE(markers.size() == 2);
    CHECK(markers[0].bar_index == 2);
    CHECK(markers[0].price == 51.0);
    CHECK(markers[0].quantity == 10.0);
    CHECK(markers[0].note == "breakout");
    CHECK(markers[1].bar_index == 3);
    CHECK(markers[1].quantity == -10.0);
}

TEST_CASE("intraday fills land on the bar that spans them")
{
    std::vector<terminal::Bar> bars;
    for (int index = 0; index < 4; ++index)
    {
        terminal::Bar bar;
        bar.instrument_id = kTradesId;
        bar.timeframe_s = 300;
        bar.ts = kTradesStart + (index * 300);
        bar.open = bar.high = bar.low = bar.close = 10.0;
        bars.push_back(bar);
    }
    const std::vector<terminal::TradeFill> fills{tradesFill(kTradesStart + 599, 1, 10.0),
                                                 tradesFill(kTradesStart + 600, 1, 10.0)};
    const auto markers = terminal::chartTradeMarkers(bars, fills, kTradesId, {});
    REQUIRE(markers.size() == 2);
    CHECK(markers[0].bar_index == 1);
    CHECK(markers[1].bar_index == 2);
}

TEST_CASE("marker prices follow the chart's split adjustment")
{
    const auto bars = tradesDailyBars(6);
    terminal::CorporateAction within;
    within.instrument_id = kTradesId;
    within.type = terminal::CorporateActionType::Split;
    within.split_ratio = 4.0;
    within.ex_ts = bars[3].ts;
    terminal::CorporateAction beyond = within;
    beyond.split_ratio = 2.0;
    beyond.ex_ts = bars.back().ts + 86'400;  // after the last bar: the chart is not adjusted for it yet
    const std::vector<terminal::CorporateAction> actions{within, beyond};
    const std::vector<terminal::TradeFill> fills{
        tradesFill(bars[1].ts + 60, 10, 200.0),  // before the split: as-traded 200 is 50 on this chart
        tradesFill(bars[4].ts + 60, -40, 55.0),  // after it: unchanged
    };
    const auto markers = terminal::chartTradeMarkers(bars, fills, kTradesId, actions);
    REQUIRE(markers.size() == 2);
    CHECK(markers[0].price == Catch::Approx(50.0));
    CHECK(markers[0].quantity == Catch::Approx(40.0));
    CHECK(markers[1].price == Catch::Approx(55.0));
    CHECK(markers[1].quantity == Catch::Approx(-40.0));
    CHECK(terminal::chartTradeMarkers({}, fills, kTradesId, actions).empty());
}
