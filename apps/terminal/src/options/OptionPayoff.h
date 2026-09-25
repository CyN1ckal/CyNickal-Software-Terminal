// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace terminal {

// Listed US equity options deliver 100 shares.
inline constexpr double kContractMultiplier = 100.0;

enum class LegInstrument : std::uint8_t
{
    Call,
    Put,
    Underlying,
};

// One position in a strategy. Any mix of legs on one underlying composes by
// concatenation: the payoff of a strategy is the sum of its legs' payoffs.
//
// quantity is signed: positive is long, negative is short. It counts contracts
// for an option and shares for the underlying. price is what one unit of the
// underlying cost or paid on entry: the option premium per share, or the share
// price. It is never negative. multiplier is shares per contract, 1 for the
// underlying. expiration is YYYYMMDD. It is 0 for the underlying, and for an
// option entered by hand without a date.
struct PayoffLeg
{
    LegInstrument instrument{LegInstrument::Call};
    double strike{0.0};
    std::int32_t expiration{0};
    double quantity{0.0};
    double price{0.0};
    double multiplier{kContractMultiplier};
};

[[nodiscard]] constexpr bool isOption(const PayoffLeg& leg) noexcept
{
    return leg.instrument != LegInstrument::Underlying;
}

// Value of one unit of the leg's instrument when the underlying settles at spot.
[[nodiscard]] constexpr double settlementValue(const PayoffLeg& leg, double spot) noexcept
{
    switch (leg.instrument)
    {
    case LegInstrument::Call:
        return std::max(spot - leg.strike, 0.0);
    case LegInstrument::Put:
        return std::max(leg.strike - spot, 0.0);
    case LegInstrument::Underlying:
        return spot;
    }
    return 0.0;
}

// Dollar profit of one leg held to expiration. Positive is a gain.
[[nodiscard]] constexpr double legPayoff(const PayoffLeg& leg, double spot) noexcept
{
    return leg.quantity * leg.multiplier * (settlementValue(leg, spot) - leg.price);
}

// Dollar profit of every leg held to expiration.
[[nodiscard]] constexpr double strategyPayoff(std::span<const PayoffLeg> legs, double spot) noexcept
{
    double total = 0.0;
    for (const PayoffLeg& leg : legs)
    {
        total += legPayoff(leg, spot);
    }
    return total;
}

// Dollars paid to open the legs. Negative is a net credit.
[[nodiscard]] constexpr double netCost(std::span<const PayoffLeg> legs) noexcept
{
    double total = 0.0;
    for (const PayoffLeg& leg : legs)
    {
        total += leg.quantity * leg.multiplier * leg.price;
    }
    return total;
}

// Empty when the legs can be priced at one expiration. Otherwise the reason,
// worded for the status line. An undated option (expiration 0) only mixes with
// other undated options. Options on different dates are rejected because
// a later leg still has time value when the first one settles, and that needs
// a pricing model, not an intrinsic payoff.
[[nodiscard]] std::optional<std::string> validateLegs(std::span<const PayoffLeg> legs);

// Appends incoming to strategy when the result still validates. On failure the
// strategy is unchanged and the reason is returned.
[[nodiscard]] std::optional<std::string> appendLegs(std::vector<PayoffLeg>& strategy,
                                                    std::span<const PayoffLeg> incoming);

// The expiration every option leg shares, or empty when there is no option leg.
[[nodiscard]] std::optional<std::int32_t> strategyExpiration(std::span<const PayoffLeg> legs) noexcept;

// Exact shape of the payoff over spot >= 0. The payoff is piecewise linear with
// kinks only at strikes, so the extremes sit at 0, at a strike, or at infinity.
//
// max_profit is the highest profit and max_loss the lowest (a loss is negative).
// Either is empty when the payoff runs to infinity in that direction as spot rises.
// breakevens are ascending spots where the payoff crosses or touches zero.
// A segment that lies on zero reports only its ends.
struct PayoffSummary
{
    double net_cost{0.0};
    std::optional<double> max_profit;
    std::optional<double> max_loss;
    std::vector<double> breakevens;
    // Dollars per $1 of spot above the highest strike.
    double upside_slope{0.0};
};

[[nodiscard]] PayoffSummary summarizePayoff(std::span<const PayoffLeg> legs);

// Ascending strikes of the option legs, without repeats.
[[nodiscard]] std::vector<double> legStrikes(std::span<const PayoffLeg> legs);

// A polyline that draws the payoff exactly on [low, high]: both ends, every
// strike inside, and every zero crossing inside, so a fill split at zero is exact.
struct PayoffPath
{
    std::vector<double> spots;
    std::vector<double> profits;
};

// low is clamped to 0. An empty path when high <= low or either end is not finite.
[[nodiscard]] PayoffPath payoffPath(std::span<const PayoffLeg> legs, double low, double high);

struct SpotRange
{
    double low{0.0};
    double high{0.0};
};

// A window that frames every strike, every breakeven, and reference_spot, with
// margin on both sides. reference_spot <= 0 or non-finite is ignored.
[[nodiscard]] SpotRange defaultSpotRange(std::span<const PayoffLeg> legs, double reference_spot);

}  // namespace terminal
