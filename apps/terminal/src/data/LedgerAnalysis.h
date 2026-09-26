// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "data/IngestWorker.h"
#include "market_data/Store.h"
#include "trading/EquityCurve.h"
#include "trading/Ledger.h"
#include "trading/TradeStats.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// Everything the statistics window shows for one ledger, computed from the store.
struct LedgerAnalysis
{
    std::vector<EquityPoint> curve;
    CurveStats curve_stats;
    TradeStats trade_stats;
    std::vector<RoundTrip> round_trips;
    // Split-adjusted closes of the benchmark within the curve's span, and their return.
    std::vector<Mark> benchmark_marks;
    std::optional<double> benchmark_return;
    // Newest coverage ingested_at among the instruments that priced the curve.
    std::optional<UnixSeconds> received_at;
    // Traded shares with no stored daily close; their positions are held at cost.
    std::vector<std::string> unpriced_symbols;
};

// Evaluates the ledger at every stored daily close of the shares it traded, from its
// first fill or cash flow through now. A ledger whose shares have no closes is
// evaluated at its own fill and cash-flow times instead. benchmark is optional.
[[nodiscard]] LedgerAnalysis analyzeLedger(const Store& store,
                                           LedgerId id,
                                           std::optional<InstrumentId> benchmark,
                                           UnixSeconds now,
                                           CurveStatsSpec spec = {});

// Daily history for every share instrument the fills traded, plus benchmark_symbol
// when it is not empty. missing_only skips instruments that already have a daily bar.
[[nodiscard]] std::vector<IngestWorker::Job> ledgerHistoryFetchJobs(const Store& store,
                                                                    std::span<const TradeFill> fills,
                                                                    std::string_view benchmark_symbol,
                                                                    SessionDate today,
                                                                    bool missing_only = true);

}  // namespace terminal
