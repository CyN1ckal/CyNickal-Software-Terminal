#include "market_data/MboumJson.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

namespace myapp {
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
        if (const auto meta = root.find("meta"); meta != root.end())
        {
            if (!meta->is_object())
            {
                throw std::runtime_error("expected JSON object");
            }
            if (const auto splits = meta->find("splits"); splits != meta->end())
            {
                page.splits = splitsTruthy(*splits);
            }
        }

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

        if (const auto message = root.find("message"); message != root.end())
        {
            if (!message->is_string())
            {
                throw std::runtime_error("expected JSON string");
            }
            const auto& text = message->get_ref<const std::string&>();
            if (text.find("No historical data") != std::string::npos ||
                text.find("Failed to fetch historical data") != std::string::npos)
            {
                page.no_data = true;
            }
        }
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

}  // namespace myapp
