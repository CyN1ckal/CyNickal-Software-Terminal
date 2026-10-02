// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <string>

namespace terminal {

// One fetch a planner can describe without starting the ingest thread.
// IngestWorker::Job is this type. The worker assigns serial when it accepts the job.
struct IngestJob
{
    std::string symbol;
    SessionDate from{};
    SessionDate to{};
    int timeframe_s{kTimeframe1m};
    bool splits_only{false};
    std::uint64_t serial{0};
    bool statements{false};
    StatementKind statement{StatementKind::Income};
    StatementTimeframe statement_timeframe{StatementTimeframe::Annually};
    bool options{false};
    SessionDate option_expiration{0};
};

}  // namespace terminal
