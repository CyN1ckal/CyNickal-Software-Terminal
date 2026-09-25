// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "options/OptionPayoff.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t kPayoffExpiry = 20261016;

terminal::PayoffLeg payoffOption(terminal::LegInstrument instrument, double strike, double quantity, double price)
{
    terminal::PayoffLeg leg;
    leg.instrument = instrument;
    leg.strike = strike;
    leg.expiration = kPayoffExpiry;
    leg.quantity = quantity;
    leg.price = price;
    return leg;
}

terminal::PayoffLeg payoffShares(double shares, double price)
{
    terminal::PayoffLeg leg;
    leg.instrument = terminal::LegInstrument::Underlying;
    leg.quantity = shares;
    leg.price = price;
    leg.multiplier = 1.0;
    return leg;
}

void checkBreakevens(const std::vector<double>& actual, const std::vector<double>& expected)
{
    REQUIRE(actual.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        CHECK(actual[index] == Catch::Approx(expected[index]));
    }
}

}  // namespace

using terminal::LegInstrument;

static_assert(terminal::legPayoff(terminal::PayoffLeg{LegInstrument::Call, 100.0, 20261016, 1.0, 5.0, 100.0}, 110.0) ==
              500.0);
static_assert(terminal::legPayoff(terminal::PayoffLeg{LegInstrument::Put, 100.0, 20261016, -2.0, 3.0, 100.0}, 120.0) ==
              600.0);

TEST_CASE("settlement value is intrinsic for options and spot for shares")
{
    const terminal::PayoffLeg call = payoffOption(LegInstrument::Call, 100.0, 1.0, 0.0);
    const terminal::PayoffLeg put = payoffOption(LegInstrument::Put, 100.0, 1.0, 0.0);
    const terminal::PayoffLeg shares = payoffShares(1.0, 0.0);
    CHECK(terminal::settlementValue(call, 90.0) == 0.0);
    CHECK(terminal::settlementValue(call, 112.5) == 12.5);
    CHECK(terminal::settlementValue(put, 90.0) == 10.0);
    CHECK(terminal::settlementValue(put, 112.5) == 0.0);
    CHECK(terminal::settlementValue(shares, 87.0) == 87.0);
}

TEST_CASE("a long call risks its premium for unbounded upside")
{
    const std::vector<terminal::PayoffLeg> legs{payoffOption(LegInstrument::Call, 100.0, 1.0, 5.0)};
    CHECK(terminal::strategyPayoff(legs, 90.0) == -500.0);
    CHECK(terminal::strategyPayoff(legs, 110.0) == 500.0);

    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    CHECK(summary.net_cost == 500.0);
    CHECK_FALSE(summary.max_profit.has_value());
    REQUIRE(summary.max_loss.has_value());
    CHECK(*summary.max_loss == -500.0);
    CHECK(summary.upside_slope == 100.0);
    checkBreakevens(summary.breakevens, {105.0});
}

TEST_CASE("a short call has unbounded loss")
{
    const std::vector<terminal::PayoffLeg> legs{payoffOption(LegInstrument::Call, 100.0, -1.0, 5.0)};
    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    CHECK(summary.net_cost == -500.0);
    REQUIRE(summary.max_profit.has_value());
    CHECK(*summary.max_profit == 500.0);
    CHECK_FALSE(summary.max_loss.has_value());
    checkBreakevens(summary.breakevens, {105.0});
}

TEST_CASE("a short put's loss stops where the underlying reaches zero")
{
    const std::vector<terminal::PayoffLeg> legs{payoffOption(LegInstrument::Put, 100.0, -1.0, 4.0)};
    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    REQUIRE(summary.max_profit.has_value());
    REQUIRE(summary.max_loss.has_value());
    CHECK(*summary.max_profit == 400.0);
    CHECK(*summary.max_loss == -9600.0);
    CHECK(summary.upside_slope == 0.0);
    checkBreakevens(summary.breakevens, {96.0});
}

TEST_CASE("a bull call spread caps both sides")
{
    const std::vector<terminal::PayoffLeg> legs{
        payoffOption(LegInstrument::Call, 100.0, 1.0, 5.0),
        payoffOption(LegInstrument::Call, 110.0, -1.0, 2.0),
    };
    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    CHECK(summary.net_cost == Catch::Approx(300.0));
    REQUIRE(summary.max_profit.has_value());
    REQUIRE(summary.max_loss.has_value());
    CHECK(*summary.max_profit == Catch::Approx(700.0));
    CHECK(*summary.max_loss == Catch::Approx(-300.0));
    checkBreakevens(summary.breakevens, {103.0});
}

TEST_CASE("an iron condor keeps its credit between the short strikes")
{
    const std::vector<terminal::PayoffLeg> legs{
        payoffOption(LegInstrument::Put, 90.0, 1.0, 1.0),
        payoffOption(LegInstrument::Put, 95.0, -1.0, 2.5),
        payoffOption(LegInstrument::Call, 105.0, -1.0, 2.5),
        payoffOption(LegInstrument::Call, 110.0, 1.0, 1.0),
    };
    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    CHECK(summary.net_cost == Catch::Approx(-300.0));
    REQUIRE(summary.max_profit.has_value());
    REQUIRE(summary.max_loss.has_value());
    CHECK(*summary.max_profit == Catch::Approx(300.0));
    CHECK(*summary.max_loss == Catch::Approx(-200.0));
    CHECK(summary.upside_slope == 0.0);
    checkBreakevens(summary.breakevens, {92.0, 108.0});
    CHECK(terminal::strategyPayoff(legs, 100.0) == Catch::Approx(300.0));
}

TEST_CASE("a long straddle breaks even on both sides of the strike")
{
    const std::vector<terminal::PayoffLeg> legs{
        payoffOption(LegInstrument::Call, 100.0, 1.0, 3.0),
        payoffOption(LegInstrument::Put, 100.0, 1.0, 2.0),
    };
    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    CHECK_FALSE(summary.max_profit.has_value());
    REQUIRE(summary.max_loss.has_value());
    CHECK(*summary.max_loss == -500.0);
    checkBreakevens(summary.breakevens, {95.0, 105.0});
}

TEST_CASE("a covered call mixes shares and a short call")
{
    const std::vector<terminal::PayoffLeg> legs{
        payoffShares(100.0, 100.0),
        payoffOption(LegInstrument::Call, 105.0, -1.0, 2.0),
    };
    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    CHECK(summary.net_cost == Catch::Approx(9800.0));
    CHECK(summary.upside_slope == 0.0);
    REQUIRE(summary.max_profit.has_value());
    REQUIRE(summary.max_loss.has_value());
    CHECK(*summary.max_profit == Catch::Approx(700.0));
    CHECK(*summary.max_loss == Catch::Approx(-9800.0));
    checkBreakevens(summary.breakevens, {98.0});
}

TEST_CASE("a payoff that lies on zero reports only the ends of that stretch")
{
    const std::vector<terminal::PayoffLeg> legs{payoffOption(LegInstrument::Call, 100.0, 1.0, 0.0)};
    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    checkBreakevens(summary.breakevens, {0.0, 100.0});
}

TEST_CASE("the payoff of combined legs is the sum of each group's payoff")
{
    const std::vector<terminal::PayoffLeg> straddle{
        payoffOption(LegInstrument::Call, 100.0, 1.0, 3.0),
        payoffOption(LegInstrument::Put, 100.0, 1.0, 2.0),
    };
    const std::vector<terminal::PayoffLeg> spread{
        payoffOption(LegInstrument::Call, 95.0, -2.0, 6.0),
        payoffOption(LegInstrument::Call, 115.0, 2.0, 0.5),
    };
    std::vector<terminal::PayoffLeg> combined = straddle;
    REQUIRE_FALSE(terminal::appendLegs(combined, spread).has_value());
    REQUIRE(combined.size() == 4);
    for (double spot = 0.0; spot <= 200.0; spot += 2.5)
    {
        CHECK(terminal::strategyPayoff(combined, spot) ==
              Catch::Approx(terminal::strategyPayoff(straddle, spot) + terminal::strategyPayoff(spread, spot)));
    }
    CHECK(terminal::netCost(combined) == Catch::Approx(terminal::netCost(straddle) + terminal::netCost(spread)));
}

TEST_CASE("summary extremes and breakevens agree with a dense sample")
{
    const std::vector<terminal::PayoffLeg> legs{
        payoffOption(LegInstrument::Put, 80.0, 2.0, 1.25),
        payoffOption(LegInstrument::Put, 95.0, -3.0, 4.0),
        payoffOption(LegInstrument::Call, 105.0, -1.0, 3.5),
        payoffOption(LegInstrument::Call, 120.0, 1.0, 0.75),
        payoffShares(50.0, 99.0),
    };
    const terminal::PayoffSummary summary = terminal::summarizePayoff(legs);
    double worst = std::numeric_limits<double>::infinity();
    for (int step = 0; step <= 40000; ++step)
    {
        worst = std::min(worst, terminal::strategyPayoff(legs, step * 0.01));
    }
    REQUIRE(summary.max_loss.has_value());
    CHECK(*summary.max_loss == Catch::Approx(worst));
    // Short one call, long one call, and 50 shares above 120.
    CHECK(summary.upside_slope == Catch::Approx(50.0));
    CHECK_FALSE(summary.max_profit.has_value());
    REQUIRE_FALSE(summary.breakevens.empty());
    for (const double breakeven : summary.breakevens)
    {
        CHECK(std::fabs(terminal::strategyPayoff(legs, breakeven)) < 1e-6);
    }
}

TEST_CASE("validation rejects legs that cannot share one expiration payoff")
{
    CHECK(terminal::validateLegs({}) == std::optional<std::string>("add a leg"));

    std::vector<terminal::PayoffLeg> legs{payoffOption(LegInstrument::Call, 100.0, 0.0, 1.0)};
    CHECK(terminal::validateLegs(legs).has_value());

    legs = {payoffOption(LegInstrument::Call, 100.0, 1.0, -1.0)};
    CHECK(terminal::validateLegs(legs).has_value());

    legs = {payoffOption(LegInstrument::Call, std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0)};
    CHECK(terminal::validateLegs(legs).has_value());

    legs = {payoffOption(LegInstrument::Put, 0.0, 1.0, 1.0)};
    CHECK(terminal::validateLegs(legs).has_value());

    terminal::PayoffLeg later = payoffOption(LegInstrument::Put, 100.0, 1.0, 1.0);
    later.expiration = kPayoffExpiry + 7;
    legs = {payoffOption(LegInstrument::Call, 100.0, 1.0, 1.0), later};
    CHECK(terminal::validateLegs(legs).has_value());

    legs = {payoffShares(-100.0, 50.0), payoffOption(LegInstrument::Put, 50.0, 1.0, 1.0)};
    CHECK_FALSE(terminal::validateLegs(legs).has_value());
    REQUIRE(terminal::strategyExpiration(legs).has_value());
    CHECK(*terminal::strategyExpiration(legs) == kPayoffExpiry);
    CHECK_FALSE(terminal::strategyExpiration(std::vector<terminal::PayoffLeg>{payoffShares(1.0, 1.0)}).has_value());
}

TEST_CASE("a rejected append leaves the strategy unchanged")
{
    std::vector<terminal::PayoffLeg> strategy{payoffOption(LegInstrument::Call, 100.0, 1.0, 1.0)};
    terminal::PayoffLeg later = payoffOption(LegInstrument::Call, 105.0, -1.0, 0.5);
    later.expiration = kPayoffExpiry + 7;
    const std::vector<terminal::PayoffLeg> incoming{payoffOption(LegInstrument::Put, 95.0, 1.0, 1.0), later};
    CHECK(terminal::appendLegs(strategy, incoming).has_value());
    CHECK(strategy.size() == 1);
    CHECK(terminal::appendLegs(strategy, {}).has_value());
    CHECK(strategy.size() == 1);
}

TEST_CASE("the payoff path has every kink and zero crossing in range")
{
    const std::vector<terminal::PayoffLeg> legs{
        payoffOption(LegInstrument::Call, 100.0, 1.0, 3.0),
        payoffOption(LegInstrument::Put, 100.0, 1.0, 2.0),
    };
    const terminal::PayoffPath path = terminal::payoffPath(legs, 80.0, 120.0);
    const std::vector<double> spots{80.0, 95.0, 100.0, 105.0, 120.0};
    const std::vector<double> profits{1500.0, 0.0, -500.0, 0.0, 1500.0};
    REQUIRE(path.spots.size() == spots.size());
    REQUIRE(path.profits.size() == profits.size());
    for (std::size_t index = 0; index < spots.size(); ++index)
    {
        CHECK(path.spots[index] == Catch::Approx(spots[index]));
        CHECK(path.profits[index] == Catch::Approx(profits[index]).margin(1e-9));
    }

    const terminal::PayoffPath clamped = terminal::payoffPath(legs, -50.0, 10.0);
    REQUIRE_FALSE(clamped.spots.empty());
    CHECK(clamped.spots.front() == 0.0);
    CHECK(terminal::payoffPath(legs, 120.0, 80.0).spots.empty());
    CHECK(terminal::payoffPath(legs, 0.0, std::numeric_limits<double>::infinity()).spots.empty());
}

TEST_CASE("the default window frames strikes, breakevens, and spot")
{
    const std::vector<terminal::PayoffLeg> legs{
        payoffOption(LegInstrument::Call, 100.0, 1.0, 5.0),
        payoffOption(LegInstrument::Call, 110.0, -1.0, 2.0),
    };
    const terminal::SpotRange range = terminal::defaultSpotRange(legs, 90.0);
    CHECK(range.low < 90.0);
    CHECK(range.high > 110.0);
    CHECK(range.low >= 0.0);

    const terminal::SpotRange empty = terminal::defaultSpotRange({}, std::numeric_limits<double>::quiet_NaN());
    CHECK(empty.high > empty.low);
}
