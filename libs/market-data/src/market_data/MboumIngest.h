// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Store.h"
#include "market_data/Types.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

struct HttpResponse
{
    int status{};
    std::string body;
    std::string error;
};

using HttpGet = std::function<HttpResponse(std::string_view url)>;

struct IngestDayResult
{
    SessionDate session_date{};
    CoverageStatus status{CoverageStatus::Error};
    int bar_count{};
    int http_status{};
};

using IngestDayCallback = std::function<void(const IngestDayResult&)>;

struct IngestSymbolResult
{
    InstrumentId instrument_id{};
    std::vector<IngestDayResult> days;
};

// Walks NYSE sessions in [from, to]. Holidays are written complete 0/0.
// Complete coverage rows are skipped. HTTP failures become status=error.
// 401/403 throw. Inject get(); the CLI uses libcurl and retries 0/429/5xx.
[[nodiscard]] IngestSymbolResult ingestSymbol(Store& store,
                                              const HttpGet& get,
                                              std::string_view symbol,
                                              SessionDate from,
                                              SessionDate to,
                                              IngestDayCallback on_day = {});

// Pages GET /v3/markets/historical?interval=daily (newest-N, limit 4000).
// Coverage is written per NYSE weekday in each received page span.
[[nodiscard]] IngestSymbolResult ingestDailySymbol(Store& store,
                                                   const HttpGet& get,
                                                   std::string_view symbol,
                                                   SessionDate from,
                                                   SessionDate to,
                                                   const IngestDayCallback& on_day = {});

}  // namespace terminal
