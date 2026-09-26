// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "trading/Ledger.h"

#include "options/OptionPayoff.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>

namespace terminal {
namespace {

void requireFillNumbers(const TradeFill& fill)
{
    if (!std::isfinite(fill.quantity) || fill.quantity == 0.0)
    {
        throw std::runtime_error("ledger fill quantity is zero or not finite");
    }
    if (!std::isfinite(fill.price) || fill.price < 0.0)
    {
        throw std::runtime_error("ledger fill price is negative or not finite");
    }
    if (!std::isfinite(fill.fees) || fill.fees < 0.0)
    {
        throw std::runtime_error("ledger fill fees are negative or not finite");
    }
}

[[nodiscard]] bool isShare(TradeAssetKind kind) noexcept
{
    return kind != TradeAssetKind::Option;
}

}  // namespace

double positionMultiplier(TradeAssetKind kind) noexcept
{
    return kind == TradeAssetKind::Option ? kContractMultiplier : 1.0;
}

PositionKey positionKey(const TradeFill& fill)
{
    if (!fill.instrument_id.has_value())
    {
        throw std::runtime_error("ledger fill has no instrument");
    }
    PositionKey key;
    key.instrument_id = *fill.instrument_id;
    key.kind = fill.kind;
    if (fill.kind == TradeAssetKind::Option)
    {
        if (!fill.expiration.has_value() || !fill.expiration_type.has_value() || !fill.strike.has_value() ||
            !fill.right.has_value())
        {
            throw std::runtime_error("ledger option fill has no contract");
        }
        key.expiration = *fill.expiration;
        key.expiration_type = *fill.expiration_type;
        key.strike = *fill.strike;
        key.right = *fill.right;
    }
    return key;
}

double fillCashFlow(const TradeFill& fill)
{
    return (-fill.quantity * fill.price * positionMultiplier(fill.kind)) - fill.fees;
}

double unrealizedPnl(const Position& position, double mark) noexcept
{
    return position.quantity * (mark - position.average_price) * position.multiplier;
}

LotBook::LotBook(std::span<const CorporateAction> actions)
{
    for (const CorporateAction& action : actions)
    {
        if (action.type != CorporateActionType::Split || !action.split_ratio.has_value())
        {
            continue;
        }
        const double ratio = *action.split_ratio;
        if (!std::isfinite(ratio) || ratio <= 0.0)
        {
            continue;
        }
        splits_.push_back(Split{.ex_ts = action.ex_ts, .instrument_id = action.instrument_id, .ratio = ratio});
    }
    std::ranges::stable_sort(splits_, {}, &Split::ex_ts);
}

void LotBook::advanceTo(UnixSeconds ts)
{
    if (ts < time_)
    {
        throw std::runtime_error("ledger time moved backward");
    }
    time_ = ts;
    while (next_split_ < splits_.size() && splits_[next_split_].ex_ts <= ts)
    {
        const Split& split = splits_[next_split_];
        for (OpenLot& lot : lots_)
        {
            if (isShare(lot.key.kind) && lot.key.instrument_id == split.instrument_id)
            {
                lot.quantity *= split.ratio;
                lot.price /= split.ratio;
            }
        }
        ++next_split_;
    }
}

void LotBook::apply(const TradeFill& fill)
{
    requireFillNumbers(fill);
    const PositionKey key = positionKey(fill);
    advanceTo(fill.ts);

    const double multiplier = positionMultiplier(fill.kind);
    const double fill_size = std::abs(fill.quantity);
    const std::string symbol = fill.symbol.value_or(std::string{});
    double remaining = fill.quantity;

    for (std::size_t index = 0; index < lots_.size() && remaining != 0.0;)
    {
        OpenLot& lot = lots_[index];
        if (lot.key != key || (lot.quantity > 0.0) == (remaining > 0.0))
        {
            ++index;
            continue;
        }
        const double lot_size = std::abs(lot.quantity);
        const double matched = std::min(std::abs(remaining), lot_size);
        const double closed = std::copysign(matched, lot.quantity);
        const double open_fees = lot.fees * (matched / lot_size);
        const double close_fees = fill.fees * (matched / fill_size);

        RoundTrip trip;
        trip.key = key;
        trip.symbol = symbol.empty() ? lot.symbol : symbol;
        trip.open_fill_id = lot.fill_id;
        trip.close_fill_id = fill.id;
        trip.opened_at = lot.opened_at;
        trip.closed_at = fill.ts;
        trip.quantity = closed;
        trip.entry_price = lot.price;
        trip.exit_price = fill.price;
        trip.multiplier = multiplier;
        trip.gross_pnl = closed * (fill.price - lot.price) * multiplier;
        trip.fees = open_fees + close_fees;
        trip.net_pnl = trip.gross_pnl - trip.fees;
        realized_ += trip.net_pnl;
        trips_.push_back(std::move(trip));

        lot.quantity -= closed;
        lot.fees -= open_fees;
        remaining += closed;
        if (std::abs(remaining) <= kLotQuantityEpsilon)
        {
            remaining = 0.0;
        }
        if (std::abs(lot.quantity) <= kLotQuantityEpsilon)
        {
            lots_.erase(lots_.begin() + static_cast<std::ptrdiff_t>(index));
        }
        else
        {
            ++index;
        }
    }

    if (remaining != 0.0)
    {
        OpenLot lot;
        lot.key = key;
        lot.symbol = symbol;
        lot.fill_id = fill.id;
        lot.opened_at = fill.ts;
        lot.quantity = remaining;
        lot.price = fill.price;
        lot.fees = fill.fees * (std::abs(remaining) / fill_size);
        lots_.push_back(std::move(lot));
    }
    fees_ += fill.fees;
    cash_ += fillCashFlow(fill);
}

const std::vector<OpenLot>& LotBook::openLots() const noexcept
{
    return lots_;
}

const std::vector<RoundTrip>& LotBook::roundTrips() const noexcept
{
    return trips_;
}

std::vector<Position> LotBook::positions() const
{
    std::map<PositionKey, Position> grouped;
    for (const OpenLot& lot : lots_)
    {
        Position& position = grouped[lot.key];
        position.key = lot.key;
        if (!lot.symbol.empty())
        {
            position.symbol = lot.symbol;
        }
        position.multiplier = positionMultiplier(lot.key.kind);
        position.quantity += lot.quantity;
        position.cost_basis += lot.quantity * lot.price * position.multiplier;
        position.open_fees += lot.fees;
        ++position.lots;
    }
    std::vector<Position> out;
    out.reserve(grouped.size());
    for (auto& [key, position] : grouped)
    {
        // Every open lot of one key is on the same side, so quantity is not zero.
        position.average_price = position.cost_basis / (position.quantity * position.multiplier);
        out.push_back(std::move(position));
    }
    return out;
}

double LotBook::realizedPnl() const noexcept
{
    return realized_;
}

double LotBook::feesPaid() const noexcept
{
    return fees_;
}

double LotBook::tradeCash() const noexcept
{
    return cash_;
}

UnixSeconds LotBook::time() const noexcept
{
    return time_;
}

LedgerBook matchLots(std::span<const TradeFill> fills, std::span<const CorporateAction> actions, UnixSeconds as_of)
{
    std::vector<const TradeFill*> ordered;
    ordered.reserve(fills.size());
    for (const TradeFill& fill : fills)
    {
        ordered.push_back(&fill);
    }
    std::ranges::stable_sort(ordered, {}, [](const TradeFill* fill) { return fill->ts; });

    LotBook book(actions);
    for (const TradeFill* fill : ordered)
    {
        book.apply(*fill);
    }
    if (as_of > book.time())
    {
        book.advanceTo(as_of);
    }

    LedgerBook out;
    out.open_lots = book.openLots();
    out.round_trips = book.roundTrips();
    out.positions = book.positions();
    out.realized_pnl = book.realizedPnl();
    out.fees_paid = book.feesPaid();
    out.trade_cash = book.tradeCash();
    return out;
}

}  // namespace terminal
