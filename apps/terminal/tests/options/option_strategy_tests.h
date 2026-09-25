// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "options/OptionPayoff.h"
#include "options/OptionStrategy.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr terminal::SessionDate kStrategyExpiry = 20261016;

terminal::OptionQuote strategyQuote(terminal::OptionRight right, double strike, double bid, double ask)
{
    terminal::OptionQuote quote;
    quote.expiration = kStrategyExpiry;
    quote.right = right;
    quote.strike = strike;
    quote.bid = bid;
    quote.ask = ask;
    quote.mid = (bid + ask) * 0.5;
    quote.last = ask;
    return quote;
}

// Strikes 80..120 by 5 around spot 100. Every contract is intrinsic plus 2 of
// time value with a 0.10 wide market, so parity lands exactly on spot.
std::vector<terminal::OptionQuote> strategyChain(double spot = 100.0)
{
    std::vector<terminal::OptionQuote> chain;
    for (double strike = 80.0; strike <= 120.0; strike += 5.0)
    {
        const double call = std::max(spot - strike, 0.0) + 2.0;
        const double put = std::max(strike - spot, 0.0) + 2.0;
        chain.push_back(strategyQuote(terminal::OptionRight::Call, strike, call - 0.05, call + 0.05));
        chain.push_back(strategyQuote(terminal::OptionRight::Put, strike, put - 0.05, put + 0.05));
    }
    return chain;
}

std::vector<double> strategySlotStrikes(const terminal::StrategyTemplate& recipe,
                                        const std::vector<double>& strikes,
                                        double spot)
{
    const std::optional<std::size_t> atm = terminal::nearestStrikeIndex(strikes, spot);
    std::vector<double> picked;
    for (const std::size_t index : terminal::defaultSlotStrikes(recipe, strikes.size(), atm.value_or(0)))
    {
        picked.push_back(strikes[index]);
    }
    return picked;
}

}  // namespace

TEST_CASE("quote price follows the basis and falls back when a side is missing")
{
    terminal::OptionQuote quote = strategyQuote(terminal::OptionRight::Call, 100.0, 1.9, 2.1);
    quote.last = 2.4;
    CHECK(terminal::quotePrice(quote, terminal::PriceBasis::Mid, true) == Catch::Approx(2.0));
    CHECK(terminal::quotePrice(quote, terminal::PriceBasis::Natural, true) == Catch::Approx(2.1));
    CHECK(terminal::quotePrice(quote, terminal::PriceBasis::Natural, false) == Catch::Approx(1.9));
    CHECK(terminal::quotePrice(quote, terminal::PriceBasis::Last, false) == Catch::Approx(2.4));

    quote.mid = 0.0;
    CHECK(terminal::quotePrice(quote, terminal::PriceBasis::Mid, true) == Catch::Approx(2.0));
    quote.ask = 0.0;
    CHECK(terminal::quotePrice(quote, terminal::PriceBasis::Mid, true) == Catch::Approx(2.4));
    CHECK(terminal::quotePrice(quote, terminal::PriceBasis::Natural, true) == Catch::Approx(2.4));
    quote.bid = 0.0;
    CHECK(terminal::quotePrice(quote, terminal::PriceBasis::Natural, false) == 0.0);
}

TEST_CASE("a chain quote becomes a contract leg and shares become a unit leg")
{
    const terminal::OptionQuote quote = strategyQuote(terminal::OptionRight::Put, 95.0, 1.0, 1.2);
    const terminal::PayoffLeg leg = terminal::legFromQuote(quote, -3.0, 1.0);
    CHECK(leg.instrument == terminal::LegInstrument::Put);
    CHECK(leg.strike == 95.0);
    CHECK(leg.expiration == kStrategyExpiry);
    CHECK(leg.quantity == -3.0);
    CHECK(leg.price == 1.0);
    CHECK(leg.multiplier == terminal::kContractMultiplier);

    const terminal::PayoffLeg shares = terminal::underlyingLeg(200.0, 101.5);
    CHECK(shares.instrument == terminal::LegInstrument::Underlying);
    CHECK(shares.multiplier == 1.0);
    CHECK(shares.expiration == 0);
}

TEST_CASE("listed strikes are sorted and unique, and the nearest tie takes the lower")
{
    const std::vector<double> strikes = terminal::listedStrikes(strategyChain());
    REQUIRE(strikes.size() == 9);
    CHECK(strikes.front() == 80.0);
    CHECK(strikes.back() == 120.0);
    CHECK(std::ranges::is_sorted(strikes));

    CHECK(terminal::nearestStrikeIndex(strikes, 101.0) == std::optional<std::size_t>(4));
    CHECK(terminal::nearestStrikeIndex(strikes, 102.5) == std::optional<std::size_t>(4));
    CHECK(terminal::nearestStrikeIndex(strikes, 1000.0) == std::optional<std::size_t>(8));
    CHECK_FALSE(terminal::nearestStrikeIndex({}, 100.0).has_value());
}

TEST_CASE("parity recovers spot from the most at-the-money strike")
{
    const std::optional<double> spot = terminal::parityImpliedSpot(strategyChain(101.0));
    REQUIRE(spot.has_value());
    CHECK(*spot == Catch::Approx(101.0));

    std::vector<terminal::OptionQuote> calls_only;
    for (const terminal::OptionQuote& quote : strategyChain())
    {
        if (quote.right == terminal::OptionRight::Call)
        {
            calls_only.push_back(quote);
        }
    }
    CHECK_FALSE(terminal::parityImpliedSpot(calls_only).has_value());
}

TEST_CASE("every template is well formed and builds from a full chain")
{
    const std::vector<terminal::OptionQuote> chain = strategyChain();
    const std::vector<double> strikes = terminal::listedStrikes(chain);
    std::set<std::string_view> ids;
    for (const terminal::StrategyTemplate& recipe : terminal::strategyTemplates())
    {
        INFO(recipe.id);
        CHECK(ids.insert(recipe.id).second);
        CHECK(terminal::findStrategyTemplate(recipe.id) == &recipe);
        REQUIRE_FALSE(recipe.legs.empty());
        for (const terminal::TemplateLeg& leg : recipe.legs)
        {
            CHECK(leg.ratio != 0.0);
            if (leg.instrument == terminal::LegInstrument::Underlying)
            {
                CHECK(leg.slot == -1);
                continue;
            }
            CHECK(leg.slot >= 0);
            CHECK(static_cast<std::size_t>(leg.slot) < recipe.slots.size());
        }
        for (std::size_t index = 1; index < recipe.slots.size(); ++index)
        {
            CHECK(recipe.slots[index].offset > recipe.slots[index - 1].offset);
        }

        const std::vector<double> slot_strikes = strategySlotStrikes(recipe, strikes, 100.0);
        terminal::TemplateInputs inputs;
        inputs.slot_strikes = slot_strikes;
        inputs.quantity = 2.0;
        inputs.underlying_price = 100.0;
        const terminal::TemplateBuild build = terminal::buildTemplate(recipe, chain, inputs);
        CHECK(build.error.empty());
        CHECK(build.legs.size() == recipe.legs.size());
        CHECK_FALSE(terminal::validateLegs(build.legs).has_value());
    }
    CHECK(terminal::findStrategyTemplate("no_such_strategy") == nullptr);
}

TEST_CASE("an iron condor is priced at the natural market")
{
    const terminal::StrategyTemplate* recipe = terminal::findStrategyTemplate("iron_condor");
    REQUIRE(recipe != nullptr);
    const std::vector<terminal::OptionQuote> chain = strategyChain();
    const std::vector<double> slot_strikes{85.0, 95.0, 105.0, 115.0};
    terminal::TemplateInputs inputs;
    inputs.slot_strikes = slot_strikes;
    inputs.basis = terminal::PriceBasis::Natural;
    const terminal::TemplateBuild build = terminal::buildTemplate(*recipe, chain, inputs);
    REQUIRE(build.error.empty());
    REQUIRE(build.legs.size() == 4);

    CHECK(build.legs[0].instrument == terminal::LegInstrument::Put);
    CHECK(build.legs[0].strike == 85.0);
    CHECK(build.legs[0].quantity == 1.0);
    CHECK(build.legs[0].price == Catch::Approx(2.05));
    CHECK(build.legs[1].strike == 95.0);
    CHECK(build.legs[1].quantity == -1.0);
    CHECK(build.legs[1].price == Catch::Approx(1.95));
    CHECK(build.legs[2].instrument == terminal::LegInstrument::Call);
    CHECK(build.legs[2].strike == 105.0);
    CHECK(build.legs[2].quantity == -1.0);
    CHECK(build.legs[3].strike == 115.0);
    CHECK(build.legs[3].quantity == 1.0);

    // Every contract here carries the same 2.00 of time value, so the wings cost
    // what the shorts collect and only the 0.05 half spread on four legs is paid.
    CHECK(terminal::netCost(build.legs) == Catch::Approx(20.0));
}

TEST_CASE("the default slots clamp to the ends of the strike list")
{
    const terminal::StrategyTemplate* recipe = terminal::findStrategyTemplate("iron_condor");
    REQUIRE(recipe != nullptr);
    const std::vector<std::size_t> low = terminal::defaultSlotStrikes(*recipe, 9, 0);
    CHECK(low == std::vector<std::size_t>{0, 0, 1, 3});
    const std::vector<std::size_t> high = terminal::defaultSlotStrikes(*recipe, 9, 50);
    CHECK(high == std::vector<std::size_t>{5, 7, 8, 8});
    CHECK(terminal::defaultSlotStrikes(*recipe, 0, 0).empty());
}

TEST_CASE("template builds explain what is wrong")
{
    const std::vector<terminal::OptionQuote> chain = strategyChain();
    const terminal::StrategyTemplate* spread = terminal::findStrategyTemplate("bull_call_spread");
    const terminal::StrategyTemplate* covered = terminal::findStrategyTemplate("covered_call");
    REQUIRE(spread != nullptr);
    REQUIRE(covered != nullptr);

    terminal::TemplateInputs inputs;
    const std::vector<double> one{100.0};
    inputs.slot_strikes = one;
    CHECK(terminal::buildTemplate(*spread, chain, inputs).error == "choose a strike for every slot");

    const std::vector<double> reversed{105.0, 100.0};
    inputs.slot_strikes = reversed;
    CHECK(terminal::buildTemplate(*spread, chain, inputs).error == "Short must be above Long");

    const std::vector<double> unlisted{100.0, 107.0};
    inputs.slot_strikes = unlisted;
    const terminal::TemplateBuild missing = terminal::buildTemplate(*spread, chain, inputs);
    CHECK(missing.error == "107.00 call is not listed");
    CHECK(missing.legs.empty());

    const std::vector<double> fine{100.0, 110.0};
    inputs.slot_strikes = fine;
    inputs.quantity = 0.0;
    CHECK(terminal::buildTemplate(*spread, chain, inputs).error == "quantity must be positive");

    inputs.quantity = 1.0;
    inputs.slot_strikes = one;
    inputs.underlying_price = 0.0;
    CHECK(terminal::buildTemplate(*covered, chain, inputs).error == "enter a spot price for the share leg");
    inputs.underlying_price = 100.0;
    const terminal::TemplateBuild shares = terminal::buildTemplate(*covered, chain, inputs);
    REQUIRE(shares.error.empty());
    CHECK(shares.legs[0].quantity == 100.0);
    CHECK(shares.legs[0].price == 100.0);
}

TEST_CASE("templates compose into one strategy")
{
    const std::vector<terminal::OptionQuote> chain = strategyChain();
    const terminal::StrategyTemplate* straddle = terminal::findStrategyTemplate("long_straddle");
    const terminal::StrategyTemplate* condor = terminal::findStrategyTemplate("iron_condor");
    REQUIRE(straddle != nullptr);
    REQUIRE(condor != nullptr);

    terminal::TemplateInputs inputs;
    const std::vector<double> body{100.0};
    inputs.slot_strikes = body;
    const terminal::TemplateBuild first = terminal::buildTemplate(*straddle, chain, inputs);
    const std::vector<double> wings{85.0, 95.0, 105.0, 115.0};
    inputs.slot_strikes = wings;
    const terminal::TemplateBuild second = terminal::buildTemplate(*condor, chain, inputs);
    REQUIRE(first.error.empty());
    REQUIRE(second.error.empty());

    std::vector<terminal::PayoffLeg> strategy;
    REQUIRE_FALSE(terminal::appendLegs(strategy, first.legs).has_value());
    REQUIRE_FALSE(terminal::appendLegs(strategy, second.legs).has_value());
    CHECK(strategy.size() == 6);
    for (double spot = 60.0; spot <= 140.0; spot += 1.0)
    {
        CHECK(terminal::strategyPayoff(strategy, spot) ==
              Catch::Approx(terminal::strategyPayoff(first.legs, spot) + terminal::strategyPayoff(second.legs, spot)));
    }
}
