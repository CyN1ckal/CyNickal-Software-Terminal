// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace terminal {

// Loss at one confidence level. Positive means a loss, so a negative value is a
// quantile that is still a gain. cvar is at least var.
//
// var is the lower empirical quantile of loss: the smallest order statistic whose
// share of the sample is at least `confidence`. cvar is the average of that
// quantile function from `confidence` to 1 (expected shortfall). Each sample has
// equal weight. The input order does not matter.
struct ValueAtRisk
{
    double var{0.0};
    double cvar{0.0};
    std::size_t observations{0};
};

// One-period profit and loss. Positive is a gain. `confidence` is in (0, 1);
// 0.95 is a 95% one-period historical VaR. An empty sample, a non-finite
// sample, or a confidence outside (0, 1) returns nullopt.
[[nodiscard]] std::optional<ValueAtRisk> valueAtRisk(std::span<const double> profit_and_loss, double confidence);

// (level[i] - level[i - 1]) / level[i - 1], in order. A step is skipped when
// either level is non-finite or the earlier level is not positive, and the
// next step does not bridge across that hole.
[[nodiscard]] std::vector<double> simpleReturns(std::span<const double> levels);

// Dollar P&L of one option contract's underlying move: quantity * multiplier * delta * underlying.
// A long put (negative delta) loses when the underlying rises.
[[nodiscard]] constexpr double optionDeltaExposure(double quantity,
                                                   double contract_multiplier,
                                                   double delta,
                                                   double underlying_price) noexcept
{
    return quantity * contract_multiplier * delta * underlying_price;
}

// How positionValueAtRisk turns a price path into a one-period historical VaR.
// lookback keeps the most recent simple returns. Fewer than minimum_returns
// returns nullopt, except a zero exposure, which is zero risk with no sample.
struct ValueAtRiskSpec
{
    double confidence{0.95};
    std::size_t lookback{252};
    std::size_t minimum_returns{20};
};

// `closes` are in time order. `signed_exposure` is the profit of a +100% simple
// return and is negative for a short. The result is in the same unit as the exposure.
[[nodiscard]] std::optional<ValueAtRisk> positionValueAtRisk(std::span<const double> closes,
                                                             double signed_exposure,
                                                             ValueAtRiskSpec spec = {});

// `position` is `quantity * unit_exposure`. `per_unit` is one unit held in that
// same direction, or one long unit when quantity is zero. A non-finite quantity
// or unit exposure leaves both empty.
struct HoldingValueAtRisk
{
    std::optional<ValueAtRisk> position;
    std::optional<ValueAtRisk> per_unit;
};

[[nodiscard]] HoldingValueAtRisk holdingValueAtRisk(std::span<const double> closes,
                                                    double quantity,
                                                    double unit_exposure,
                                                    ValueAtRiskSpec spec = {});

// One line, valued on its own closes. Each line keeps its own latest window.
struct PortfolioLeg
{
    std::span<const double> closes;
    double signed_exposure{0.0};
};

// `var` is the sum of the leg VaRs that can be measured. `component_var` is
// parallel to the input and empty when that leg has no VaR. A short history on
// one line does not drop the others.
struct PortfolioValueAtRisk
{
    std::optional<double> var;
    std::vector<std::optional<double>> component_var;
};

[[nodiscard]] PortfolioValueAtRisk portfolioValueAtRisk(std::span<const PortfolioLeg> legs, ValueAtRiskSpec spec = {});

}  // namespace terminal
