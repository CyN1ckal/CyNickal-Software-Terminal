// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "trading/TradeStats.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace terminal {
namespace {

constexpr double kSecondsPerYear = 365.25 * 86400.0;

}  // namespace

TradeStats tradeStats(std::span<const RoundTrip> trips)
{
    TradeStats stats;
    stats.trades = trips.size();
    if (trips.empty())
    {
        return stats;
    }
    std::size_t win_run = 0;
    std::size_t loss_run = 0;
    double held = 0.0;
    for (const RoundTrip& trip : trips)
    {
        const double net = trip.net_pnl;
        stats.net_profit += net;
        stats.fees += trip.fees;
        held += static_cast<double>(trip.closed_at - trip.opened_at);
        if (net > 0.0)
        {
            ++stats.wins;
            stats.gross_profit += net;
            stats.largest_win = std::max(stats.largest_win, net);
            ++win_run;
            loss_run = 0;
        }
        else if (net < 0.0)
        {
            ++stats.losses;
            stats.gross_loss += net;
            stats.largest_loss = std::min(stats.largest_loss, net);
            ++loss_run;
            win_run = 0;
        }
        else
        {
            ++stats.breakeven;
            win_run = 0;
            loss_run = 0;
        }
        stats.max_consecutive_wins = std::max(stats.max_consecutive_wins, win_run);
        stats.max_consecutive_losses = std::max(stats.max_consecutive_losses, loss_run);
    }
    const auto count = static_cast<double>(stats.trades);
    stats.win_rate = static_cast<double>(stats.wins) / count;
    stats.average_trade = stats.net_profit / count;
    stats.average_holding_seconds = held / count;
    if (stats.wins > 0)
    {
        stats.average_win = stats.gross_profit / static_cast<double>(stats.wins);
    }
    if (stats.losses > 0)
    {
        stats.average_loss = stats.gross_loss / static_cast<double>(stats.losses);
        stats.profit_factor = stats.gross_profit / -stats.gross_loss;
        if (stats.wins > 0)
        {
            stats.payoff_ratio = stats.average_win / -stats.average_loss;
        }
    }
    return stats;
}

std::vector<double> periodPnl(std::span<const EquityPoint> curve)
{
    std::vector<double> pnl;
    if (curve.size() < 2)
    {
        return pnl;
    }
    pnl.reserve(curve.size() - 1);
    for (std::size_t index = 1; index < curve.size(); ++index)
    {
        pnl.push_back(curve[index].equity - curve[index - 1].equity - curve[index].net_flow);
    }
    return pnl;
}

CurveStats curveStats(std::span<const EquityPoint> curve, CurveStatsSpec spec)
{
    CurveStats stats;
    if (curve.empty())
    {
        return stats;
    }
    const EquityPoint& first = curve.front();
    const EquityPoint& last = curve.back();
    stats.total_return = last.growth - 1.0;
    stats.net_pnl = last.equity - last.contributed;

    std::vector<double> returns;
    returns.reserve(curve.size());
    std::size_t exposed = 0;
    double running_peak = first.growth;
    UnixSeconds peak_ts = first.ts;
    bool below_peak = false;
    for (const EquityPoint& point : curve)
    {
        if (std::isfinite(point.period_return))
        {
            returns.push_back(point.period_return);
        }
        if (point.open_positions > 0)
        {
            ++exposed;
        }
        stats.max_drawdown = std::min(stats.max_drawdown, point.drawdown);
        if (point.growth >= running_peak)
        {
            if (below_peak)
            {
                stats.max_drawdown_seconds = std::max(stats.max_drawdown_seconds, point.ts - peak_ts);
            }
            running_peak = point.growth;
            peak_ts = point.ts;
            below_peak = false;
        }
        else
        {
            below_peak = true;
        }
    }
    if (below_peak)
    {
        stats.max_drawdown_seconds = std::max(stats.max_drawdown_seconds, last.ts - peak_ts);
    }
    stats.periods = returns.size();
    stats.exposure = static_cast<double>(exposed) / static_cast<double>(curve.size());

    const double years = static_cast<double>(last.ts - first.ts) / kSecondsPerYear;
    if (years > 0.0 && last.growth > 0.0)
    {
        stats.cagr = std::pow(last.growth, 1.0 / years) - 1.0;
    }
    if (stats.cagr.has_value() && stats.max_drawdown < 0.0)
    {
        stats.calmar = *stats.cagr / -stats.max_drawdown;
    }

    if (returns.size() >= 2 && spec.periods_per_year > 0.0)
    {
        const auto n = static_cast<double>(returns.size());
        const double rf_period = std::pow(1.0 + spec.risk_free_annual, 1.0 / spec.periods_per_year) - 1.0;
        double sum = 0.0;
        for (const double r : returns)
        {
            sum += r;
        }
        const double mean = sum / n;
        double squares = 0.0;
        double downside = 0.0;
        for (const double r : returns)
        {
            squares += (r - mean) * (r - mean);
            const double below = std::min(0.0, r - rf_period);
            downside += below * below;
        }
        const double deviation = std::sqrt(squares / (n - 1.0));
        const double downside_deviation = std::sqrt(downside / n);
        const double annualize = std::sqrt(spec.periods_per_year);
        const double excess = mean - rf_period;
        stats.volatility = deviation * annualize;
        if (deviation > 0.0)
        {
            stats.sharpe = excess / deviation * annualize;
        }
        if (downside_deviation > 0.0)
        {
            stats.sortino = excess / downside_deviation * annualize;
        }
    }

    const auto pnl = periodPnl(curve);
    stats.var = valueAtRisk(pnl, spec.var_confidence);
    return stats;
}

std::optional<double> buyAndHoldReturn(std::span<const Mark> marks, UnixSeconds from, UnixSeconds to)
{
    const Mark* first = nullptr;
    const Mark* last = nullptr;
    for (const Mark& mark : marks)
    {
        if (mark.ts < from || mark.ts > to)
        {
            continue;
        }
        if (first == nullptr || mark.ts < first->ts)
        {
            first = &mark;
        }
        if (last == nullptr || mark.ts >= last->ts)
        {
            last = &mark;
        }
    }
    if (first == nullptr || first == last || !(first->price > 0.0))
    {
        return std::nullopt;
    }
    return (last->price / first->price) - 1.0;
}

}  // namespace terminal
