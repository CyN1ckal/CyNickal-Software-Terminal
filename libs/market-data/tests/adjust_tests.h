// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "market_data/Adjust.h"

#include <vector>

namespace {

terminal::Bar priceBar(terminal::UnixSeconds ts, double close, double volume)
{
    terminal::Bar bar;
    bar.ts = ts;
    bar.open = close;
    bar.high = close;
    bar.low = close;
    bar.close = close;
    bar.volume = volume;
    return bar;
}

terminal::CorporateAction splitAt(terminal::UnixSeconds ex_ts, double ratio)
{
    terminal::CorporateAction action;
    action.ex_ts = ex_ts;
    action.type = terminal::CorporateActionType::Split;
    action.split_ratio = ratio;
    return action;
}

}  // namespace

TEST_CASE("adjustBarsForSplits divides pre-split NVDA prices and scales volume")
{
    const terminal::UnixSeconds ex_ts = 1718026200;
    std::vector<terminal::Bar> bars;
    bars.push_back(priceBar(1717767000, 1208.88, 41238500.0));
    bars.push_back(priceBar(ex_ts, 121.79, 222551100.0));
    const std::vector<terminal::CorporateAction> actions{splitAt(ex_ts, 10.0)};

    const std::vector<terminal::Bar> adjusted = terminal::adjustBarsForSplits(bars, actions);
    REQUIRE(adjusted.size() == 2);
    CHECK(adjusted[0].close == 1208.88 / 10.0);
    CHECK(adjusted[0].open == 1208.88 / 10.0);
    CHECK(adjusted[0].volume == 41238500.0 * 10.0);
    CHECK(adjusted[1].close == 121.79);
    CHECK(adjusted[1].volume == 222551100.0);
}

TEST_CASE("adjustBarsForSplits stacks ratios and ignores dividends")
{
    terminal::Bar bar = priceBar(100, 400.0, 80.0);
    terminal::CorporateAction dividend;
    dividend.ex_ts = 150;
    dividend.type = terminal::CorporateActionType::Dividend;
    dividend.amount = 1.0;
    const std::vector<terminal::CorporateAction> actions{splitAt(200, 4.0), dividend, splitAt(300, 10.0)};

    const std::vector<terminal::Bar> adjusted =
        terminal::adjustBarsForSplits(std::vector<terminal::Bar>{bar}, actions);
    REQUIRE(adjusted.size() == 1);
    CHECK(adjusted[0].close == 400.0 / 40.0);
    CHECK(adjusted[0].volume == 80.0 * 40.0);
}

TEST_CASE("adjustBarsForSplits reverse split multiplies price")
{
    const terminal::Bar bar = priceBar(100, 10.0, 100.0);
    const std::vector<terminal::CorporateAction> actions{splitAt(200, 0.2)};
    const std::vector<terminal::Bar> adjusted =
        terminal::adjustBarsForSplits(std::vector<terminal::Bar>{bar}, actions);
    REQUIRE(adjusted.size() == 1);
    CHECK(adjusted[0].close == 10.0 / 0.2);
    CHECK(adjusted[0].volume == 100.0 * 0.2);
}

TEST_CASE("adjustBarsForSplits with no splits returns the bars")
{
    const terminal::Bar bar = priceBar(100, 10.0, 5.0);
    const std::vector<terminal::Bar> adjusted =
        terminal::adjustBarsForSplits(std::vector<terminal::Bar>{bar}, {});
    REQUIRE(adjusted.size() == 1);
    CHECK(adjusted[0].close == 10.0);
    CHECK(adjusted[0].volume == 5.0);
}
