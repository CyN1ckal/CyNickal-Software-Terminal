// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "data/IngestWorker.h"
#include "market_data/Store.h"

#include <span>
#include <vector>

namespace terminal {

// Open listings. Cash and a closed listing produce nothing.
// missing_only skips an equity that already has a daily bar and an option that
// already has a matching quote. A refresh passes false so those names are fetched again.
[[nodiscard]] std::vector<IngestWorker::Job> portfolioFetchJobs(const Store& store,
                                                                std::span<const PortfolioHolding> holdings,
                                                                SessionDate today,
                                                                bool missing_only = true);

[[nodiscard]] std::vector<IngestWorker::Job> portfolioFetchJobs(const Store& store,
                                                                PortfolioId id,
                                                                SessionDate today,
                                                                bool missing_only = true);

inline void enqueuePortfolioFetches(IngestWorker& worker, std::span<const IngestWorker::Job> jobs)
{
    for (const IngestWorker::Job& job : jobs)
    {
        worker.enqueue(job);
    }
}

}  // namespace terminal
