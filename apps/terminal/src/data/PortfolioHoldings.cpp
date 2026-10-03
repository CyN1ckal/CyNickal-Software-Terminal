// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "data/PortfolioHoldings.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <utility>

namespace terminal {
namespace {

// The chain treats strikes this close as one contract. The holding key does too.
constexpr double kStrikeTolerance = 0.0001;

[[nodiscard]] bool isShare(PortfolioAssetKind kind) noexcept
{
    return kind == PortfolioAssetKind::Equity || kind == PortfolioAssetKind::Etf;
}

[[nodiscard]] bool sameInstrument(const PortfolioHolding& held, const PortfolioHolding& incoming)
{
    const std::optional<std::string> held_figi = held.figi;
    const std::optional<std::string> incoming_figi = incoming.figi;
    if (held_figi.has_value() && incoming_figi.has_value())
    {
        return held_figi.value() == incoming_figi.value();
    }
    const std::optional<InstrumentId> held_id = held.instrument_id;
    const std::optional<InstrumentId> incoming_id = incoming.instrument_id;
    if (held_id.has_value() && incoming_id.has_value())
    {
        return held_id.value() == incoming_id.value();
    }
    return false;
}

[[nodiscard]] bool sameContract(const PortfolioHolding& held, const PortfolioHolding& incoming)
{
    const std::optional<SessionDate> held_expiration = held.expiration;
    const std::optional<SessionDate> incoming_expiration = incoming.expiration;
    if (!held_expiration.has_value() || !incoming_expiration.has_value() ||
        held_expiration.value() != incoming_expiration.value())
    {
        return false;
    }
    const std::optional<OptionExpirationType> held_type = held.expiration_type;
    const std::optional<OptionExpirationType> incoming_type = incoming.expiration_type;
    if (!held_type.has_value() || !incoming_type.has_value() || held_type.value() != incoming_type.value())
    {
        return false;
    }
    const std::optional<OptionRight> held_right = held.right;
    const std::optional<OptionRight> incoming_right = incoming.right;
    if (!held_right.has_value() || !incoming_right.has_value() || held_right.value() != incoming_right.value())
    {
        return false;
    }
    const std::optional<double> held_strike = held.strike;
    const std::optional<double> incoming_strike = incoming.strike;
    if (!held_strike.has_value() || !incoming_strike.has_value())
    {
        return false;
    }
    return std::fabs(held_strike.value() - incoming_strike.value()) <= kStrikeTolerance;
}

[[nodiscard]] bool optionUnderlying(AssetClass asset_class) noexcept
{
    return asset_class == AssetClass::Equity || asset_class == AssetClass::Etf || asset_class == AssetClass::Index;
}

void fillGaps(PortfolioHolding& held, const PortfolioHolding& incoming)
{
    if (!held.figi.has_value() && incoming.figi.has_value())
    {
        held.figi = incoming.figi;
    }
    if (!held.instrument_id.has_value() && incoming.instrument_id.has_value())
    {
        held.instrument_id = incoming.instrument_id;
    }
    if (!held.symbol.has_value() && incoming.symbol.has_value())
    {
        held.symbol = incoming.symbol;
    }
    if (!held.vendor_symbol.has_value() && incoming.vendor_symbol.has_value())
    {
        held.vendor_symbol = incoming.vendor_symbol;
    }
    if (incoming.listing_open)
    {
        held.listing_open = true;
    }
}

}  // namespace

bool sameHolding(const PortfolioHolding& held, const PortfolioHolding& incoming)
{
    if (held.kind == PortfolioAssetKind::Cash || incoming.kind == PortfolioAssetKind::Cash)
    {
        return held.kind == PortfolioAssetKind::Cash && incoming.kind == PortfolioAssetKind::Cash;
    }
    if (!sameInstrument(held, incoming))
    {
        return false;
    }
    if (held.kind == PortfolioAssetKind::Option || incoming.kind == PortfolioAssetKind::Option)
    {
        return held.kind == PortfolioAssetKind::Option && incoming.kind == PortfolioAssetKind::Option &&
               sameContract(held, incoming);
    }
    return isShare(held.kind) && isShare(incoming.kind);
}

HoldingAggregate aggregateHolding(std::vector<PortfolioHolding>& holdings, PortfolioHolding incoming)
{
    const auto found =
        std::ranges::find_if(holdings, [&](const PortfolioHolding& held) { return sameHolding(held, incoming); });
    if (found == holdings.end())
    {
        const std::size_t index = holdings.size();
        holdings.push_back(std::move(incoming));
        return HoldingAggregate{
            .action = HoldingAggregateAction::Added,
            .index = index,
        };
    }

    const auto index = static_cast<std::size_t>(std::distance(holdings.begin(), found));
    auto& held = *found;
    const double total = held.quantity + incoming.quantity;
    if (!std::isfinite(total) || total == 0.0)
    {
        holdings.erase(found);
        return HoldingAggregate{
            .action = HoldingAggregateAction::Removed,
            .index = index,
        };
    }
    held.quantity = total;
    fillGaps(held, incoming);
    return HoldingAggregate{
        .action = HoldingAggregateAction::Combined,
        .index = index,
    };
}

std::vector<PortfolioHolding> aggregateHoldings(std::span<const PortfolioHolding> holdings)
{
    std::vector<PortfolioHolding> book;
    book.reserve(holdings.size());
    for (const PortfolioHolding& holding : holdings)
    {
        (void)aggregateHolding(book, holding);
    }
    return book;
}

HoldingRetarget retargetHolding(std::vector<PortfolioHolding>& holdings, std::size_t index, const Instrument& instrument)
{
    const std::optional<std::string> figi = instrument.figi;
    if (index >= holdings.size() || !figi.has_value())
    {
        return HoldingRetarget{
            .action = HoldingRetargetAction::Rejected,
            .index = index,
        };
    }
    const std::string& resolved_figi = figi.value();
    PortfolioHolding& row = holdings[index];
    if (row.kind == PortfolioAssetKind::Cash)
    {
        return HoldingRetarget{
            .action = HoldingRetargetAction::Rejected,
            .index = index,
        };
    }

    PortfolioAssetKind kind = row.kind;
    if (kind == PortfolioAssetKind::Option)
    {
        if (!optionUnderlying(instrument.asset_class))
        {
            return HoldingRetarget{
                .action = HoldingRetargetAction::Rejected,
                .index = index,
            };
        }
    }
    else if (instrument.asset_class == AssetClass::Equity)
    {
        kind = PortfolioAssetKind::Equity;
    }
    else if (instrument.asset_class == AssetClass::Etf)
    {
        kind = PortfolioAssetKind::Etf;
    }
    else
    {
        return HoldingRetarget{
            .action = HoldingRetargetAction::Rejected,
            .index = index,
        };
    }

    PortfolioHolding next = row;
    next.kind = kind;
    next.figi = resolved_figi;
    next.instrument_id = instrument.id;
    next.symbol = instrument.symbol;
    next.listing_open = instrument.listing_open;
    if (row.kind == PortfolioAssetKind::Option && !sameHolding(row, next))
    {
        next.vendor_symbol.reset();
    }

    std::optional<std::size_t> other;
    for (std::size_t candidate = 0; candidate < holdings.size(); ++candidate)
    {
        if (candidate == index || !sameHolding(holdings[candidate], next))
        {
            continue;
        }
        other = candidate;
        break;
    }
    if (!other.has_value())
    {
        row = std::move(next);
        return HoldingRetarget{
            .action = HoldingRetargetAction::Updated,
            .index = index,
        };
    }

    const std::size_t other_index = other.value();
    const double total = holdings[other_index].quantity + next.quantity;
    if (!std::isfinite(total) || total == 0.0)
    {
        const std::size_t hi = std::max(index, other_index);
        const std::size_t lo = std::min(index, other_index);
        holdings.erase(holdings.begin() + static_cast<std::ptrdiff_t>(hi));
        holdings.erase(holdings.begin() + static_cast<std::ptrdiff_t>(lo));
        return HoldingRetarget{
            .action = HoldingRetargetAction::Removed,
            .index = lo,
        };
    }

    PortfolioHolding& survivor = holdings[other_index];
    survivor.quantity = total;
    fillGaps(survivor, next);
    holdings.erase(holdings.begin() + static_cast<std::ptrdiff_t>(index));
    const std::size_t at = index < other_index ? other_index - 1 : other_index;
    return HoldingRetarget{
        .action = HoldingRetargetAction::Combined,
        .index = at,
    };
}

}  // namespace terminal
