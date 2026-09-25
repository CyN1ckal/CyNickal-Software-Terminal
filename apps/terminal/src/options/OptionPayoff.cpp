// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "options/OptionPayoff.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace terminal {
namespace {

// Rounding noise in a sum of legs scales with their notional.
[[nodiscard]] double zeroTolerance(std::span<const PayoffLeg> legs) noexcept
{
    double notional = 0.0;
    for (const PayoffLeg& leg : legs)
    {
        notional += std::fabs(leg.quantity * leg.multiplier) * (1.0 + std::fabs(leg.strike) + std::fabs(leg.price));
    }
    return 1e-9 * (1.0 + notional);
}

[[nodiscard]] double slopeAboveStrikes(std::span<const PayoffLeg> legs) noexcept
{
    double slope = 0.0;
    for (const PayoffLeg& leg : legs)
    {
        if (leg.instrument != LegInstrument::Put)
        {
            slope += leg.quantity * leg.multiplier;
        }
    }
    return slope;
}

// Spot 0 and every positive strike, ascending.
[[nodiscard]] std::vector<double> payoffKnots(std::span<const PayoffLeg> legs)
{
    std::vector<double> knots{0.0};
    for (const double strike : legStrikes(legs))
    {
        if (strike > 0.0)
        {
            knots.push_back(strike);
        }
    }
    return knots;
}

[[nodiscard]] bool finiteLeg(const PayoffLeg& leg) noexcept
{
    return std::isfinite(leg.strike) && std::isfinite(leg.quantity) && std::isfinite(leg.price) &&
           std::isfinite(leg.multiplier);
}

}  // namespace

std::optional<std::string> validateLegs(std::span<const PayoffLeg> legs)
{
    if (legs.empty())
    {
        return std::string("add a leg");
    }
    std::optional<std::int32_t> expiration;
    for (const PayoffLeg& leg : legs)
    {
        if (!finiteLeg(leg))
        {
            return std::string("a leg has a value that is not a number");
        }
        if (leg.quantity == 0.0)
        {
            return std::string("a leg has zero quantity");
        }
        if (leg.price < 0.0)
        {
            return std::string("a leg has a negative price");
        }
        if (leg.multiplier <= 0.0)
        {
            return std::string("a leg has no multiplier");
        }
        if (!isOption(leg))
        {
            continue;
        }
        if (leg.strike <= 0.0)
        {
            return std::string("an option leg has no strike");
        }
        if (leg.expiration < 0)
        {
            return std::string("an option leg has an invalid expiration");
        }
        if (!expiration.has_value())
        {
            expiration = leg.expiration;
        }
        else if (*expiration != leg.expiration)
        {
            return std::string("legs expire on different dates; an expiration payoff needs one date");
        }
    }
    return std::nullopt;
}

std::optional<std::string> appendLegs(std::vector<PayoffLeg>& strategy, std::span<const PayoffLeg> incoming)
{
    if (incoming.empty())
    {
        return std::string("nothing to add");
    }
    std::vector<PayoffLeg> combined = strategy;
    combined.insert(combined.end(), incoming.begin(), incoming.end());
    if (std::optional<std::string> problem = validateLegs(combined); problem.has_value())
    {
        return problem;
    }
    strategy = std::move(combined);
    return std::nullopt;
}

std::optional<std::int32_t> strategyExpiration(std::span<const PayoffLeg> legs) noexcept
{
    for (const PayoffLeg& leg : legs)
    {
        if (isOption(leg))
        {
            return leg.expiration;
        }
    }
    return std::nullopt;
}

std::vector<double> legStrikes(std::span<const PayoffLeg> legs)
{
    std::vector<double> strikes;
    for (const PayoffLeg& leg : legs)
    {
        if (isOption(leg))
        {
            strikes.push_back(leg.strike);
        }
    }
    std::ranges::sort(strikes);
    const auto repeats = std::ranges::unique(strikes);
    strikes.erase(repeats.begin(), repeats.end());
    return strikes;
}

PayoffSummary summarizePayoff(std::span<const PayoffLeg> legs)
{
    PayoffSummary summary;
    summary.net_cost = netCost(legs);
    summary.upside_slope = slopeAboveStrikes(legs);

    const std::vector<double> knots = payoffKnots(legs);
    std::vector<double> values;
    values.reserve(knots.size());
    for (const double knot : knots)
    {
        values.push_back(strategyPayoff(legs, knot));
    }

    const double tolerance = zeroTolerance(legs);
    const double slope = std::fabs(summary.upside_slope) <= tolerance ? 0.0 : summary.upside_slope;
    const auto [lowest, highest] = std::ranges::minmax(values);
    if (slope <= 0.0)
    {
        summary.max_profit = highest;
    }
    if (slope >= 0.0)
    {
        summary.max_loss = lowest;
    }

    const auto is_zero = [tolerance](double value) { return std::fabs(value) <= tolerance; };
    for (std::size_t index = 0; index < knots.size(); ++index)
    {
        const double here = values[index];
        if (is_zero(here))
        {
            summary.breakevens.push_back(knots[index]);
            continue;
        }
        if (index + 1 < knots.size())
        {
            const double next = values[index + 1];
            if (!is_zero(next) && (here < 0.0) != (next < 0.0))
            {
                const double width = knots[index + 1] - knots[index];
                summary.breakevens.push_back(knots[index] + (width * here / (here - next)));
            }
        }
        else if (slope != 0.0 && (here < 0.0) != (slope < 0.0))
        {
            summary.breakevens.push_back(knots[index] - (here / slope));
        }
    }
    return summary;
}

PayoffPath payoffPath(std::span<const PayoffLeg> legs, double low, double high)
{
    PayoffPath path;
    if (!std::isfinite(low) || !std::isfinite(high))
    {
        return path;
    }
    low = std::max(low, 0.0);
    if (high <= low)
    {
        return path;
    }

    std::vector<double> corners{low};
    for (const double strike : legStrikes(legs))
    {
        if (strike > low && strike < high)
        {
            corners.push_back(strike);
        }
    }
    corners.push_back(high);

    path.spots.reserve(corners.size() * 2);
    path.profits.reserve(corners.size() * 2);
    double previous = 0.0;
    for (std::size_t index = 0; index < corners.size(); ++index)
    {
        const double spot = corners[index];
        const double profit = strategyPayoff(legs, spot);
        if (index > 0 && ((previous < 0.0 && profit > 0.0) || (previous > 0.0 && profit < 0.0)))
        {
            const double from = corners[index - 1];
            path.spots.push_back(from + ((spot - from) * previous / (previous - profit)));
            path.profits.push_back(0.0);
        }
        path.spots.push_back(spot);
        path.profits.push_back(profit);
        previous = profit;
    }
    return path;
}

SpotRange defaultSpotRange(std::span<const PayoffLeg> legs, double reference_spot)
{
    std::vector<double> anchors = legStrikes(legs);
    if (!legs.empty())
    {
        const PayoffSummary summary = summarizePayoff(legs);
        anchors.insert(anchors.end(), summary.breakevens.begin(), summary.breakevens.end());
    }
    if (std::isfinite(reference_spot) && reference_spot > 0.0)
    {
        anchors.push_back(reference_spot);
    }
    std::erase_if(anchors, [](double anchor) { return !std::isfinite(anchor) || anchor <= 0.0; });
    if (anchors.empty())
    {
        return SpotRange{.low = 0.0, .high = 1.0};
    }
    const auto [lowest, highest] = std::ranges::minmax(anchors);
    const double margin = std::max((highest - lowest) * 0.25, highest * 0.1);
    return SpotRange{.low = std::max(0.0, lowest - margin), .high = highest + margin};
}

}  // namespace terminal
