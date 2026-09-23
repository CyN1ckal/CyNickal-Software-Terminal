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
// Also fetches split events, including when daily coverage is already complete.
[[nodiscard]] IngestSymbolResult ingestDailySymbol(Store& store,
                                                   const HttpGet& get,
                                                   std::string_view symbol,
                                                   SessionDate from,
                                                   SessionDate to,
                                                   const IngestDayCallback& on_day = {});

struct IngestSplitsResult
{
    InstrumentId instrument_id{};
    int upserted{};
};

// GET /v1/markets/stock/history events.splits only. Does not write bars.
// Transport errors, non-200 responses, and parse failures throw.
// corporate_action is left unchanged. HTTP 200 with no splits writes nothing.
// A second split at the same ex_ts with a different ratio is not inserted.
[[nodiscard]] IngestSplitsResult ingestSplits(Store& store,
                                              const HttpGet& get,
                                              std::string_view symbol);

struct IngestStatementResult
{
    InstrumentId instrument_id{};
    int cell_count{};
    bool no_data{false};
};

// GET /v1/markets/stock/modules for one v2 statement and timeframe.
// HTTP 200 with no grid records an empty snapshot so the pane does not fetch it again.
// Transport errors, non-200 responses, and parse failures throw and leave the grid unchanged.
[[nodiscard]] IngestStatementResult ingestStatement(Store& store,
                                                    const HttpGet& get,
                                                    std::string_view symbol,
                                                    StatementKind statement,
                                                    StatementTimeframe timeframe);

struct IngestOptionsResult
{
    InstrumentId instrument_id{};
    std::string symbol;
    int quote_count{};
    int expiration_count{};
    bool no_data{false};
};

// GET /v3/markets/options. expiration 0 lets the server choose the date.
// An unknown ticker writes nothing. A known name with an unlisted date
// refreshes the calendar and leaves stored quotes for dates still listed.
// The stored symbol is the vendor base ($SPX when the request was SPX).
[[nodiscard]] IngestOptionsResult ingestOptions(Store& store,
                                                const HttpGet& get,
                                                std::string_view symbol,
                                                SessionDate expiration = 0);

}  // namespace terminal
