// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/MboumMap.h"

#include <string>
#include <string_view>
#include <vector>

namespace terminal {

struct MboumV3Page
{
    bool splits{false};
    bool no_data{false};
    std::vector<MboumV3BarRow> bars;
};

struct MboumV3DailyPage
{
    bool splits{false};
    bool no_data{false};
    std::vector<MboumV3DailyRow> bars;
};

[[nodiscard]] MboumV3Page parseMboumV3Historical(std::string_view json);
[[nodiscard]] MboumV3DailyPage parseMboumV3Daily(std::string_view json);

[[nodiscard]] std::string mboumV3HistoricalUrl(std::string_view ticker, SessionDate session_date);
[[nodiscard]] std::string mboumV3DailyUrl(std::string_view ticker,
                                          SessionDate from,
                                          SessionDate to,
                                          int limit = kMboumDailyPageLimit);

// One split from GET /v1/markets/stock/history?interval=1mo&diffandsplits=true.
// 1mo is the documented 10-year window. 1d is 5 years, short of a 2520-session chart.
// Price bars in that payload are already adjusted and are not represented here.
struct MboumV1SplitEvent
{
    UnixSeconds ex_ts{};
    double split_ratio{};
};

[[nodiscard]] std::vector<MboumV1SplitEvent> parseMboumV1SplitEvents(std::string_view json);
[[nodiscard]] std::string mboumV1SplitsUrl(std::string_view ticker);

// One v2 modules statement. body line items keep vendor key order.
// A missing period key is omitted. TTM is a period token, not a date.
struct MboumV2StatementCell
{
    std::string line_item;
    std::string period_end;
    StatementValue value;
};

struct MboumV2Statement
{
    bool no_data{false};
    std::vector<MboumV2StatementCell> cells;
};

[[nodiscard]] MboumV2Statement parseMboumV2Statement(std::string_view json);
[[nodiscard]] std::string mboumV2StatementUrl(std::string_view ticker,
                                              StatementKind statement,
                                              StatementTimeframe timeframe);

// One contract from GET /v3/markets/options. Percents are fractions.
// trade_date and trade_minute are mutually exclusive.
struct MboumV3OptionContract
{
    std::string vendor_symbol;
    SessionDate expiration{};
    OptionExpirationType expiration_type{OptionExpirationType::Weekly};
    double strike{};
    OptionRight right{OptionRight::Call};
    double bid{};
    double ask{};
    double mid{};
    double last{};
    double price_change{};
    double percent_change{};
    std::int64_t volume{};
    std::int64_t open_interest{};
    std::int64_t open_interest_change{};
    double implied_vol{};
    double delta{};
    double rho{};
    double vega{};
    double theta{};
    double moneyness{};
    int days_to_expiration{};
    std::optional<SessionDate> trade_date;
    std::optional<int> trade_minute;
};

// Contracts that share an expiration date and type. average_iv is that slice's ATM vol.
struct MboumV3OptionGroup
{
    SessionDate expiration{};
    OptionExpirationType expiration_type{OptionExpirationType::Weekly};
    std::optional<double> average_iv;
    std::vector<MboumV3OptionContract> contracts;
};

struct MboumV3OptionExpiry
{
    SessionDate expiration{};
    OptionExpirationType expiration_type{OptionExpirationType::Weekly};
};

// has_calendar is false when meta.expirations is an empty array (unknown ticker).
// no_data is that case with an empty body. A known underlying with an unlisted
// date has a calendar and no groups.
struct MboumV3Options
{
    bool no_data{false};
    bool has_calendar{false};
    std::vector<MboumV3OptionExpiry> calendar;
    std::string base_symbol;
    std::optional<double> historic_vol_30d;
    std::optional<double> iv_rank_1y;
    std::optional<SessionDate> next_earnings;
    std::optional<SessionDate> dividend_ex;
    std::optional<std::string> earnings_time;
    std::vector<MboumV3OptionGroup> groups;
};

[[nodiscard]] MboumV3Options parseMboumV3Options(std::string_view json);

// expiration 0 omits the parameter. The ticker is percent-encoded ($SPX).
[[nodiscard]] std::string mboumV3OptionsUrl(std::string_view ticker, SessionDate expiration = 0);

}  // namespace terminal
