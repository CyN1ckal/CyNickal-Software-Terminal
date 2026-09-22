// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

using UnixSeconds = std::int64_t;
using SessionDate = std::int32_t;
using InstrumentId = std::int64_t;

inline constexpr int kTimeframe1m = 60;
inline constexpr int kTimeframe1d = 86400;
inline constexpr int kUsRthExpected1m = 390;
inline constexpr int kUsRthExpected1d = 1;
inline constexpr int kUsRthDurationS = 23400;  // 09:30–16:00 local; daily forming window
inline constexpr int kMboumDailyPageLimit = 4000;
inline constexpr int kSchemaUserVersion = 1;

enum class AssetClass : std::uint8_t
{
    Equity,
    Etf,
    Index,
    Future,
    Crypto,
    Other
};

enum class CoverageStatus : std::uint8_t
{
    Complete,
    Partial,
    Missing,
    Error
};

enum class CorporateActionType : std::uint8_t
{
    Split,
    Dividend,
    Spinoff,
    Other
};

struct Instrument
{
    InstrumentId id{};
    std::string symbol;
    std::optional<std::string> exchange;
    AssetClass asset_class{AssetClass::Equity};
    std::string currency{"USD"};
    std::string timezone{"America/New_York"};
    std::optional<std::string> name;
    std::optional<UnixSeconds> listed_at;
    std::optional<UnixSeconds> delisted_at;
    UnixSeconds created_at{};
};

struct Bar
{
    InstrumentId instrument_id{};
    int timeframe_s{kTimeframe1m};
    UnixSeconds ts{};
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
};

struct CoverageDay
{
    InstrumentId instrument_id{};
    int timeframe_s{kTimeframe1m};
    SessionDate session_date{};
    std::optional<UnixSeconds> first_ts;
    std::optional<UnixSeconds> last_ts;
    int bar_count{};
    std::optional<int> expected_count;
    CoverageStatus status{CoverageStatus::Missing};
    std::string source{"mboum"};
    UnixSeconds ingested_at{};
};

struct CoverageSummary
{
    Instrument instrument;
    int timeframe_s{kTimeframe1m};
    std::optional<SessionDate> first_session;
    std::optional<SessionDate> last_session;
    int bar_count{};
    int session_count{};
    int complete_count{};
    int partial_count{};
    int missing_count{};
    int error_count{};
    std::optional<UnixSeconds> last_ingested_at;
};

struct CorporateAction
{
    std::int64_t id{};
    InstrumentId instrument_id{};
    UnixSeconds ex_ts{};
    CorporateActionType type{CorporateActionType::Other};
    std::optional<double> split_ratio;
    std::optional<double> amount;
    std::optional<std::string> currency;
    std::string source{"mboum"};
};

struct UpsertBarsResult
{
    int written{};
    int rejected{};
};

struct IngestSessionResult
{
    CoverageDay coverage;
    UpsertBarsResult bars;
};

struct IngestDailyRangeResult
{
    UpsertBarsResult bars;
    std::vector<CoverageDay> coverage;
};

inline std::string_view toSql(AssetClass value)
{
    switch (value)
    {
    case AssetClass::Equity:
        return "equity";
    case AssetClass::Etf:
        return "etf";
    case AssetClass::Index:
        return "index";
    case AssetClass::Future:
        return "future";
    case AssetClass::Crypto:
        return "crypto";
    case AssetClass::Other:
        return "other";
    }
    throw std::runtime_error("unknown AssetClass");
}

inline std::string_view toSql(CoverageStatus value)
{
    switch (value)
    {
    case CoverageStatus::Complete:
        return "complete";
    case CoverageStatus::Partial:
        return "partial";
    case CoverageStatus::Missing:
        return "missing";
    case CoverageStatus::Error:
        return "error";
    }
    throw std::runtime_error("unknown CoverageStatus");
}

inline std::string_view toSql(CorporateActionType value)
{
    switch (value)
    {
    case CorporateActionType::Split:
        return "split";
    case CorporateActionType::Dividend:
        return "dividend";
    case CorporateActionType::Spinoff:
        return "spinoff";
    case CorporateActionType::Other:
        return "other";
    }
    throw std::runtime_error("unknown CorporateActionType");
}

inline AssetClass assetClassFromSql(std::string_view text)
{
    if (text == "equity")
    {
        return AssetClass::Equity;
    }
    if (text == "etf")
    {
        return AssetClass::Etf;
    }
    if (text == "index")
    {
        return AssetClass::Index;
    }
    if (text == "future")
    {
        return AssetClass::Future;
    }
    if (text == "crypto")
    {
        return AssetClass::Crypto;
    }
    if (text == "other")
    {
        return AssetClass::Other;
    }
    throw std::runtime_error("unknown asset_class");
}

inline CoverageStatus coverageStatusFromSql(std::string_view text)
{
    if (text == "complete")
    {
        return CoverageStatus::Complete;
    }
    if (text == "partial")
    {
        return CoverageStatus::Partial;
    }
    if (text == "missing")
    {
        return CoverageStatus::Missing;
    }
    if (text == "error")
    {
        return CoverageStatus::Error;
    }
    throw std::runtime_error("unknown coverage status");
}

inline CorporateActionType corporateActionTypeFromSql(std::string_view text)
{
    if (text == "split")
    {
        return CorporateActionType::Split;
    }
    if (text == "dividend")
    {
        return CorporateActionType::Dividend;
    }
    if (text == "spinoff")
    {
        return CorporateActionType::Spinoff;
    }
    if (text == "other")
    {
        return CorporateActionType::Other;
    }
    throw std::runtime_error("unknown corporate_action type");
}

inline bool isValidBar(const Bar& b) noexcept
{
    if (b.timeframe_s <= 0 || b.ts < 0 || b.instrument_id <= 0)
    {
        return false;
    }
    if (b.timeframe_s == kTimeframe1m && (b.ts % kTimeframe1m) != 0)
    {
        return false;
    }
    if (!std::isfinite(b.open) || !std::isfinite(b.high) || !std::isfinite(b.low) ||
        !std::isfinite(b.close) || !std::isfinite(b.volume))
    {
        return false;
    }
    if (b.volume < 0.0)
    {
        return false;
    }
    return b.high >= b.low && b.high >= b.open && b.high >= b.close &&
           b.low <= b.open && b.low <= b.close;
}

inline bool isFormingBar(const Bar& b, UnixSeconds now_utc) noexcept
{
    if (b.timeframe_s <= 0)
    {
        return false;
    }
    if (b.timeframe_s == kTimeframe1d)
    {
        return b.ts + kUsRthDurationS > now_utc;
    }
    return b.ts + b.timeframe_s > now_utc;
}

}  // namespace terminal
