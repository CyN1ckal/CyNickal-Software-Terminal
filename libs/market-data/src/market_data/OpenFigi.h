// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Http.h"
#include "market_data/Types.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// POST /v3/mapping. Behavior below was observed against the live API on 2026-09-23;
// the requests and responses are in libs/market-data/tests/fixtures/openfigi/.
inline constexpr std::string_view kOpenFigiMappingUrl = "https://api.openfigi.com/v3/mapping";
inline constexpr std::size_t kOpenFigiJobsWithoutKey = 10;
inline constexpr std::size_t kOpenFigiJobsWithKey = 100;
inline constexpr int kOpenFigiWindowWithoutKeyS = 60;
inline constexpr int kOpenFigiWindowWithKeyS = 6;
inline constexpr int kOpenFigi429Retries = 3;

struct OpenFigiJob
{
    std::string id_type;
    std::string id_value;
    std::optional<std::string> exch_code;
    std::optional<std::string> market_sec_des;
};

struct OpenFigiHit
{
    std::string figi;
    std::optional<std::string> composite_figi;
    std::string ticker;
    std::string name;
    std::optional<std::string> exch_code;
    std::string market_sector;
    std::string security_type;
};

// One array element of the response. {"warning": ...} is NoMatch, an answer.
// {"error": ...} and every transport failure are Unreachable.
enum class OpenFigiAnswer : std::uint8_t
{
    Hits,
    NoMatch,
    Unreachable
};

struct OpenFigiJobResult
{
    OpenFigiAnswer answer{OpenFigiAnswer::Unreachable};
    std::vector<OpenFigiHit> hits;
    std::string message;
};

[[nodiscard]] std::string buildOpenFigiMappingBody(std::span<const OpenFigiJob> jobs);

// nullopt when the body is not a JSON array with exactly job_count elements.
[[nodiscard]] std::optional<std::vector<OpenFigiJobResult>> parseOpenFigiMappingResponse(std::string_view body,
                                                                                         std::size_t job_count);

// ratelimit-policy ("25;w=60"), ratelimit-limit, ratelimit-remaining, ratelimit-reset (seconds).
struct OpenFigiRateLimit
{
    std::optional<int> limit;
    std::optional<int> window_s;
    std::optional<int> remaining;
    std::optional<int> reset_s;
};

[[nodiscard]] OpenFigiRateLimit parseOpenFigiRateLimit(const HttpResponse& response);

// A leading $ marks an index in this store ($SPX).
[[nodiscard]] bool isIndexSymbol(std::string_view symbol) noexcept;
// $SPX -> "SPX Index"; BRK.B -> "BRK/B". Uppercased.
[[nodiscard]] std::string toOpenFigiTicker(std::string_view symbol);
// "SPX Index" -> $SPX; BRK/B -> BRK.B.
[[nodiscard]] std::string fromOpenFigiTicker(std::string_view ticker);
// Case-insensitive ticker equality.
[[nodiscard]] bool sameTicker(std::string_view a, std::string_view b) noexcept;

// Forward: ticker -> FIGI, active securities only. includeUnlistedEquities is never sent.
[[nodiscard]] OpenFigiJob forwardJob(std::string_view symbol);
// Reverse: FIGI -> ticker. A delisted FIGI still returns its last ticker.
[[nodiscard]] OpenFigiJob reverseJob(std::string_view figi);

// Index for marketSector Index; etf for Equity/ETP; equity for any other Equity.
[[nodiscard]] std::optional<AssetClass> classifyOpenFigiHit(const OpenFigiHit& hit);

enum class ForwardKind : std::uint8_t
{
    Confirmed,
    NoMatch,
    Ambiguous,
    Unsupported,
    Unreachable
};

struct ForwardOutcome
{
    ForwardKind kind{ForwardKind::Unreachable};
    std::string figi;
    AssetClass asset_class{AssetClass::Equity};
    std::string name;
    std::string message;
};

// Equity and ETF jobs reduce on the distinct compositeFIGI values; a hit without one is
// ignored, so a venue figi is never stored. Index jobs use figi when compositeFIGI is
// absent. Exactly one is Confirmed when its class fits the job: an index job must
// return an index, and an equity job an equity or ETF; otherwise Unsupported.
// Only an empty data array or the warning is NoMatch. Hits that carry no FIGI are
// Unreachable, so an unreadable answer never reads as "not listed".
[[nodiscard]] ForwardOutcome reduceForward(const OpenFigiJobResult& result, bool index);

enum class ReverseKind : std::uint8_t
{
    Ticker,
    NoMatch,
    Ambiguous,
    Unreachable
};

struct ReverseOutcome
{
    ReverseKind kind{ReverseKind::Unreachable};
    std::string ticker;  // store spelling ($SPX, BRK.B)
    std::string message;
};

// Hits that carry no ticker are Unreachable, the same rule as reduceForward.
[[nodiscard]] ReverseOutcome reduceReverse(const OpenFigiJobResult& result);

using SleepFn = std::function<void(std::chrono::milliseconds)>;

// Batches jobs (10 per request without a key, 100 with one) and honours the
// rate-limit headers. The key header itself is added by the injected post.
class OpenFigiClient
{
public:
    OpenFigiClient(HttpPost post, bool has_key, SleepFn sleep = {});

    [[nodiscard]] std::size_t batchSize() const noexcept;
    [[nodiscard]] int requestCount() const noexcept { return requests_; }

    // One result per job, in order. When ratelimit-remaining reaches 0 the next request
    // waits ratelimit-reset seconds. A 429 waits the same way (one window when the
    // header is missing) and retries up to 3 times; after that the batch is Unreachable.
    // Throws on HTTP 413, which means the batch was too large.
    [[nodiscard]] std::vector<OpenFigiJobResult> map(std::span<const OpenFigiJob> jobs);

    [[nodiscard]] ForwardOutcome forward(std::string_view symbol);
    [[nodiscard]] ReverseOutcome reverse(std::string_view figi);

private:
    void sleepSeconds(int seconds);

    HttpPost post_;
    bool has_key_{false};
    SleepFn sleep_;
    int pending_wait_s_{0};
    int requests_{0};
};

}  // namespace terminal
