// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "data/PortfolioFetch.h"

#include "IngestDefaults.h"

#include "market_data/NyseCalendar.h"
#include "market_data/Time.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace terminal {

std::vector<IngestWorker::Job> portfolioFetchJobs(const Store& store,
                                                std::span<const PortfolioHolding> holdings,
                                                SessionDate today,
                                                bool missing_only)
{
    const auto ymd = sessionDateToYmd(today);
    const SessionDate from =
        toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{kIngestDefaultDailyDays});

    std::vector<IngestWorker::Job> jobs;
    // One options fetch covers every contract on that symbol and expiration date.
    std::vector<std::pair<std::string, SessionDate>> planned_options;
    for (const PortfolioHolding& holding : holdings)
    {
        if (holding.kind == PortfolioAssetKind::Cash || !holding.listing_open ||
            !holding.instrument_id.has_value() || !holding.symbol.has_value())
        {
            continue;
        }
        const InstrumentId instrument_id = *holding.instrument_id;
        const std::string& symbol = *holding.symbol;
        if (holding.kind == PortfolioAssetKind::Equity || holding.kind == PortfolioAssetKind::Etf)
        {
            if (missing_only)
            {
                const std::vector<CoverageDay> days = store.queryCoverageDays(instrument_id, kTimeframe1d);
                const bool has_bars =
                    std::ranges::any_of(days, [](const CoverageDay& day) { return day.bar_count > 0; });
                if (has_bars)
                {
                    continue;
                }
            }
            IngestWorker::Job job;
            job.symbol = symbol;
            job.timeframe_s = kTimeframe1d;
            job.from = from;
            job.to = today;
            jobs.push_back(std::move(job));
            continue;
        }
        if (holding.kind != PortfolioAssetKind::Option || !holding.expiration.has_value() ||
            !holding.expiration_type.has_value() || !holding.strike.has_value() || !holding.right.has_value())
        {
            continue;
        }
        const SessionDate expiration = *holding.expiration;
        const OptionExpirationType expiration_type = *holding.expiration_type;
        const double strike = *holding.strike;
        const OptionRight right = *holding.right;
        if (missing_only)
        {
            const std::vector<OptionQuote> quotes =
                store.queryOptionQuotes(instrument_id, expiration, expiration_type);
            const bool matched = std::ranges::any_of(quotes, [strike, right](const OptionQuote& quote) {
                return quote.strike == strike && quote.right == right;
            });
            if (matched)
            {
                continue;
            }
        }
        const std::pair<std::string, SessionDate> key{symbol, expiration};
        if (std::ranges::find(planned_options, key) != planned_options.end())
        {
            continue;
        }
        planned_options.emplace_back(symbol, expiration);
        IngestWorker::Job job;
        job.symbol = symbol;
        job.options = true;
        job.option_expiration = expiration;
        jobs.push_back(std::move(job));
    }
    return jobs;
}

std::vector<IngestWorker::Job> portfolioFetchJobs(const Store& store, PortfolioId id, SessionDate today,
                                                  bool missing_only)
{
    return portfolioFetchJobs(store, store.queryHoldings(id), today, missing_only);
}

}  // namespace terminal
