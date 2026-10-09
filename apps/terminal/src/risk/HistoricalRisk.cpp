// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "risk/HistoricalRisk.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <optional>
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

// A bad series blanks the whole book. Dates match closes and strictly increase.
// Every close is finite. Every close but the last is a previous close and must be positive.
[[nodiscard]] bool seriesMeasurable(std::span<const SessionDate> dates, std::span<const double> closes) noexcept
{
    if (dates.size() != closes.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < closes.size(); ++index)
    {
        const double close = closes[index];
        if (!std::isfinite(close))
        {
            return false;
        }
        if (index > 0 && dates[index] <= dates[index - 1])
        {
            return false;
        }
        const bool has_later = index + 1 < closes.size();
        if (has_later && !(close > 0.0))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<double> closeOnDate(std::span<const SessionDate> dates,
                                               std::span<const double> closes,
                                               SessionDate date) noexcept
{
    const auto found = std::ranges::lower_bound(dates, date);
    if (found == dates.end() || *found != date || closes.size() != dates.size())
    {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(std::ranges::distance(dates.begin(), found));
    return closes[index];
}

// Equal losses keep the earlier sample, so a tied day has one leg split.
[[nodiscard]] std::optional<std::size_t> lossQuantileIndex(std::span<const double> profit_and_loss, double confidence)
{
    const auto observations = profit_and_loss.size();
    if (observations == 0 || !confidenceInRange(confidence))
    {
        return std::nullopt;
    }
    std::vector<std::size_t> order(observations);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::stable_sort(order, [&profit_and_loss](std::size_t left, std::size_t right) {
        return profit_and_loss[left] > profit_and_loss[right];
    });
    const auto count = static_cast<double>(observations);
    for (std::size_t rank = 1; rank <= observations; ++rank)
    {
        const auto right = static_cast<double>(rank) / count;
        if (right >= confidence)
        {
            return order[rank - 1];
        }
    }
    return std::nullopt;
}

struct PricedLeg
{
    std::size_t index{0};
    double signed_exposure{0.0};
    std::span<const SessionDate> dates;
    std::span<const double> closes;
};

[[nodiscard]] std::optional<double> legProfit(const PricedLeg& leg, SessionDate earlier, SessionDate later)
{
    const std::optional<double> previous = closeOnDate(leg.dates, leg.closes, earlier);
    const std::optional<double> current = closeOnDate(leg.dates, leg.closes, later);
    if (!previous.has_value() || !current.has_value())
    {
        return std::nullopt;
    }
    const double earlier_close = previous.value();
    const double later_close = current.value();
    if (!std::isfinite(earlier_close) || !std::isfinite(later_close) || !(earlier_close > 0.0))
    {
        return std::nullopt;
    }
    const double ret = (later_close - earlier_close) / earlier_close;
    return leg.signed_exposure * ret;
}

[[nodiscard]] PortfolioValueAtRisk unmeasuredBook(std::size_t count)
{
    return PortfolioValueAtRisk{
        .var = std::nullopt,
        .component_var = std::vector<std::optional<double>>(count),
    };
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
    if (legs.empty())
    {
        return unmeasuredBook(0);
    }
    for (const PortfolioLeg& leg : legs)
    {
        if (!std::isfinite(leg.signed_exposure))
        {
            return unmeasuredBook(legs.size());
        }
    }

    std::vector<PricedLeg> priced;
    priced.reserve(legs.size());
    for (std::size_t index = 0; index < legs.size(); ++index)
    {
        const PortfolioLeg& leg = legs[index];
        if (leg.signed_exposure == 0.0)
        {
            continue;
        }
        if (!seriesMeasurable(leg.dates, leg.closes))
        {
            return unmeasuredBook(legs.size());
        }
        priced.push_back(PricedLeg{
            .index = index,
            .signed_exposure = leg.signed_exposure,
            .dates = leg.dates,
            .closes = leg.closes,
        });
    }
    if (priced.empty())
    {
        return PortfolioValueAtRisk{
            .var = 0.0,
            .component_var = std::vector<std::optional<double>>(legs.size(), std::optional<double>{0.0}),
        };
    }

    std::vector<SessionDate> shared(priced.front().dates.begin(), priced.front().dates.end());
    for (std::size_t index = 1; index < priced.size(); ++index)
    {
        std::vector<SessionDate> next;
        next.reserve(std::min(shared.size(), priced[index].dates.size()));
        std::ranges::set_intersection(shared, priced[index].dates, std::back_inserter(next));
        shared = std::move(next);
    }

    std::size_t kept = 0;
    std::size_t start = 0;
    if (shared.size() >= 2 && spec.lookback > 0)
    {
        const std::size_t pairs = shared.size() - 1;
        kept = std::min(pairs, spec.lookback);
        start = pairs - kept;
    }
    if (kept < spec.minimum_returns)
    {
        return unmeasuredBook(legs.size());
    }

    std::vector<double> portfolio_pnl(kept, 0.0);
    std::vector<std::vector<double>> leg_pnl(legs.size(), std::vector<double>(kept, 0.0));
    for (std::size_t step = 0; step < kept; ++step)
    {
        const std::size_t earlier_index = start + step;
        const SessionDate earlier = shared[earlier_index];
        const SessionDate later = shared[earlier_index + 1];
        for (const PricedLeg& leg : priced)
        {
            const std::optional<double> profit = legProfit(leg, earlier, later);
            if (!profit.has_value())
            {
                return unmeasuredBook(legs.size());
            }
            const double amount = profit.value();
            leg_pnl[leg.index][step] = amount;
            portfolio_pnl[step] += amount;
        }
    }

    const std::optional<ValueAtRisk> risk = valueAtRisk(portfolio_pnl, spec.confidence);
    const std::optional<std::size_t> chosen = lossQuantileIndex(portfolio_pnl, spec.confidence);
    if (!risk.has_value() || !chosen.has_value())
    {
        return unmeasuredBook(legs.size());
    }
    // One shared day's loss per line. Flat lines stay 0, and the parts add up to var.
    const std::size_t at = chosen.value();
    std::vector<std::optional<double>> components;
    components.reserve(legs.size());
    for (std::size_t index = 0; index < legs.size(); ++index)
    {
        components.emplace_back(-leg_pnl[index][at]);
    }
    return PortfolioValueAtRisk{.var = risk.value().var, .component_var = std::move(components)};
}

}  // namespace terminal
