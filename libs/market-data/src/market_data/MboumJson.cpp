// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/MboumJson.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdio>
#include <cmath>
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
    const int month = (period[5] - '0') * 10 + (period[6] - '0');
    const int day = (period[8] - '0') * 10 + (period[9] - '0');
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

}  // namespace terminal
