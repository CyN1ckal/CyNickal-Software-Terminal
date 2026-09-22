// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/MboumJson.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdio>
#include <optional>
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

}  // namespace terminal
