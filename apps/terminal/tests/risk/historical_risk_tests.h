// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "risk/HistoricalRisk.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

std::vector<double> lossesAsPnl(std::size_t count)
{
    std::vector<double> pnl(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        pnl[index] = -static_cast<double>(index + 1);
    }
    return pnl;
}

}  // namespace

TEST_CASE("value at risk defaults are a one-year 95 percent window")
{
    constexpr terminal::ValueAtRiskSpec spec;
    CHECK(spec.confidence == Catch::Approx(0.95));
    CHECK(spec.lookback == 252);
    CHECK(spec.minimum_returns == 20);
}

TEST_CASE("value at risk rejects an empty sample and a confidence outside (0, 1)")
{
    const std::vector<double> pnl{-1.0, 2.0};
    CHECK_FALSE(terminal::valueAtRisk({}, 0.95).has_value());
    CHECK_FALSE(terminal::valueAtRisk(pnl, 0.0).has_value());
    CHECK_FALSE(terminal::valueAtRisk(pnl, 1.0).has_value());
    CHECK_FALSE(terminal::valueAtRisk(pnl, -0.1).has_value());
    CHECK_FALSE(terminal::valueAtRisk(pnl, 1.1).has_value());
    CHECK_FALSE(terminal::valueAtRisk(pnl, std::numeric_limits<double>::quiet_NaN()).has_value());
    CHECK_FALSE(terminal::valueAtRisk(pnl, std::numeric_limits<double>::infinity()).has_value());
}

TEST_CASE("value at risk rejects a non-finite profit")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<double> with_nan{1.0, nan, -2.0};
    const std::vector<double> with_inf{1.0, std::numeric_limits<double>::infinity()};
    CHECK_FALSE(terminal::valueAtRisk(with_nan, 0.95).has_value());
    CHECK_FALSE(terminal::valueAtRisk(with_inf, 0.95).has_value());
}

TEST_CASE("one observation is both the VaR and the expected shortfall")
{
    const std::vector<double> pnl{-7.0};
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(pnl, 0.95);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == 7.0);
    CHECK(measured.cvar == 7.0);
    CHECK(measured.observations == 1);
}

TEST_CASE("historical VaR is the lower empirical loss quantile")
{
    // Losses sorted: -5, -2, 0, 1, 4. At 60% the third order statistic is 0.
    // The tail integral weights 1 by 0.2 and 4 by 0.2, so shortfall is 2.5.
    const std::vector<double> pnl{5.0, -1.0, -4.0, 2.0, 0.0};
    const std::vector<double> shuffled{-4.0, 5.0, 0.0, -1.0, 2.0};
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(pnl, 0.6);
    const std::optional<terminal::ValueAtRisk> again = terminal::valueAtRisk(shuffled, 0.6);
    REQUIRE(risk.has_value());
    REQUIRE(again.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    const terminal::ValueAtRisk reordered = again.value();
    CHECK(measured.var == 0.0);
    CHECK(measured.cvar == Catch::Approx(2.5));
    CHECK(measured.observations == 5);
    CHECK(reordered.var == measured.var);
    CHECK(reordered.cvar == Catch::Approx(measured.cvar));
    CHECK(pnl == std::vector<double>{5.0, -1.0, -4.0, 2.0, 0.0});
}

TEST_CASE("a low confidence reads the favorable part of the same sample")
{
    const std::vector<double> pnl{5.0, -1.0, -4.0, 2.0, 0.0};
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(pnl, 0.2);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == -5.0);
    CHECK(measured.cvar == Catch::Approx(0.75));
}

TEST_CASE("expected shortfall averages the losses beyond an integer tail")
{
    const std::vector<double> pnl = lossesAsPnl(100);
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(pnl, 0.95);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == 95.0);
    CHECK(measured.cvar == Catch::Approx(98.0));
    CHECK(measured.observations == 100);
    CHECK(measured.cvar >= measured.var);
}

TEST_CASE("twenty losses put 95 percent VaR on the second-worst outcome")
{
    const std::vector<double> pnl = lossesAsPnl(20);
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(pnl, 0.95);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == 19.0);
    CHECK(measured.cvar == Catch::Approx(20.0));
}

TEST_CASE("a confidence that reaches only the worst loss sets VaR equal to CVaR")
{
    const std::vector<double> pnl = lossesAsPnl(10);
    const std::optional<terminal::ValueAtRisk> at_99 = terminal::valueAtRisk(pnl, 0.99);
    const std::optional<terminal::ValueAtRisk> at_90 = terminal::valueAtRisk(pnl, 0.90);
    REQUIRE(at_99.has_value());
    REQUIRE(at_90.has_value());
    const terminal::ValueAtRisk tight = at_99.value();
    const terminal::ValueAtRisk wide = at_90.value();
    CHECK(tight.var == 10.0);
    CHECK(tight.cvar == Catch::Approx(10.0));
    CHECK(wide.var == 9.0);
    CHECK(wide.cvar == Catch::Approx(10.0));
}

TEST_CASE("a sample of gains has a negative loss quantile")
{
    const std::vector<double> pnl{1.0, 2.0, 3.0, 4.0};
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(pnl, 0.5);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == -3.0);
    CHECK(measured.cvar == Catch::Approx(-1.5));
    CHECK(measured.cvar >= measured.var);
}

TEST_CASE("equal profits are a flat VaR and shortfall")
{
    const std::vector<double> pnl{3.0, 3.0, 3.0, 3.0};
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(pnl, 0.95);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == -3.0);
    CHECK(measured.cvar == Catch::Approx(-3.0));
}

TEST_CASE("scaling profit scales both risk numbers")
{
    const std::vector<double> pnl{5.0, -1.0, -4.0, 2.0, 0.0};
    std::vector<double> scaled(pnl.size());
    for (std::size_t index = 0; index < pnl.size(); ++index)
    {
        scaled[index] = pnl[index] * 3.0;
    }
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(scaled, 0.6);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == 0.0);
    CHECK(measured.cvar == Catch::Approx(7.5));
}

TEST_CASE("negating profit reads the other tail")
{
    const std::vector<double> pnl{-5.0, 1.0, 4.0, -2.0, 0.0};
    const std::optional<terminal::ValueAtRisk> risk = terminal::valueAtRisk(pnl, 0.6);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == 0.0);
    CHECK(measured.cvar == Catch::Approx(3.5));
}

TEST_CASE("a higher confidence is a worse loss quantile")
{
    const std::vector<double> pnl{0.2, -1.5, 0.3, -0.7,  1.1, -2.4, 0.05, -0.2, 0.8, -3.0, 0.4, -0.1, 1.4,
                                  -1.1, 0.6,  -0.4, 0.9, -2.0, 0.15, -0.8, 0.3,  -1.7, 0.7, -0.05, 1.2};
    const std::optional<terminal::ValueAtRisk> at_90 = terminal::valueAtRisk(pnl, 0.90);
    const std::optional<terminal::ValueAtRisk> at_95 = terminal::valueAtRisk(pnl, 0.95);
    const std::optional<terminal::ValueAtRisk> at_99 = terminal::valueAtRisk(pnl, 0.99);
    REQUIRE(at_90.has_value());
    REQUIRE(at_95.has_value());
    REQUIRE(at_99.has_value());
    const terminal::ValueAtRisk low = at_90.value();
    const terminal::ValueAtRisk mid = at_95.value();
    const terminal::ValueAtRisk high = at_99.value();
    CHECK(mid.var >= low.var);
    CHECK(high.var >= mid.var);
    CHECK(mid.cvar >= low.cvar);
    CHECK(high.cvar >= mid.cvar);
    CHECK(low.cvar >= low.var);
    CHECK(mid.cvar >= mid.var);
    CHECK(high.cvar >= high.var);
    CHECK(mid.observations == pnl.size());
}

TEST_CASE("simple returns skip a non-positive or non-finite level without bridging it")
{
    const std::vector<double> none;
    const std::vector<double> one{100.0};
    CHECK(terminal::simpleReturns(none).empty());
    CHECK(terminal::simpleReturns(one).empty());

    const std::vector<double> rising{100.0, 110.0, 121.0};
    const std::vector<double> steady = terminal::simpleReturns(rising);
    REQUIRE(steady.size() == 2);
    CHECK(steady[0] == Catch::Approx(0.1));
    CHECK(steady[1] == Catch::Approx(0.1));

    const std::vector<double> to_zero{100.0, 0.0, 50.0};
    const std::vector<double> through_zero = terminal::simpleReturns(to_zero);
    REQUIRE(through_zero.size() == 1);
    CHECK(through_zero[0] == Catch::Approx(-1.0));

    const std::vector<double> to_negative{100.0, -5.0, 110.0};
    const std::vector<double> through_negative = terminal::simpleReturns(to_negative);
    REQUIRE(through_negative.size() == 1);
    CHECK(through_negative[0] == Catch::Approx(-1.05));

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<double> with_nan{100.0, nan, 110.0};
    CHECK(terminal::simpleReturns(with_nan).empty());
    const std::vector<double> unchanged{10.0, 10.0};
    const std::vector<double> flat = terminal::simpleReturns(unchanged);
    REQUIRE(flat.size() == 1);
    CHECK(flat[0] == 0.0);
}

TEST_CASE("option delta exposure keeps the sign of a put")
{
    CHECK(terminal::optionDeltaExposure(1.0, 100.0, 0.5, 100.0) == Catch::Approx(5000.0));
    CHECK(terminal::optionDeltaExposure(1.0, 100.0, -0.4, 100.0) == Catch::Approx(-4000.0));
    CHECK(terminal::optionDeltaExposure(-1.0, 100.0, -0.4, 100.0) == Catch::Approx(4000.0));
    CHECK(terminal::optionDeltaExposure(2.0, 100.0, 0.0, 40.0) == 0.0);
}

TEST_CASE("position VaR matches the profit-and-loss measure on the same path")
{
    const std::vector<double> closes{80.0, 100.0, 80.0, 100.0, 120.0};
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.75, .lookback = 10, .minimum_returns = 4};
    const std::optional<terminal::ValueAtRisk> from_path = terminal::positionValueAtRisk(closes, 1000.0, spec);
    std::vector<double> pnl;
    for (const double ret : terminal::simpleReturns(closes))
    {
        pnl.push_back(1000.0 * ret);
    }
    const std::optional<terminal::ValueAtRisk> from_pnl = terminal::valueAtRisk(pnl, spec.confidence);
    REQUIRE(from_path.has_value());
    REQUIRE(from_pnl.has_value());
    const terminal::ValueAtRisk path = from_path.value();
    const terminal::ValueAtRisk direct = from_pnl.value();
    CHECK(path.var == Catch::Approx(direct.var));
    CHECK(path.cvar == Catch::Approx(direct.cvar));
    // Three of the four days are gains, so the 75% loss quantile is still a gain.
    // Expected shortfall is the one down day.
    CHECK(path.var == Catch::Approx(-200.0));
    CHECK(path.cvar == Catch::Approx(200.0));
    CHECK(path.cvar >= path.var);
    CHECK(path.observations == 4);
}

TEST_CASE("a short uses the up-move tail rather than the negated long VaR")
{
    const std::vector<double> closes{80.0, 100.0, 80.0, 100.0, 120.0};
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.75, .lookback = 10, .minimum_returns = 4};
    const std::optional<terminal::ValueAtRisk> risk = terminal::positionValueAtRisk(closes, -1000.0, spec);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == Catch::Approx(250.0));
    CHECK(measured.cvar == Catch::Approx(250.0));
}

TEST_CASE("the lookback keeps the recent move and drops the older one")
{
    std::vector<double> closes{100.0, 50.0};
    closes.insert(closes.end(), 10, 50.0);
    closes.push_back(40.0);
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.95, .lookback = 5, .minimum_returns = 5};
    const std::optional<terminal::ValueAtRisk> risk = terminal::positionValueAtRisk(closes, 1000.0, spec);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == Catch::Approx(200.0));
    CHECK(measured.cvar == Catch::Approx(200.0));
    CHECK(measured.observations == 5);
}

TEST_CASE("a flat tail inside the lookback is zero risk")
{
    std::vector<double> closes{100.0, 50.0};
    closes.insert(closes.end(), 6, 50.0);
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.95, .lookback = 5, .minimum_returns = 5};
    const std::optional<terminal::ValueAtRisk> risk = terminal::positionValueAtRisk(closes, 200.0, spec);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    CHECK(measured.var == 0.0);
    CHECK(measured.cvar == 0.0);
}

TEST_CASE("the default window needs twenty returns")
{
    const std::vector<double> too_short(20, 100.0);
    CHECK_FALSE(terminal::positionValueAtRisk(too_short, 500.0).has_value());

    std::vector<double> enough(21, 100.0);
    enough.back() = 80.0;
    const std::optional<terminal::ValueAtRisk> risk = terminal::positionValueAtRisk(enough, 500.0);
    REQUIRE(risk.has_value());
    const terminal::ValueAtRisk measured = risk.value();
    // Nineteen unchanged days and one -20% day. 95% VaR is the second-worst day.
    CHECK(measured.observations == 20);
    CHECK(measured.var == 0.0);
    CHECK(measured.cvar == Catch::Approx(100.0));
}

TEST_CASE("a zero exposure is zero risk and does not read the path")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<double> broken{nan, -1.0};
    const std::optional<terminal::ValueAtRisk> flat = terminal::positionValueAtRisk(broken, 0.0);
    const std::optional<terminal::ValueAtRisk> empty = terminal::positionValueAtRisk({}, -0.0);
    REQUIRE(flat.has_value());
    REQUIRE(empty.has_value());
    CHECK(flat.value().var == 0.0);
    CHECK(flat.value().cvar == 0.0);
    CHECK(flat.value().observations == 0);
    CHECK(empty.value().var == 0.0);
}

TEST_CASE("holding risk splits the position from one unit in the same direction")
{
    const std::vector<double> closes{80.0, 100.0, 80.0, 100.0, 120.0};
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.75, .lookback = 10, .minimum_returns = 4};

    const terminal::HoldingValueAtRisk long_line = terminal::holdingValueAtRisk(closes, 2.0, 1000.0, spec);
    REQUIRE(long_line.position.has_value());
    REQUIRE(long_line.per_unit.has_value());
    const terminal::ValueAtRisk long_position = long_line.position.value();
    const terminal::ValueAtRisk long_unit = long_line.per_unit.value();
    CHECK(long_position.var == Catch::Approx(-400.0));
    CHECK(long_position.cvar == Catch::Approx(400.0));
    CHECK(long_unit.var == Catch::Approx(-200.0));
    CHECK(long_unit.cvar == Catch::Approx(200.0));
    CHECK(long_position.observations == 4);
    CHECK(long_unit.observations == 4);

    const terminal::HoldingValueAtRisk short_line = terminal::holdingValueAtRisk(closes, -4.0, 1000.0, spec);
    REQUIRE(short_line.position.has_value());
    REQUIRE(short_line.per_unit.has_value());
    const terminal::ValueAtRisk short_position = short_line.position.value();
    const terminal::ValueAtRisk short_unit = short_line.per_unit.value();
    CHECK(short_position.var == Catch::Approx(1000.0));
    CHECK(short_position.cvar == Catch::Approx(1000.0));
    CHECK(short_unit.var == Catch::Approx(250.0));
    CHECK(short_unit.cvar == Catch::Approx(250.0));

    // A short put's unit exposure is already negative. One short contract flips it positive.
    const terminal::HoldingValueAtRisk short_put = terminal::holdingValueAtRisk(closes, -1.0, -4000.0, spec);
    REQUIRE(short_put.position.has_value());
    REQUIRE(short_put.per_unit.has_value());
    CHECK(short_put.position.value().var == Catch::Approx(-800.0));
    CHECK(short_put.per_unit.value().var == Catch::Approx(-800.0));
    CHECK(short_put.position.value().cvar == Catch::Approx(800.0));
}

TEST_CASE("a flat quantity keeps a per-unit figure and a zero position")
{
    const std::vector<double> closes{80.0, 100.0, 80.0, 100.0, 120.0};
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.75, .lookback = 10, .minimum_returns = 4};
    const terminal::HoldingValueAtRisk flat = terminal::holdingValueAtRisk(closes, 0.0, 1000.0, spec);
    REQUIRE(flat.position.has_value());
    REQUIRE(flat.per_unit.has_value());
    CHECK(flat.position.value().var == 0.0);
    CHECK(flat.position.value().cvar == 0.0);
    CHECK(flat.position.value().observations == 0);
    CHECK(flat.per_unit.value().var == Catch::Approx(-200.0));
    CHECK(flat.per_unit.value().cvar == Catch::Approx(200.0));

    const std::vector<double> too_short{100.0, 110.0};
    constexpr terminal::ValueAtRiskSpec needs_history{.confidence = 0.95, .lookback = 252, .minimum_returns = 20};
    const terminal::HoldingValueAtRisk unknown = terminal::holdingValueAtRisk(too_short, 0.0, 50.0, needs_history);
    REQUIRE(unknown.position.has_value());
    CHECK(unknown.position.value().var == 0.0);
    CHECK_FALSE(unknown.per_unit.has_value());
}

TEST_CASE("holding risk follows the requested confidence")
{
    const std::vector<double> closes{80.0, 100.0, 80.0, 100.0, 120.0};
    constexpr terminal::ValueAtRiskSpec wide{.confidence = 0.75, .lookback = 10, .minimum_returns = 4};
    constexpr terminal::ValueAtRiskSpec tight{.confidence = 0.95, .lookback = 10, .minimum_returns = 4};
    const terminal::HoldingValueAtRisk at_75 = terminal::holdingValueAtRisk(closes, 1.0, 1000.0, wide);
    const terminal::HoldingValueAtRisk at_95 = terminal::holdingValueAtRisk(closes, 1.0, 1000.0, tight);
    REQUIRE(at_75.position.has_value());
    REQUIRE(at_95.position.has_value());
    CHECK(at_75.position.value().var == Catch::Approx(-200.0));
    CHECK(at_95.position.value().var == Catch::Approx(200.0));
    CHECK(at_95.per_unit.value().var == Catch::Approx(at_95.position.value().var));
}

TEST_CASE("holding risk rejects a non-finite quantity or unit exposure")
{
    const std::vector<double> closes{80.0, 100.0, 80.0, 100.0, 120.0};
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.75, .lookback = 10, .minimum_returns = 4};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const terminal::HoldingValueAtRisk bad_quantity = terminal::holdingValueAtRisk(closes, nan, 1000.0, spec);
    const terminal::HoldingValueAtRisk bad_unit = terminal::holdingValueAtRisk(closes, 2.0, nan, spec);
    const terminal::HoldingValueAtRisk cash = terminal::holdingValueAtRisk(closes, 5.0, 0.0, spec);
    CHECK_FALSE(bad_quantity.position.has_value());
    CHECK_FALSE(bad_quantity.per_unit.has_value());
    CHECK_FALSE(bad_unit.position.has_value());
    CHECK_FALSE(bad_unit.per_unit.has_value());
    REQUIRE(cash.position.has_value());
    REQUIRE(cash.per_unit.has_value());
    CHECK(cash.position.value().var == 0.0);
    CHECK(cash.per_unit.value().var == 0.0);
}

namespace {

struct StoredLeg
{
    std::vector<double> closes;
    double exposure{0.0};

    [[nodiscard]] terminal::PortfolioLeg view() const
    {
        terminal::PortfolioLeg leg;
        leg.closes = closes;
        leg.signed_exposure = exposure;
        return leg;
    }
};

}  // namespace

TEST_CASE("portfolio VaR sums each line on its own latest closes")
{
    StoredLeg first{{80.0, 100.0, 80.0, 100.0, 120.0}, 1000.0};
    StoredLeg second{{100.0, 200.0, 100.0, 200.0}, 100.0};
    StoredLeg cash{{}, 0.0};
    const std::vector<terminal::PortfolioLeg> legs{first.view(), second.view(), cash.view()};
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.75, .lookback = 10, .minimum_returns = 3};
    const std::optional<terminal::ValueAtRisk> first_alone = terminal::positionValueAtRisk(first.closes, 1000.0, spec);
    const std::optional<terminal::ValueAtRisk> second_alone = terminal::positionValueAtRisk(second.closes, 100.0, spec);
    REQUIRE(first_alone.has_value());
    REQUIRE(second_alone.has_value());
    const terminal::PortfolioValueAtRisk book = terminal::portfolioValueAtRisk(legs, spec);
    REQUIRE(book.var.has_value());
    REQUIRE(book.component_var.size() == 3);
    REQUIRE(book.component_var[0].has_value());
    REQUIRE(book.component_var[1].has_value());
    REQUIRE(book.component_var[2].has_value());
    CHECK(book.component_var[0].value() == Catch::Approx(first_alone.value().var));
    CHECK(book.component_var[1].value() == 50.0);
    CHECK(book.component_var[2].value() == 0.0);
    CHECK(book.var.value() == Catch::Approx(first_alone.value().var + 50.0));
}

TEST_CASE("a short history on one line does not drop the other line")
{
    StoredLeg ready{{100.0, 50.0, 50.0, 50.0, 50.0}, 100.0};
    StoredLeg thin{{100.0, 80.0}, 100.0};
    const std::vector<terminal::PortfolioLeg> legs{ready.view(), thin.view()};
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.95, .lookback = 2, .minimum_returns = 2};
    const terminal::PortfolioValueAtRisk book = terminal::portfolioValueAtRisk(legs, spec);
    REQUIRE(book.var.has_value());
    REQUIRE(book.component_var[0].has_value());
    CHECK(book.component_var[0].value() == 0.0);
    CHECK_FALSE(book.component_var[1].has_value());
    CHECK(book.var.value() == 0.0);

    constexpr terminal::ValueAtRiskSpec allow_one{.confidence = 0.95, .lookback = 4, .minimum_returns = 1};
    const terminal::PortfolioValueAtRisk both = terminal::portfolioValueAtRisk(legs, allow_one);
    REQUIRE(both.var.has_value());
    REQUIRE(both.component_var[0].has_value());
    REQUIRE(both.component_var[1].has_value());
    CHECK(both.component_var[0].value() == 50.0);
    CHECK(both.component_var[1].value() == 20.0);
    CHECK(both.var.value() == 70.0);
}

TEST_CASE("a flat book has zero portfolio VaR")
{
    const terminal::PortfolioValueAtRisk empty = terminal::portfolioValueAtRisk({});
    CHECK_FALSE(empty.var.has_value());
    CHECK(empty.component_var.empty());

    StoredLeg cash{{10.0, 11.0, 12.0}, 0.0};
    const std::vector<terminal::PortfolioLeg> legs{cash.view()};
    const terminal::PortfolioValueAtRisk flat = terminal::portfolioValueAtRisk(legs);
    REQUIRE(flat.var.has_value());
    CHECK(flat.var.value() == 0.0);
    REQUIRE(flat.component_var.size() == 1);
    REQUIRE(flat.component_var[0].has_value());
    CHECK(flat.component_var[0].value() == 0.0);
}

TEST_CASE("portfolio VaR skips a leg with no measure")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    StoredLeg broken{{10.0, 11.0, 12.0, 13.0}, nan};
    StoredLeg ready{{80.0, 100.0, 80.0, 100.0, 120.0}, 1000.0};
    constexpr terminal::ValueAtRiskSpec spec{.confidence = 0.75, .lookback = 10, .minimum_returns = 4};
    const std::vector<terminal::PortfolioLeg> legs{broken.view(), ready.view()};
    const terminal::PortfolioValueAtRisk book = terminal::portfolioValueAtRisk(legs, spec);
    const std::optional<terminal::ValueAtRisk> alone = terminal::positionValueAtRisk(ready.closes, 1000.0, spec);
    REQUIRE(book.var.has_value());
    REQUIRE(alone.has_value());
    CHECK_FALSE(book.component_var[0].has_value());
    REQUIRE(book.component_var[1].has_value());
    CHECK(book.component_var[1].value() == Catch::Approx(alone.value().var));
    CHECK(book.var.value() == Catch::Approx(alone.value().var));
}

TEST_CASE("position VaR rejects a non-finite exposure, a short sample, and an empty window")
{
    const std::vector<double> closes{10.0, 11.0, 12.0, 13.0, 14.0};
    constexpr terminal::ValueAtRiskSpec needs_four{.confidence = 0.95, .lookback = 10, .minimum_returns = 4};
    constexpr terminal::ValueAtRiskSpec no_window{.confidence = 0.95, .lookback = 0, .minimum_returns = 1};
    constexpr terminal::ValueAtRiskSpec window_shorter_than_minimum{
        .confidence = 0.95, .lookback = 2, .minimum_returns = 4};
    CHECK_FALSE(terminal::positionValueAtRisk(closes, std::numeric_limits<double>::quiet_NaN(), needs_four).has_value());
    CHECK_FALSE(terminal::positionValueAtRisk({}, 25.0, needs_four).has_value());
    CHECK_FALSE(terminal::positionValueAtRisk(closes, 25.0, no_window).has_value());
    CHECK_FALSE(terminal::positionValueAtRisk(closes, 25.0, window_shorter_than_minimum).has_value());
    CHECK_FALSE(terminal::positionValueAtRisk(closes, 25.0, {.confidence = 1.0, .lookback = 10, .minimum_returns = 1})
                    .has_value());
}
