// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "data/LedgerMarks.h"

#include "data/PortfolioFetch.h"

#include <cmath>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace terminal {
namespace {

[[nodiscard]] std::optional<LedgerMark> latestClose(const Store& store, InstrumentId id, int timeframe_s)
{
    const std::vector<CoverageDay> days = store.queryCoverageDays(id, timeframe_s);
    const CoverageDay* best = nullptr;
    UnixSeconds latest = 0;
    for (const CoverageDay& day : days)
    {
        if (day.bar_count <= 0 || !day.last_ts.has_value())
        {
            continue;
        }
        const UnixSeconds last_ts = *day.last_ts;
        if (best == nullptr || last_ts > latest)
        {
            best = &day;
            latest = last_ts;
        }
    }
    if (best == nullptr)
    {
        return std::nullopt;
    }
    const std::vector<Bar> bars = store.queryBars(id, timeframe_s, latest, latest + 1);
    if (bars.empty())
    {
        return std::nullopt;
    }
    LedgerMark mark;
    mark.price = bars.back().close;
    mark.as_of = bars.back().ts;
    mark.received_at = best->ingested_at;
    return mark;
}

}  // namespace

std::optional<LedgerMark> latestShareMark(const Store& store, InstrumentId id)
{
    const std::optional<LedgerMark> daily = latestClose(store, id, kTimeframe1d);
    const std::optional<LedgerMark> minute = latestClose(store, id, kTimeframe1m);
    if (!daily.has_value())
    {
        return minute;
    }
    if (!minute.has_value() || minute->as_of <= daily->as_of)
    {
        return daily;
    }
    return minute;
}

std::optional<LedgerMark> latestOptionMark(const Store& store, const PositionKey& key)
{
    if (key.kind != TradeAssetKind::Option)
    {
        return std::nullopt;
    }
    const std::vector<OptionQuote> quotes = store.queryOptionQuotes(key.instrument_id, key.expiration,
                                                                    key.expiration_type);
    for (const OptionQuote& quote : quotes)
    {
        if (quote.right != key.right || std::abs(quote.strike - key.strike) > 0.0001)
        {
            continue;
        }
        LedgerMark mark;
        mark.price = quote.last;
        mark.as_of = quote.fetched_at;
        mark.received_at = quote.fetched_at;
        return mark;
    }
    return std::nullopt;
}

std::vector<PortfolioHolding> positionsAsHoldings(std::span<const Position> positions,
                                                  std::span<const TradeFill> fills)
{
    struct Listing
    {
        UnixSeconds ts{};
        std::optional<std::string> symbol;
        bool open{false};
    };
    std::map<InstrumentId, Listing> listings;
    for (const TradeFill& fill : fills)
    {
        if (!fill.instrument_id.has_value())
        {
            continue;
        }
        Listing& listing = listings[*fill.instrument_id];
        if (!listing.symbol.has_value() || fill.ts >= listing.ts)
        {
            listing.ts = fill.ts;
            listing.symbol = fill.symbol;
            listing.open = fill.listing_open;
        }
    }

    std::vector<PortfolioHolding> holdings;
    holdings.reserve(positions.size());
    for (const Position& position : positions)
    {
        PortfolioHolding holding;
        switch (position.key.kind)
        {
        case TradeAssetKind::Equity:
            holding.kind = PortfolioAssetKind::Equity;
            break;
        case TradeAssetKind::Etf:
            holding.kind = PortfolioAssetKind::Etf;
            break;
        case TradeAssetKind::Option:
            holding.kind = PortfolioAssetKind::Option;
            holding.expiration = position.key.expiration;
            holding.expiration_type = position.key.expiration_type;
            holding.strike = position.key.strike;
            holding.right = position.key.right;
            break;
        }
        holding.instrument_id = position.key.instrument_id;
        if (const auto found = listings.find(position.key.instrument_id); found != listings.end())
        {
            holding.symbol = found->second.symbol;
            holding.listing_open = found->second.open;
        }
        holding.quantity = position.quantity;
        holdings.push_back(std::move(holding));
    }
    return holdings;
}

std::vector<IngestWorker::Job> ledgerFetchJobs(const Store& store,
                                               std::span<const Position> positions,
                                               std::span<const TradeFill> fills,
                                               SessionDate today,
                                               bool missing_only)
{
    const std::vector<PortfolioHolding> holdings = positionsAsHoldings(positions, fills);
    return portfolioFetchJobs(store, holdings, today, missing_only);
}

}  // namespace terminal
