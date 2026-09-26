// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "trading/EquityCurve.h"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

constexpr terminal::InstrumentId kCurveAapl = 7;

terminal::TradeFill curveFill(terminal::TradeFillId id, terminal::UnixSeconds ts, double quantity, double price,
                              double fees = 0.0)
{
    terminal::TradeFill fill;
    fill.id = id;
    fill.kind = terminal::TradeAssetKind::Equity;
    fill.instrument_id = kCurveAapl;
    fill.ts = ts;
    fill.quantity = quantity;
    fill.price = price;
    fill.fees = fees;
    return fill;
}

terminal::LedgerCashFlow curveFlow(terminal::UnixSeconds ts, double amount)
{
    terminal::LedgerCashFlow flow;
    flow.ts = ts;
    flow.amount = amount;
    return flow;
}

terminal::MarkSeries curveMarks(std::vector<terminal::Mark> marks)
{
    terminal::MarkSeries series;
    series.instrument_id = kCurveAapl;
    series.marks = std::move(marks);
    return series;
}

}  // namespace

TEST_CASE("a deposit alone is equity with a zero return")
{
    const std::vector<terminal::LedgerCashFlow> flows{curveFlow(10, 10'000.0)};
    const std::vector<terminal::UnixSeconds> points{100, 200};
    const auto curve = terminal::equityCurve({}, flows, {}, {}, points);
    REQUIRE(curve.size() == 2);
    CHECK(curve[0].cash == 10'000.0);
    CHECK(curve[0].equity == 10'000.0);
    CHECK(curve[0].net_flow == 10'000.0);
    CHECK(curve[0].period_return == 0.0);
    CHECK(curve[1].net_flow == 0.0);
    CHECK(curve[1].contributed == 10'000.0);
    CHECK(curve[1].growth == 1.0);
    CHECK(curve[1].drawdown == 0.0);
    CHECK(curve[1].open_positions == 0);
}

TEST_CASE("an account with no capital has no return")
{
    const std::vector<terminal::UnixSeconds> points{100};
    const auto curve = terminal::equityCurve({}, {}, {}, {}, points);
    REQUIRE(curve.size() == 1);
    CHECK(std::isnan(curve[0].period_return));
    CHECK(curve[0].growth == 1.0);
}

TEST_CASE("a held position is marked to the latest close and drawdown follows the index")
{
    const std::vector<terminal::LedgerCashFlow> flows{curveFlow(0, 1'000.0)};
    const std::vector<terminal::TradeFill> fills{curveFill(1, 50, 10, 100.0)};
    const std::vector<terminal::MarkSeries> marks{curveMarks({{100, 110.0}, {200, 99.0}})};
    const std::vector<terminal::UnixSeconds> points{100, 150, 200};
    const auto curve = terminal::equityCurve(fills, flows, {}, marks, points);
    REQUIRE(curve.size() == 3);

    CHECK(curve[0].cash == Catch::Approx(0.0));
    CHECK(curve[0].market_value == Catch::Approx(1'100.0));
    CHECK(curve[0].period_return == Catch::Approx(0.1));
    CHECK(curve[0].open_positions == 1);

    // No new mark at 150: the 100 close still stands.
    CHECK(curve[1].market_value == Catch::Approx(1'100.0));
    CHECK(curve[1].period_return == Catch::Approx(0.0));

    CHECK(curve[2].equity == Catch::Approx(990.0));
    CHECK(curve[2].period_return == Catch::Approx(-0.1));
    CHECK(curve[2].growth == Catch::Approx(0.99));
    CHECK(curve[2].drawdown == Catch::Approx(-0.1));
}

TEST_CASE("a deposit mid-curve is not a return")
{
    const std::vector<terminal::LedgerCashFlow> flows{curveFlow(0, 1'000.0), curveFlow(150, 1'000.0)};
    const std::vector<terminal::UnixSeconds> points{100, 200};
    const auto curve = terminal::equityCurve({}, flows, {}, {}, points);
    REQUIRE(curve.size() == 2);
    CHECK(curve[1].equity == 2'000.0);
    CHECK(curve[1].net_flow == 1'000.0);
    CHECK(curve[1].contributed == 2'000.0);
    CHECK(curve[1].period_return == Catch::Approx(0.0));
    CHECK(curve[1].growth == Catch::Approx(1.0));
}

TEST_CASE("a split between points keeps the value of an as-traded mark")
{
    const std::vector<terminal::LedgerCashFlow> flows{curveFlow(0, 4'000.0)};
    const std::vector<terminal::TradeFill> fills{curveFill(1, 50, 10, 400.0)};
    terminal::CorporateAction split;
    split.instrument_id = kCurveAapl;
    split.ex_ts = 150;
    split.type = terminal::CorporateActionType::Split;
    split.split_ratio = 4.0;
    const std::vector<terminal::CorporateAction> actions{split};
    const std::vector<terminal::MarkSeries> marks{curveMarks({{100, 400.0}, {200, 100.0}})};
    const std::vector<terminal::UnixSeconds> points{100, 200};
    const auto curve = terminal::equityCurve(fills, flows, actions, marks, points);
    REQUIRE(curve.size() == 2);
    CHECK(curve[1].market_value == Catch::Approx(4'000.0));
    CHECK(curve[1].period_return == Catch::Approx(0.0));
}

TEST_CASE("unmarked shares hold at cost and options at their last trade")
{
    const std::vector<terminal::LedgerCashFlow> flows{curveFlow(0, 1'000.0)};
    terminal::TradeFill put = curveFill(1, 10, 1, 2.0);
    put.kind = terminal::TradeAssetKind::Option;
    put.expiration = 20261016;
    put.expiration_type = terminal::OptionExpirationType::Monthly;
    put.strike = 90.0;
    put.right = terminal::OptionRight::Put;
    terminal::TradeFill second_put = put;
    second_put.id = 2;
    second_put.ts = 150;
    second_put.price = 3.0;
    const std::vector<terminal::TradeFill> fills{put, curveFill(3, 20, 2, 50.0), second_put};
    const std::vector<terminal::UnixSeconds> points{100, 200};
    const auto curve = terminal::equityCurve(fills, flows, {}, {}, points);
    REQUIRE(curve.size() == 2);
    CHECK(curve[0].market_value == Catch::Approx(200.0 + 100.0));
    CHECK(curve[0].equity == Catch::Approx(1'000.0));
    CHECK(curve[0].open_positions == 2);
    CHECK(curve[1].market_value == Catch::Approx((2 * 3.0 * 100.0) + 100.0));
    CHECK(curve[1].equity == Catch::Approx(1'100.0));
}

TEST_CASE("equity curve points must ascend")
{
    const std::vector<terminal::UnixSeconds> repeated{100, 100};
    CHECK_THROWS_WITH(terminal::equityCurve({}, {}, {}, {}, repeated), "equity curve points are not ascending");
    const std::vector<terminal::UnixSeconds> backward{200, 100};
    CHECK_THROWS_WITH(terminal::equityCurve({}, {}, {}, {}, backward), "equity curve points are not ascending");
}

TEST_CASE("close marks land when the bar ends and mark times merge series")
{
    terminal::Bar daily;
    daily.instrument_id = kCurveAapl;
    daily.timeframe_s = terminal::kTimeframe1d;
    daily.ts = 1'000'000;
    daily.open = daily.high = daily.low = daily.close = 10.0;
    terminal::Bar minute = daily;
    minute.timeframe_s = terminal::kTimeframe1m;
    minute.ts = 2'000'040;
    minute.close = 11.0;
    minute.high = 11.0;
    terminal::Bar broken = daily;
    broken.high = 5.0;
    const std::vector<terminal::Bar> bars{daily, minute, broken};

    const auto series = terminal::closeMarks(kCurveAapl, bars);
    CHECK(series.instrument_id == kCurveAapl);
    REQUIRE(series.marks.size() == 2);
    CHECK(series.marks[0].ts == 1'000'000 + terminal::kUsRthDurationS);
    CHECK(series.marks[0].price == 10.0);
    CHECK(series.marks[1].ts == 2'000'100);
    CHECK(series.marks[1].price == 11.0);

    terminal::MarkSeries other;
    other.instrument_id = 8;
    other.marks = {{2'000'100, 1.0}, {50, 1.0}, {3'000'000, 1.0}};
    const std::vector<terminal::MarkSeries> both{series, other};
    const auto times = terminal::markTimes(both, 100, 2'500'000);
    const std::vector<terminal::UnixSeconds> expected{1'000'000 + terminal::kUsRthDurationS, 2'000'100};
    CHECK(times == expected);
}
