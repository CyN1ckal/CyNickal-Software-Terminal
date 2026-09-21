#pragma once

#include "market_data/Store.h"
#include "market_data/Types.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace myapp {

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

struct IngestSymbolResult
{
    InstrumentId instrument_id{};
    std::vector<IngestDayResult> days;
};

// Walks NYSE sessions in [from, to]. Holidays are written complete 0/0.
// Complete coverage rows are skipped. HTTP failures become status=error.
// 401/403 throw. Inject get(); the CLI wraps curl and 429 retries.
[[nodiscard]] IngestSymbolResult ingestSymbol(Store& store,
                                              const HttpGet& get,
                                              std::string_view symbol,
                                              SessionDate from,
                                              SessionDate to);

}  // namespace myapp
