// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "data/IngestJob.h"
#include "market_data/Store.h"
#include "trading/Ledger.h"

#include <optional>
#include <span>
#include <vector>

namespace terminal {

// A current price for one open position. as_of is when the price was true (a
// bar's open, or a quote's fetch time). received_at is when the store got it.
struct LedgerMark
{
    double price{};
    UnixSeconds as_of{};
    UnixSeconds received_at{};
};

// The as-traded close of the newest stored bar, daily or 1-minute, whichever is later.
[[nodiscard]] std::optional<LedgerMark> latestShareMark(const Store& store, InstrumentId id);

// The stored chain's last print for this contract, or its mid when it has not printed.
// nullopt for a share key, an unquoted contract, or one with neither a print nor a mid.
[[nodiscard]] std::optional<LedgerMark> latestOptionMark(const Store& store, const PositionKey& key);

// Open positions as holdings, so portfolioFetchJobs can plan their prices. The
// symbol and listing state come from the newest fill of each instrument.
[[nodiscard]] std::vector<PortfolioHolding> positionsAsHoldings(std::span<const Position> positions,
                                                                std::span<const TradeFill> fills);

[[nodiscard]] std::vector<IngestJob> ledgerFetchJobs(const Store& store,
                                                             std::span<const Position> positions,
                                                             std::span<const TradeFill> fills,
                                                             SessionDate today,
                                                             bool missing_only = true);

}  // namespace terminal
