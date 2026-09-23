// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/MboumJson.h"

#include "market_data/Time.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] bool splitsTruthy(const nlohmann::json& value)
{
    if (value.is_boolean())
    {
        return value.get<bool>();
    }
    if (value.is_number())
    {
        return value.get<double>() != 0.0;
    }
    if (value.is_string())
    {
        const auto& text = value.get_ref<const std::string&>();
        return text == "1" || text == "true" || text == "True";
    }
    throw std::runtime_error("expected JSON boolean");
}

[[nodiscard]] bool messageIsNoData(const nlohmann::json& root)
{
    const auto message = root.find("message");
    if (message == root.end())
    {
        return false;
    }
    if (!message->is_string())
    {
        throw std::runtime_error("expected JSON string");
    }
    const auto& text = message->get_ref<const std::string&>();
    return text.find("No historical data") != std::string::npos ||
           text.find("Failed to fetch historical data") != std::string::npos;
}

[[nodiscard]] bool metaSplits(const nlohmann::json& root)
{
    const auto meta = root.find("meta");
    if (meta == root.end())
    {
        return false;
    }
    if (!meta->is_object())
    {
        throw std::runtime_error("expected JSON object");
    }
    const auto splits = meta->find("splits");
    if (splits == meta->end())
    {
        return false;
    }
    return splitsTruthy(*splits);
}

[[nodiscard]] std::optional<double> jsonDouble(const nlohmann::json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number())
    {
        return std::nullopt;
    }
    return it->get<double>();
}

[[nodiscard]] std::optional<UnixSeconds> jsonUnix(const nlohmann::json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number())
    {
        return std::nullopt;
    }
    if (it->is_number_integer())
    {
        return it->get<UnixSeconds>();
    }
    const auto seconds = static_cast<UnixSeconds>(it->get<double>());
    return seconds;
}

[[nodiscard]] bool trimmedToken(std::string_view text)
{
    if (text.empty())
    {
        return false;
    }
    const auto first = text.find_first_not_of(" \t\r\n");
    const auto last = text.find_last_not_of(" \t\r\n");
    return first == 0 && last == text.size() - 1;
}

[[nodiscard]] bool isFiscalPeriodEnd(std::string_view period)
{
    if (period == "TTM")
    {
        return true;
    }
    if (period.size() != 10 || period[4] != '-' || period[7] != '-')
    {
        return false;
    }
    for (const std::size_t index : {0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U})
    {
        const char ch = period[index];
        if (ch < '0' || ch > '9')
        {
            return false;
        }
    }
    const int month = ((period[5] - '0') * 10) + (period[6] - '0');
    const int day = ((period[8] - '0') * 10) + (period[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

[[nodiscard]] bool messageIsNoStatement(const nlohmann::ordered_json& root)
{
    const auto message = root.find("message");
    if (message == root.end() || !message->is_string())
    {
        return false;
    }
    return message->get_ref<const std::string&>().find("No data returned") != std::string::npos;
}

[[nodiscard]] StatementValue statementValueFromJson(const nlohmann::ordered_json& value)
{
    if (value.is_string())
    {
        const auto& text = value.get_ref<const std::string&>();
        if (text.empty())
        {
            throw std::runtime_error("statement text is empty");
        }
        return text;
    }
    if (value.is_number_unsigned())
    {
        const auto wide = value.get<std::uint64_t>();
        if (wide > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            return static_cast<double>(wide);
        }
        return static_cast<std::int64_t>(wide);
    }
    if (value.is_number_integer())
    {
        return value.get<std::int64_t>();
    }
    if (value.is_number_float())
    {
        const double real = value.get<double>();
        if (!std::isfinite(real))
        {
            throw std::runtime_error("statement real is not finite");
        }
        return real;
    }
    throw std::runtime_error("statement value is not a number or string");
}

[[nodiscard]] std::string_view statementModule(StatementKind statement)
{
    switch (statement)
    {
    case StatementKind::Income:
        return "income-statement-v2";
    case StatementKind::Balance:
        return "balance-sheet-v2";
    case StatementKind::Cashflow:
        return "cashflow-statement-v2";
    }
    throw std::runtime_error("unknown StatementKind");
}

[[nodiscard]] MboumV2Statement parseMboumV2StatementBody(const nlohmann::ordered_json& root)
{
    if (!root.is_object())
    {
        throw std::runtime_error("expected JSON object");
    }
    const auto success = root.find("success");
    if (success != root.end() && success->is_boolean() && !success->get<bool>())
    {
        if (messageIsNoStatement(root))
        {
            MboumV2Statement page;
            page.no_data = true;
            return page;
        }
        throw std::runtime_error("MBoum statement returned no grid");
    }
    if (messageIsNoStatement(root))
    {
        MboumV2Statement page;
        page.no_data = true;
        return page;
    }

    const auto body = root.find("body");
    if (body == root.end() || !body->is_object())
    {
        throw std::runtime_error("MBoum statement body is missing");
    }

    MboumV2Statement page;
    std::set<std::pair<std::string, std::string>> seen;
    for (auto line = body->begin(); line != body->end(); ++line)
    {
        const std::string& line_item = line.key();
        if (!trimmedToken(line_item))
        {
            throw std::runtime_error("statement line_item is empty");
        }
        if (line->is_null())
        {
            continue;
        }
        if (!line->is_object())
        {
            throw std::runtime_error("expected JSON object");
        }
        for (auto period = line->begin(); period != line->end(); ++period)
        {
            if (period->is_null())
            {
                continue;
            }
            const std::string& period_end = period.key();
            if (!isFiscalPeriodEnd(period_end))
            {
                throw std::runtime_error("statement period_end is invalid");
            }
            if (!seen.emplace(line_item, period_end).second)
            {
                throw std::runtime_error("duplicate statement cell");
            }
            MboumV2StatementCell cell;
            cell.line_item = line_item;
            cell.period_end = period_end;
            cell.value = statementValueFromJson(*period);
            page.cells.push_back(std::move(cell));
        }
    }
    return page;
}

}  // namespace

MboumV3Page parseMboumV3Historical(std::string_view json)
{
    try
    {
        const auto root = nlohmann::json::parse(json);
        if (!root.is_object())
        {
            throw std::runtime_error("expected JSON object");
        }

        MboumV3Page page;
        page.splits = metaSplits(root);

        if (const auto body = root.find("body"); body != root.end() && body->is_array())
        {
            page.bars.reserve(body->size());
            for (const auto& item : *body)
            {
                if (!item.is_object())
                {
                    throw std::runtime_error("expected JSON object");
                }
                const auto datetime = item.find("datetime");
                if (datetime == item.end())
                {
                    continue;
                }
                if (!datetime->is_string())
                {
                    throw std::runtime_error("expected JSON string");
                }
                MboumV3BarRow row;
                row.datetime = datetime->get<std::string>();
                row.open = item.value("open", 0.0);
                row.high = item.value("high", 0.0);
                row.low = item.value("low", 0.0);
                row.close = item.value("close", 0.0);
                row.volume = item.value("volume", 0.0);
                page.bars.push_back(std::move(row));
            }
        }

        page.no_data = messageIsNoData(root);
        return page;
    }
    catch (const nlohmann::json::exception& ex)
    {
        throw std::runtime_error(std::string("invalid MBoum JSON: ") + ex.what());
    }
}

std::string mboumV3HistoricalUrl(std::string_view ticker, SessionDate session_date)
{
    const int y = session_date / 10000;
    const int m = (session_date / 100) % 100;
    const int d = session_date % 100;
    char start[32]{};
    char end[32]{};
    std::snprintf(start, sizeof(start), "%04d%02d%02d093000", y, m, d);
    std::snprintf(end, sizeof(end), "%04d%02d%02d160000", y, m, d);
    std::string url = "https://api.mboum.com/v3/markets/historical?ticker=";
    url.append(ticker);
    url += "&interval=1min&limit=400&startDate=";
    url += start;
    url += "&endDate=";
    url += end;
    // Laravel boolean rules accept 0/1, not the strings "false"/"true".
    url += "&splits=0&dividends=0&order=asc";
    return url;
}

MboumV3DailyPage parseMboumV3Daily(std::string_view json)
{
    try
    {
        const auto root = nlohmann::json::parse(json);
        if (!root.is_object())
        {
            throw std::runtime_error("expected JSON object");
        }

        MboumV3DailyPage page;
        page.splits = metaSplits(root);

        if (const auto body = root.find("body"); body != root.end() && body->is_array())
        {
            page.bars.reserve(body->size());
            for (const auto& item : *body)
            {
                if (!item.is_object())
                {
                    throw std::runtime_error("expected JSON object");
                }
                const auto date = item.find("date");
                if (date == item.end())
                {
                    continue;
                }
                if (!date->is_string())
                {
                    throw std::runtime_error("expected JSON string");
                }
                MboumV3DailyRow row;
                row.date = date->get<std::string>();
                row.open = item.value("open", 0.0);
                row.high = item.value("high", 0.0);
                row.low = item.value("low", 0.0);
                row.close = item.value("close", 0.0);
                row.volume = item.value("volume", 0.0);
                page.bars.push_back(std::move(row));
            }
        }

        page.no_data = messageIsNoData(root);
        return page;
    }
    catch (const nlohmann::json::exception& ex)
    {
        throw std::runtime_error(std::string("invalid MBoum JSON: ") + ex.what());
    }
}

std::string mboumV3DailyUrl(std::string_view ticker, SessionDate from, SessionDate to, int limit)
{
    const int fy = from / 10000;
    const int fm = (from / 100) % 100;
    const int fd = from % 100;
    const int ty = to / 10000;
    const int tm = (to / 100) % 100;
    const int td = to % 100;
    char start[16]{};
    char end[16]{};
    std::snprintf(start, sizeof(start), "%04d%02d%02d", fy, fm, fd);
    std::snprintf(end, sizeof(end), "%04d%02d%02d", ty, tm, td);
    std::string url = "https://api.mboum.com/v3/markets/historical?ticker=";
    url.append(ticker);
    url += "&interval=daily&limit=";
    url += std::to_string(limit);
    url += "&startDate=";
    url += start;
    url += "&endDate=";
    url += end;
    url += "&splits=0&dividends=0&order=asc";
    return url;
}

std::vector<MboumV1SplitEvent> parseMboumV1SplitEvents(std::string_view json)
{
    try
    {
        const auto root = nlohmann::json::parse(json);
        if (!root.is_object())
        {
            throw std::runtime_error("expected JSON object");
        }
        const auto body = root.find("body");
        if (body == root.end() || body->is_null())
        {
            return {};
        }
        if (!body->is_object())
        {
            throw std::runtime_error("expected JSON object");
        }
        const auto events = body->find("events");
        if (events == body->end() || events->is_null())
        {
            return {};
        }
        if (!events->is_object())
        {
            throw std::runtime_error("expected JSON object");
        }
        const auto splits = events->find("splits");
        if (splits == events->end() || splits->is_null())
        {
            return {};
        }
        if (!splits->is_object())
        {
            throw std::runtime_error("expected JSON object");
        }

        std::vector<MboumV1SplitEvent> out;
        for (const auto& item : splits->items())
        {
            const auto& row = item.value();
            if (!row.is_object())
            {
                continue;
            }
            const std::optional<UnixSeconds> ex_ts = jsonUnix(row, "date");
            const std::optional<double> numerator = jsonDouble(row, "numerator");
            const std::optional<double> denominator = jsonDouble(row, "denominator");
            if (!ex_ts.has_value() || !numerator.has_value() || !denominator.has_value())
            {
                continue;
            }
            const double den = *denominator;
            const double num = *numerator;
            if (!(den > 0.0) || !(num / den > 0.0))
            {
                continue;
            }
            MboumV1SplitEvent event;
            event.ex_ts = *ex_ts;
            event.split_ratio = num / den;
            out.push_back(event);
        }
        return out;
    }
    catch (const nlohmann::json::exception& ex)
    {
        throw std::runtime_error(std::string("invalid MBoum JSON: ") + ex.what());
    }
}

std::string mboumV1SplitsUrl(std::string_view ticker)
{
    std::string url = "https://api.mboum.com/v1/markets/stock/history?ticker=";
    url.append(ticker);
    url += "&interval=1mo&diffandsplits=true";
    return url;
}

MboumV2Statement parseMboumV2Statement(std::string_view json)
{
    try
    {
        const auto root = nlohmann::ordered_json::parse(json);
        return parseMboumV2StatementBody(root);
    }
    catch (const nlohmann::json::exception& ex)
    {
        throw std::runtime_error(std::string("invalid MBoum JSON: ") + ex.what());
    }
}

std::string mboumV2StatementUrl(std::string_view ticker,
                                StatementKind statement,
                                StatementTimeframe timeframe)
{
    std::string url = "https://api.mboum.com/v1/markets/stock/modules?ticker=";
    url.append(ticker);
    url += "&module=";
    url.append(statementModule(statement));
    url += "&timeframe=";
    url.append(toSql(timeframe));
    return url;
}

namespace {

[[nodiscard]] const std::string& optionString(const nlohmann::json& object, const char* key)
{
    const auto found = object.find(key);
    if (found == object.end() || !found->is_string())
    {
        throw std::runtime_error(std::string("option contract ") + key + " is missing");
    }
    return found->get_ref<const std::string&>();
}

[[nodiscard]] std::optional<double> parseOptionNumber(std::string_view text)
{
    if (text == "unch")
    {
        return 0.0;
    }
    // from_chars rejects a leading plus. Display strings use one for gains.
    if (!text.empty() && text.front() == '+')
    {
        text.remove_prefix(1);
    }
    const auto amount = parseMoneyAmount(text);
    if (!amount.has_value() || !std::isfinite(*amount))
    {
        return std::nullopt;
    }
    return *amount;
}

[[nodiscard]] std::optional<double> parseOptionPercent(std::string_view text)
{
    if (text == "unch")
    {
        return 0.0;
    }
    std::string_view body = text;
    if (!body.empty() && body.back() == '%')
    {
        body.remove_suffix(1);
    }
    const auto amount = parseOptionNumber(body);
    if (!amount.has_value())
    {
        return std::nullopt;
    }
    return *amount / 100.0;
}

[[nodiscard]] std::optional<std::int64_t> parseOptionWhole(std::string_view text)
{
    const auto amount = parseOptionNumber(text);
    if (!amount.has_value())
    {
        return std::nullopt;
    }
    const double truncated = std::trunc(*amount);
    if (truncated != *amount || truncated > static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
        truncated < static_cast<double>(std::numeric_limits<std::int64_t>::min()))
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(truncated);
}

[[nodiscard]] double requireOptionNumber(std::string_view text, const char* key)
{
    const auto value = parseOptionNumber(text);
    if (!value.has_value())
    {
        throw std::runtime_error(std::string("option contract ") + key + " is not a number");
    }
    return *value;
}

[[nodiscard]] double requireOptionPercent(std::string_view text, const char* key)
{
    const auto value = parseOptionPercent(text);
    if (!value.has_value())
    {
        throw std::runtime_error(std::string("option contract ") + key + " is not a percent");
    }
    return *value;
}

[[nodiscard]] std::int64_t requireOptionWhole(std::string_view text, const char* key)
{
    const auto value = parseOptionWhole(text);
    if (!value.has_value())
    {
        throw std::runtime_error(std::string("option contract ") + key + " is not a whole number");
    }
    return *value;
}

[[nodiscard]] std::optional<SessionDate> usTextToSessionDate(std::string_view text)
{
    const auto utc = parseUsDateToUtcMidnight(text);
    if (!utc.has_value())
    {
        return std::nullopt;
    }
    std::tm parts{};
    if (!tryUtcTm(static_cast<std::time_t>(*utc), parts))
    {
        return std::nullopt;
    }
    return ((parts.tm_year + 1900) * 10000) + ((parts.tm_mon + 1) * 100) + parts.tm_mday;
}

[[nodiscard]] std::optional<SessionDate> optionalUsDate(std::string_view text, const char* key)
{
    if (text == "N/A" || text == "--" || text.empty())
    {
        return std::nullopt;
    }
    const auto date = usTextToSessionDate(text);
    if (!date.has_value())
    {
        throw std::runtime_error(std::string("option contract ") + key + " is not a date");
    }
    return date;
}

[[nodiscard]] std::optional<std::string> optionalToken(std::string_view text)
{
    if (text == "N/A" || text == "--" || text.empty())
    {
        return std::nullopt;
    }
    return std::string(text);
}

struct VendorSymbol
{
    std::string base;
    SessionDate expiration{};
    double strike{};
    OptionRight right{OptionRight::Call};
};

[[nodiscard]] std::optional<VendorSymbol> parseVendorSymbol(std::string_view symbol)
{
    const auto first = symbol.find('|');
    const auto second = first == std::string_view::npos ? std::string_view::npos : symbol.find('|', first + 1);
    if (first == std::string_view::npos || first == 0 || second == std::string_view::npos ||
        symbol.find('|', second + 1) != std::string_view::npos)
    {
        return std::nullopt;
    }
    const auto date_text = symbol.substr(first + 1, second - first - 1);
    if (date_text.size() != 8)
    {
        return std::nullopt;
    }
    std::string iso;
    iso.reserve(10);
    iso.append(date_text.substr(0, 4));
    iso.push_back('-');
    iso.append(date_text.substr(4, 2));
    iso.push_back('-');
    iso.append(date_text.substr(6, 2));
    const auto expiration = tryParseIsoDate(iso);
    if (!expiration.has_value())
    {
        return std::nullopt;
    }
    std::string_view rest = symbol.substr(second + 1);
    if (rest.size() < 2)
    {
        return std::nullopt;
    }
    const char side = rest.back();
    if (side != 'C' && side != 'P')
    {
        return std::nullopt;
    }
    rest.remove_suffix(1);
    if (!rest.empty() && rest.back() == 'W')
    {
        rest.remove_suffix(1);
    }
    const auto strike = parseOptionNumber(rest);
    if (!strike.has_value() || *strike <= 0.0)
    {
        return std::nullopt;
    }
    VendorSymbol parsed;
    parsed.base = std::string(symbol.substr(0, first));
    parsed.expiration = *expiration;
    parsed.strike = *strike;
    parsed.right = side == 'C' ? OptionRight::Call : OptionRight::Put;
    return parsed;
}

[[nodiscard]] std::optional<int> parseTradeClock(std::string_view text)
{
    if (text.size() < 6 || !text.ends_with(" ET"))
    {
        return std::nullopt;
    }
    const auto clock = text.substr(0, text.size() - 3);
    const auto colon = clock.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 >= clock.size())
    {
        return std::nullopt;
    }
    int hour = 0;
    int minute = 0;
    const auto hour_text = clock.substr(0, colon);
    const auto minute_text = clock.substr(colon + 1);
    const auto hour_parsed = std::from_chars(hour_text.data(), hour_text.data() + hour_text.size(), hour);
    const auto minute_parsed = std::from_chars(minute_text.data(), minute_text.data() + minute_text.size(), minute);
    if (hour_parsed.ec != std::errc{} || hour_parsed.ptr != hour_text.data() + hour_text.size() ||
        minute_parsed.ec != std::errc{} || minute_parsed.ptr != minute_text.data() + minute_text.size())
    {
        return std::nullopt;
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
    {
        return std::nullopt;
    }
    return (hour * 60) + minute;
}

struct ParsedOption
{
    MboumV3OptionContract contract;
    std::string base_symbol;
    std::optional<double> average_iv;
    std::optional<double> historic_vol_30d;
    std::optional<double> iv_rank_1y;
    std::optional<SessionDate> next_earnings;
    std::optional<SessionDate> dividend_ex;
    std::optional<std::string> earnings_time;
};

[[nodiscard]] ParsedOption parseOptionContract(const nlohmann::json& item)
{
    if (!item.is_object())
    {
        throw std::runtime_error("option contract is not an object");
    }
    const std::string& symbol = optionString(item, "symbol");
    const auto vendor = parseVendorSymbol(symbol);
    if (!vendor.has_value())
    {
        throw std::runtime_error("option contract symbol is invalid");
    }
    const std::string& base = optionString(item, "baseSymbol");
    if (base != vendor->base)
    {
        throw std::runtime_error("option contract baseSymbol does not match the symbol");
    }
    const auto display_expiration = usTextToSessionDate(optionString(item, "expirationDate"));
    if (!display_expiration.has_value() || *display_expiration != vendor->expiration)
    {
        throw std::runtime_error("option contract expirationDate does not match the symbol");
    }
    const double display_strike = requireOptionNumber(optionString(item, "strikePrice"), "strikePrice");
    if (std::fabs(display_strike - vendor->strike) > 0.0001)
    {
        throw std::runtime_error("option contract strikePrice does not match the symbol");
    }
    const std::string& option_type = optionString(item, "optionType");
    OptionRight right = OptionRight::Call;
    if (option_type == "Put")
    {
        right = OptionRight::Put;
    }
    else if (option_type != "Call")
    {
        throw std::runtime_error("option contract optionType does not match the symbol");
    }
    if (right != vendor->right)
    {
        throw std::runtime_error("option contract optionType does not match the symbol");
    }
    const std::string& expiration_type = optionString(item, "expirationType");
    if (expiration_type != "weekly" && expiration_type != "monthly")
    {
        throw std::runtime_error("option contract expirationType is unknown");
    }

    const std::string& trade_time = optionString(item, "tradeTime");
    std::optional<SessionDate> trade_date;
    std::optional<int> trade_minute;
    if (trade_time != "N/A")
    {
        if (const auto clock = parseTradeClock(trade_time))
        {
            trade_minute = clock;
        }
        else if (const auto date = usTextToSessionDate(trade_time))
        {
            trade_date = date;
        }
        else
        {
            throw std::runtime_error("option contract tradeTime is invalid");
        }
    }

    const std::int64_t days = requireOptionWhole(optionString(item, "daysToExpiration"), "daysToExpiration");
    if (days > std::numeric_limits<int>::max())
    {
        throw std::runtime_error("option contract daysToExpiration is too large");
    }

    ParsedOption parsed;
    parsed.base_symbol = base;
    parsed.average_iv = requireOptionPercent(optionString(item, "averageVolatility"), "averageVolatility");
    parsed.historic_vol_30d =
        requireOptionPercent(optionString(item, "historicVolatility30d"), "historicVolatility30d");
    parsed.iv_rank_1y = requireOptionPercent(optionString(item, "impliedVolatilityRank1y"), "impliedVolatilityRank1y");
    parsed.next_earnings = optionalUsDate(optionString(item, "baseNextEarningsDate"), "baseNextEarningsDate");
    parsed.dividend_ex = optionalUsDate(optionString(item, "dividendExDate"), "dividendExDate");
    parsed.earnings_time = optionalToken(optionString(item, "baseTimeCode"));
    parsed.contract.vendor_symbol = symbol;
    parsed.contract.expiration = vendor->expiration;
    parsed.contract.expiration_type = optionExpirationTypeFromSql(expiration_type);
    parsed.contract.strike = display_strike;
    parsed.contract.right = right;
    parsed.contract.bid = requireOptionNumber(optionString(item, "bidPrice"), "bidPrice");
    parsed.contract.ask = requireOptionNumber(optionString(item, "askPrice"), "askPrice");
    parsed.contract.mid = requireOptionNumber(optionString(item, "midpoint"), "midpoint");
    parsed.contract.last = requireOptionNumber(optionString(item, "lastPrice"), "lastPrice");
    parsed.contract.price_change = requireOptionNumber(optionString(item, "priceChange"), "priceChange");
    parsed.contract.percent_change = requireOptionPercent(optionString(item, "percentChange"), "percentChange");
    parsed.contract.volume = requireOptionWhole(optionString(item, "volume"), "volume");
    parsed.contract.open_interest = requireOptionWhole(optionString(item, "openInterest"), "openInterest");
    parsed.contract.open_interest_change =
        requireOptionWhole(optionString(item, "openInterestChange"), "openInterestChange");
    parsed.contract.implied_vol = requireOptionPercent(optionString(item, "volatility"), "volatility");
    parsed.contract.delta = requireOptionNumber(optionString(item, "delta"), "delta");
    parsed.contract.rho = requireOptionNumber(optionString(item, "rho"), "rho");
    parsed.contract.vega = requireOptionNumber(optionString(item, "vega"), "vega");
    parsed.contract.theta = requireOptionNumber(optionString(item, "theta"), "theta");
    parsed.contract.moneyness = requireOptionPercent(optionString(item, "moneyness"), "moneyness");
    parsed.contract.days_to_expiration = static_cast<int>(days);
    parsed.contract.trade_date = trade_date;
    parsed.contract.trade_minute = trade_minute;
    if (parsed.contract.bid < 0.0 || parsed.contract.ask < 0.0 || parsed.contract.mid < 0.0 ||
        parsed.contract.last < 0.0 || parsed.contract.volume < 0 || parsed.contract.open_interest < 0 ||
        parsed.contract.implied_vol < 0.0 || parsed.contract.days_to_expiration < 0 ||
        (parsed.historic_vol_30d.has_value() && *parsed.historic_vol_30d < 0.0) ||
        (parsed.iv_rank_1y.has_value() && *parsed.iv_rank_1y < 0.0) ||
        (parsed.average_iv.has_value() && *parsed.average_iv < 0.0))
    {
        throw std::runtime_error("option contract has a negative quote");
    }
    return parsed;
}

void appendCalendar(const nlohmann::json& list,
                    OptionExpirationType type,
                    std::vector<MboumV3OptionExpiry>& calendar)
{
    if (!list.is_array())
    {
        throw std::runtime_error("option expirations are not an array");
    }
    for (const auto& item : list)
    {
        if (!item.is_string())
        {
            throw std::runtime_error("option expiration is not a date");
        }
        const auto date = tryParseIsoDate(item.get_ref<const std::string&>());
        if (!date.has_value())
        {
            throw std::runtime_error("option expiration is not a date");
        }
        const bool duplicate = std::ranges::any_of(calendar, [&](const MboumV3OptionExpiry& row) {
            return row.expiration == *date && row.expiration_type == type;
        });
        if (duplicate)
        {
            throw std::runtime_error("option expiration is repeated");
        }
        MboumV3OptionExpiry row;
        row.expiration = *date;
        row.expiration_type = type;
        calendar.push_back(row);
    }
}

[[nodiscard]] MboumV3OptionGroup& groupFor(std::vector<MboumV3OptionGroup>& groups,
                                           SessionDate expiration,
                                           OptionExpirationType type)
{
    for (MboumV3OptionGroup& group : groups)
    {
        if (group.expiration == expiration && group.expiration_type == type)
        {
            return group;
        }
    }
    MboumV3OptionGroup created;
    created.expiration = expiration;
    created.expiration_type = type;
    groups.push_back(std::move(created));
    return groups.back();
}

void appendEncodedQuery(std::string& url, std::string_view value)
{
    constexpr char hex[] = "0123456789ABCDEF";
    for (const char raw : value)
    {
        const auto ch = static_cast<unsigned char>(raw);
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~')
        {
            url.push_back(static_cast<char>(ch));
            continue;
        }
        url.push_back('%');
        url.push_back(hex[ch >> 4]);
        url.push_back(hex[ch & 0x0F]);
    }
}

}  // namespace

MboumV3Options parseMboumV3Options(std::string_view json)
{
    try
    {
        const auto root = nlohmann::json::parse(json);
        if (!root.is_object())
        {
            throw std::runtime_error("expected JSON object");
        }
        MboumV3Options page;
        if (const auto meta = root.find("meta"); meta != root.end() && meta->is_object())
        {
            if (const auto expirations = meta->find("expirations"); expirations != meta->end())
            {
                if (expirations->is_object())
                {
                    page.has_calendar = true;
                    const auto weekly = expirations->find("weekly");
                    const auto monthly = expirations->find("monthly");
                    if (weekly != expirations->end())
                    {
                        appendCalendar(*weekly, OptionExpirationType::Weekly, page.calendar);
                    }
                    if (monthly != expirations->end())
                    {
                        appendCalendar(*monthly, OptionExpirationType::Monthly, page.calendar);
                    }
                }
                else if (expirations->is_array())
                {
                    if (!expirations->empty())
                    {
                        throw std::runtime_error("option expirations are not a calendar");
                    }
                }
                else if (!expirations->is_null())
                {
                    throw std::runtime_error("option expirations are not a calendar");
                }
            }
        }

        std::vector<ParsedOption> parsed;
        if (const auto body = root.find("body"); body != root.end() && !body->is_null())
        {
            if (body->is_array())
            {
                if (!body->empty())
                {
                    throw std::runtime_error("option body is not a chain");
                }
            }
            else if (body->is_object())
            {
                constexpr const char* kSides[] = {"Call", "Put"};
                for (const char* side : kSides)
                {
                    const auto list = body->find(side);
                    if (list == body->end() || list->is_null())
                    {
                        continue;
                    }
                    if (!list->is_array())
                    {
                        throw std::runtime_error("option body is not a chain");
                    }
                    for (const auto& item : *list)
                    {
                        parsed.push_back(parseOptionContract(item));
                    }
                }
            }
            else
            {
                throw std::runtime_error("option body is not a chain");
            }
        }

        if (!parsed.empty())
        {
            const ParsedOption& first = parsed.front();
            page.base_symbol = first.base_symbol;
            page.historic_vol_30d = first.historic_vol_30d;
            page.iv_rank_1y = first.iv_rank_1y;
            page.next_earnings = first.next_earnings;
            page.dividend_ex = first.dividend_ex;
            page.earnings_time = first.earnings_time;
            std::set<std::string> symbols;
            for (const ParsedOption& row : parsed)
            {
                if (row.base_symbol != page.base_symbol || row.historic_vol_30d != page.historic_vol_30d ||
                    row.iv_rank_1y != page.iv_rank_1y || row.next_earnings != page.next_earnings ||
                    row.dividend_ex != page.dividend_ex || row.earnings_time != page.earnings_time)
                {
                    throw std::runtime_error("option chain underlying fields disagree");
                }
                if (!symbols.insert(row.contract.vendor_symbol).second)
                {
                    throw std::runtime_error("option contract symbol is repeated");
                }
                MboumV3OptionGroup& group =
                    groupFor(page.groups, row.contract.expiration, row.contract.expiration_type);
                if (!group.average_iv.has_value())
                {
                    group.average_iv = row.average_iv;
                }
                else if (group.average_iv != row.average_iv)
                {
                    throw std::runtime_error("option chain average volatility disagrees");
                }
                group.contracts.push_back(row.contract);
            }
        }
        page.no_data = !page.has_calendar && page.groups.empty();
        return page;
    }
    catch (const nlohmann::json::exception& ex)
    {
        throw std::runtime_error(std::string("invalid MBoum JSON: ") + ex.what());
    }
}

std::string mboumV3OptionsUrl(std::string_view ticker, SessionDate expiration)
{
    std::string url = "https://api.mboum.com/v3/markets/options?ticker=";
    appendEncodedQuery(url, ticker);
    if (expiration != 0)
    {
        url += "&expiration=";
        url += formatSessionDate(expiration);
    }
    return url;
}

}  // namespace terminal
