// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "data/IngestWorker.h"
#include "market_data/Store.h"

#include <span>
#include <vector>

namespace terminal {

// Open listings that still have no daily bar or matching option quote.
// Cash and a closed listing produce nothing.
[[nodiscard]] std::vector<IngestWorker::Job> portfolioFetchJobs(const Store& store,
                                                                PortfolioId id,
                                                                SessionDate today);

inline void enqueuePortfolioFetches(IngestWorker& worker, std::span<const IngestWorker::Job> jobs)
{
    for (const IngestWorker::Job& job : jobs)
    {
        worker.enqueue(job);
    }
}

}  // namespace terminal
