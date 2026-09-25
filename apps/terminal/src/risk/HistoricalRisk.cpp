// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "risk/HistoricalRisk.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace terminal {
namespace {

[[nodiscard]] bool confidenceInRange(double confidence) noexcept
{
    return std::isfinite(confidence) && confidence > 0.0 && confidence < 1.0;
}

[[nodiscard]] std::optional<ValueAtRisk> measureSortedLosses(std::span<const double> losses_ascending, double confidence)
{
    const auto observations = losses_ascending.size();
    if (observations == 0 || !confidenceInRange(confidence))
    {
        return std::nullopt;
    }

    const auto count = static_cast<double>(observations);
    const double tail = 1.0 - confidence;
    bool have_var = false;
    double var = 0.0;
    double integral = 0.0;
    for (std::size_t rank = 1; rank <= observations; ++rank)
    {
        const auto left = static_cast<double>(rank - 1) / count;
        const auto right = static_cast<double>(rank) / count;
        const double loss = losses_ascending[rank - 1];
        // rank/count is the empirical CDF at this order statistic.
        if (!have_var && right >= confidence)
        {
            var = loss;
            have_var = true;
        }
        const double from = left > confidence ? left : confidence;
        if (right > from)
        {
            integral += (right - from) * loss;
        }
    }
    if (!have_var)
    {
        return std::nullopt;
    }

    ValueAtRisk result;
    result.var = var;
    result.cvar = integral / tail;
    result.observations = observations;
    if (!std::isfinite(result.var) || !std::isfinite(result.cvar))
    {
        return std::nullopt;
    }
    return result;
}

}  // namespace

std::optional<ValueAtRisk> valueAtRisk(std::span<const double> profit_and_loss, double confidence)
{
    if (profit_and_loss.empty() || !confidenceInRange(confidence))
    {
        return std::nullopt;
    }

    std::vector<double> losses(profit_and_loss.size());
    for (std::size_t index = 0; index < profit_and_loss.size(); ++index)
    {
        const double pnl = profit_and_loss[index];
        if (!std::isfinite(pnl))
        {
            return std::nullopt;
        }
        losses[index] = -pnl;
    }
    std::ranges::sort(losses);
    return measureSortedLosses(losses, confidence);
}

std::vector<double> simpleReturns(std::span<const double> levels)
{
    std::vector<double> returns;
    if (levels.size() < 2)
    {
        return returns;
    }
    returns.reserve(levels.size() - 1);
    for (std::size_t index = 1; index < levels.size(); ++index)
    {
        const double previous = levels[index - 1];
        const double current = levels[index];
        if (!std::isfinite(previous) || !std::isfinite(current) || previous <= 0.0)
        {
            continue;
        }
        returns.push_back((current - previous) / previous);
    }
    return returns;
}

std::optional<ValueAtRisk> positionValueAtRisk(std::span<const double> closes,
                                               double signed_exposure,
                                               ValueAtRiskSpec spec)
{
    if (!std::isfinite(signed_exposure) || !std::isfinite(spec.confidence))
    {
        return std::nullopt;
    }
    // A flat position cannot gain or lose, whatever the path does.
    if (signed_exposure == 0.0)
    {
        return ValueAtRisk{};
    }
    if (spec.lookback == 0)
    {
        return std::nullopt;
    }

    const std::vector<double> returns = simpleReturns(closes);
    std::span<const double> window{returns};
    if (window.size() > spec.lookback)
    {
        window = window.last(spec.lookback);
    }
    if (window.size() < spec.minimum_returns)
    {
        return std::nullopt;
    }

    std::vector<double> pnl(window.size());
    for (std::size_t index = 0; index < window.size(); ++index)
    {
        pnl[index] = signed_exposure * window[index];
    }
    return valueAtRisk(pnl, spec.confidence);
}

HoldingValueAtRisk holdingValueAtRisk(std::span<const double> closes,
                                      double quantity,
                                      double unit_exposure,
                                      ValueAtRiskSpec spec)
{
    HoldingValueAtRisk out;
    if (!std::isfinite(quantity) || !std::isfinite(unit_exposure))
    {
        return out;
    }
    // A short's unit is the opposite of one long unit. A flat line shows the long unit.
    const double unit_signed = quantity < 0.0 ? -unit_exposure : unit_exposure;
    out.position = positionValueAtRisk(closes, quantity * unit_exposure, spec);
    out.per_unit = positionValueAtRisk(closes, unit_signed, spec);
    return out;
}

PortfolioValueAtRisk portfolioValueAtRisk(std::span<const PortfolioLeg> legs, ValueAtRiskSpec spec)
{
    PortfolioValueAtRisk result;
    result.component_var.assign(legs.size(), std::nullopt);
    double sum = 0.0;
    bool any = false;
    for (std::size_t index = 0; index < legs.size(); ++index)
    {
        const std::optional<ValueAtRisk> risk =
            positionValueAtRisk(legs[index].closes, legs[index].signed_exposure, spec);
        if (!risk.has_value())
        {
            continue;
        }
        const ValueAtRisk measured = risk.value();
        result.component_var[index] = measured.var;
        sum += measured.var;
        any = true;
    }
    if (any)
    {
        result.var = sum;
    }
    return result;
}

}  // namespace terminal
