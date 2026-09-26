// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "backtest/Engine.h"

#include "market_data/Time.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace terminal {
namespace {

enum class ProtectiveExit : std::uint8_t
{
    None = 0,
    Stop,
    Target,
};

class Simulator
{
public:
    Simulator(const BacktestConfig& config, InstrumentId instrument_id, TradeAssetKind kind)
        : config_(config), cash_(config.initial_cash), instrument_id_(instrument_id), kind_(kind)
    {
    }

    [[nodiscard]] double shares() const noexcept
    {
        return shares_;
    }

    [[nodiscard]] double equityAt(double price) const noexcept
    {
        return cash_ + (shares_ * price);
    }

    // Whole shares for a target, sized at the deciding close.
    [[nodiscard]] double desiredShares(double target, double close) const
    {
        if (!std::isfinite(target) || !(close > 0.0))
        {
            return shares_;
        }
        double units = 0.0;
        switch (config_.sizing)
        {
        case BacktestSizing::FixedShares:
            units = config_.sizing_value;
            break;
        case BacktestSizing::FixedNotional:
            units = config_.sizing_value / close;
            break;
        case BacktestSizing::PercentEquity:
            units = std::max(0.0, equityAt(close)) * (config_.sizing_value / 100.0) / close;
            break;
        }
        return std::trunc(target * units);
    }

    // Trades to reach `desired` at `price` before slippage.
    void trade(double desired, double price, UnixSeconds ts, const char* note, std::vector<TradeFill>& fills)
    {
        const double quantity = desired - shares_;
        if (quantity == 0.0 || !(price > 0.0))
        {
            return;
        }
        const double slip = price * (config_.slippage_bps / 10'000.0);
        const double filled = quantity > 0.0 ? price + slip : std::max(0.0, price - slip);
        double fees = std::abs(quantity) * config_.commission_per_share;
        if (config_.commission_per_share > 0.0 || config_.commission_minimum > 0.0)
        {
            fees = std::max(fees, config_.commission_minimum);
        }
        // Average entry follows the position: a fill on the same side averages in, one
        // that flips starts fresh at this price, and a reduction keeps the old average.
        if (shares_ == 0.0 || (shares_ > 0.0) != (desired > 0.0))
        {
            entry_ = filled;
        }
        else if (std::abs(desired) > std::abs(shares_))
        {
            entry_ = ((entry_ * std::abs(shares_)) + (filled * std::abs(quantity))) / std::abs(desired);
        }
        cash_ -= (quantity * filled) + fees;
        shares_ = desired;

        TradeFill fill;
        fill.kind = kind_;
        fill.instrument_id = instrument_id_;
        fill.ts = ts;
        fill.quantity = quantity;
        fill.price = filled;
        fill.fees = fees;
        fill.note = note;
        fills.push_back(std::move(fill));
    }

    // Stop first, then take-profit. Returns the exit that filled, if any.
    [[nodiscard]] ProtectiveExit protect(const Bar& bar, std::vector<TradeFill>& fills)
    {
        if (shares_ == 0.0)
        {
            return ProtectiveExit::None;
        }
        const bool longs = shares_ > 0.0;
        if (config_.stop_loss_pct.has_value())
        {
            const double offset = *config_.stop_loss_pct / 100.0;
            const double level = longs ? entry_ * (1.0 - offset) : entry_ * (1.0 + offset);
            const bool gapped = longs ? bar.open <= level : bar.open >= level;
            const bool touched = longs ? bar.low <= level : bar.high >= level;
            if (gapped || touched)
            {
                trade(0.0, gapped ? bar.open : level, bar.ts, "stop", fills);
                return ProtectiveExit::Stop;
            }
        }
        if (config_.take_profit_pct.has_value())
        {
            const double offset = *config_.take_profit_pct / 100.0;
            const double level = longs ? entry_ * (1.0 + offset) : entry_ * (1.0 - offset);
            const bool gapped = longs ? bar.open >= level : bar.open <= level;
            const bool touched = longs ? bar.high >= level : bar.low <= level;
            if (gapped || touched)
            {
                trade(0.0, gapped ? bar.open : level, bar.ts, "target", fills);
                return ProtectiveExit::Target;
            }
        }
        return ProtectiveExit::None;
    }

private:
    const BacktestConfig& config_;
    double cash_{0.0};
    double shares_{0.0};
    double entry_{0.0};
    InstrumentId instrument_id_{0};
    TradeAssetKind kind_{TradeAssetKind::Equity};
};

[[nodiscard]] std::vector<bool> sessionEnds(std::span<const Bar> bars, const std::string& timezone)
{
    std::vector<bool> ends(bars.size(), false);
    for (std::size_t index = 0; index < bars.size(); ++index)
    {
        if (index + 1 == bars.size())
        {
            ends[index] = true;
            continue;
        }
        ends[index] = utcToSessionDate(timezone, bars[index].ts) != utcToSessionDate(timezone, bars[index + 1].ts);
    }
    return ends;
}

}  // namespace

UnixSeconds barCloseTime(const Bar& bar) noexcept
{
    return bar.timeframe_s == kTimeframe1d ? bar.ts + kUsRthDurationS : bar.ts + bar.timeframe_s;
}

BacktestResult runTargets(std::span<const Bar> bars,
                          std::span<const double> targets,
                          const BacktestConfig& config,
                          InstrumentId instrument_id,
                          TradeAssetKind kind)
{
    BacktestResult result;
    result.targets.assign(targets.begin(), targets.end());
    result.targets.resize(bars.size(), std::numeric_limits<double>::quiet_NaN());
    result.position.reserve(bars.size());
    result.equity.reserve(bars.size());

    std::vector<bool> ends;
    if (config.flatten_at_session_end)
    {
        ends = sessionEnds(bars, config.timezone);
    }
    Simulator sim(config, instrument_id, kind);
    double target = 0.0;
    double sized_target = 0.0;      // the target the current position was sized for
    std::optional<double> pending;  // shares wanted at the next open
    std::optional<double> blocked;  // target that a protective exit closed

    for (std::size_t index = 0; index < bars.size(); ++index)
    {
        const Bar& bar = bars[index];
        if (pending.has_value())
        {
            sim.trade(*pending, bar.open, bar.ts, "open", result.fills);
            pending.reset();
        }
        const ProtectiveExit exit = sim.protect(bar, result.fills);
        if (exit != ProtectiveExit::None)
        {
            blocked = target;
            ++(exit == ProtectiveExit::Stop ? result.stop_exits : result.target_exits);
        }
        const bool last = index + 1 == bars.size();
        if (!ends.empty() && ends[index] && sim.shares() != 0.0 && (!last || config.close_at_end))
        {
            sim.trade(0.0, bar.close, barCloseTime(bar), "session close", result.fills);
        }
        else if (last && config.close_at_end && sim.shares() != 0.0)
        {
            sim.trade(0.0, bar.close, barCloseTime(bar), "end", result.fills);
        }

        const double proposed = result.targets[index];
        if (std::isfinite(proposed))
        {
            target = config.allow_short ? proposed : std::max(0.0, proposed);
        }
        if (blocked.has_value() && target != *blocked)
        {
            blocked.reset();
        }
        // Size on a new target, or to re-enter from flat (after a session close). A held
        // position is not rebalanced as prices move.
        const bool resize = target != sized_target || (sim.shares() == 0.0 && target != 0.0);
        if (!last && !blocked.has_value() && resize)
        {
            const double desired = sim.desiredShares(target, bar.close);
            sized_target = target;
            if (desired != sim.shares())
            {
                pending = desired;
            }
        }
        result.position.push_back(sim.shares());
        result.equity.push_back(sim.equityAt(bar.close));
    }
    return result;
}

BacktestResult runBacktest(std::span<const Bar> bars,
                           const StrategyType& strategy,
                           std::span<const int> options,
                           const BacktestConfig& config,
                           InstrumentId instrument_id,
                           TradeAssetKind kind)
{
    const std::vector<double> targets = strategyTargets(strategy, bars, options);
    return runTargets(bars, targets, config, instrument_id, kind);
}

}  // namespace terminal
