// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/OpenFigi.h"

#include "market_data/Figi.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

namespace terminal {
namespace {

constexpr std::string_view kIndexSuffix = " Index";

[[nodiscard]] std::string upper(std::string_view text)
{
    std::string out(text);
    for (char& ch : out)
    {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return out;
}

[[nodiscard]] bool endsWithNoCase(std::string_view text, std::string_view suffix) noexcept
{
    return text.size() >= suffix.size() &&
           sameTicker(text.substr(text.size() - suffix.size()), suffix);
}

[[nodiscard]] std::string textOrEmpty(const nlohmann::json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return {};
    }
    return it->get<std::string>();
}

[[nodiscard]] std::optional<std::string> textOrNull(const nlohmann::json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return std::nullopt;
    }
    return it->get<std::string>();
}

[[nodiscard]] std::optional<int> parseInt(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
    {
        text.remove_prefix(1);
    }
    int value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end == text.data())
    {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] OpenFigiJobResult unreachable(std::string message)
{
    OpenFigiJobResult out;
    out.answer = OpenFigiAnswer::Unreachable;
    out.message = std::move(message);
    return out;
}

}  // namespace

std::string buildOpenFigiMappingBody(std::span<const OpenFigiJob> jobs)
{
    nlohmann::json array = nlohmann::json::array();
    for (const OpenFigiJob& job : jobs)
    {
        nlohmann::json object;
        object["idType"] = job.id_type;
        object["idValue"] = job.id_value;
        if (job.exch_code.has_value())
        {
            object["exchCode"] = *job.exch_code;
        }
        if (job.market_sec_des.has_value())
        {
            object["marketSecDes"] = *job.market_sec_des;
        }
        array.push_back(std::move(object));
    }
    return array.dump();
}

std::optional<std::vector<OpenFigiJobResult>> parseOpenFigiMappingResponse(std::string_view body,
                                                                           std::size_t job_count)
{
    const nlohmann::json root = nlohmann::json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_array() || root.size() != job_count)
    {
        return std::nullopt;
    }
    std::vector<OpenFigiJobResult> out;
    out.reserve(job_count);
    for (const nlohmann::json& element : root)
    {
        if (!element.is_object())
        {
            out.push_back(unreachable("OpenFIGI element is not an object"));
            continue;
        }
        if (const auto data = element.find("data"); data != element.end() && data->is_array())
        {
            OpenFigiJobResult result;
            result.answer = data->empty() ? OpenFigiAnswer::NoMatch : OpenFigiAnswer::Hits;
            for (const nlohmann::json& row : *data)
            {
                if (!row.is_object())
                {
                    continue;
                }
                OpenFigiHit hit;
                hit.figi = textOrEmpty(row, "figi");
                hit.composite_figi = textOrNull(row, "compositeFIGI");
                hit.ticker = textOrEmpty(row, "ticker");
                hit.name = textOrEmpty(row, "name");
                hit.exch_code = textOrNull(row, "exchCode");
                hit.market_sector = textOrEmpty(row, "marketSector");
                hit.security_type = textOrEmpty(row, "securityType");
                result.hits.push_back(std::move(hit));
            }
            out.push_back(std::move(result));
            continue;
        }
        if (const auto warning = element.find("warning"); warning != element.end())
        {
            OpenFigiJobResult result;
            result.answer = OpenFigiAnswer::NoMatch;
            result.message = warning->is_string() ? warning->get<std::string>() : std::string{};
            out.push_back(std::move(result));
            continue;
        }
        if (const auto error = element.find("error"); error != element.end())
        {
            out.push_back(unreachable(error->is_string() ? error->get<std::string>() : "OpenFIGI error"));
            continue;
        }
        out.push_back(unreachable("OpenFIGI element has no data, warning, or error"));
    }
    return out;
}

OpenFigiRateLimit parseOpenFigiRateLimit(const HttpResponse& response)
{
    OpenFigiRateLimit out;
    if (const auto policy = findHttpHeader(response, "ratelimit-policy"))
    {
        const std::string_view text = *policy;
        out.limit = parseInt(text.substr(0, text.find(';')));
        if (const auto w = text.find("w="); w != std::string_view::npos)
        {
            out.window_s = parseInt(text.substr(w + 2));
        }
    }
    if (const auto limit = findHttpHeader(response, "ratelimit-limit"))
    {
        out.limit = parseInt(*limit);
    }
    if (const auto remaining = findHttpHeader(response, "ratelimit-remaining"))
    {
        out.remaining = parseInt(*remaining);
    }
    if (const auto reset = findHttpHeader(response, "ratelimit-reset"))
    {
        out.reset_s = parseInt(*reset);
    }
    return out;
}

bool isIndexSymbol(std::string_view symbol) noexcept
{
    return !symbol.empty() && symbol.front() == '$';
}

std::string toOpenFigiTicker(std::string_view symbol)
{
    if (isIndexSymbol(symbol))
    {
        return upper(symbol.substr(1)) + std::string(kIndexSuffix);
    }
    std::string out = upper(symbol);
    std::ranges::replace(out, '.', '/');
    return out;
}

std::string fromOpenFigiTicker(std::string_view ticker)
{
    if (endsWithNoCase(ticker, kIndexSuffix))
    {
        return "$" + upper(ticker.substr(0, ticker.size() - kIndexSuffix.size()));
    }
    std::string out = upper(ticker);
    std::ranges::replace(out, '/', '.');
    return out;
}

bool sameTicker(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::toupper(static_cast<unsigned char>(a[i])) != std::toupper(static_cast<unsigned char>(b[i])))
        {
            return false;
        }
    }
    return true;
}

OpenFigiJob forwardJob(std::string_view symbol)
{
    OpenFigiJob job;
    job.id_type = "TICKER";
    job.id_value = toOpenFigiTicker(symbol);
    if (isIndexSymbol(symbol))
    {
        job.market_sec_des = "Index";
    }
    else
    {
        job.exch_code = "US";
        job.market_sec_des = "Equity";
    }
    return job;
}

OpenFigiJob reverseJob(std::string_view figi)
{
    OpenFigiJob job;
    job.id_type = "ID_BB_GLOBAL";
    job.id_value = std::string(figi);
    return job;
}

std::optional<AssetClass> classifyOpenFigiHit(const OpenFigiHit& hit)
{
    if (hit.market_sector == "Index")
    {
        return AssetClass::Index;
    }
    if (hit.market_sector == "Equity")
    {
        return hit.security_type == "ETP" ? AssetClass::Etf : AssetClass::Equity;
    }
    return std::nullopt;
}

ForwardOutcome reduceForward(const OpenFigiJobResult& result, bool index)
{
    ForwardOutcome out;
    out.message = result.message;
    if (result.answer == OpenFigiAnswer::Unreachable)
    {
        out.kind = ForwardKind::Unreachable;
        return out;
    }
    std::vector<std::string> keys;
    for (const OpenFigiHit& hit : result.hits)
    {
        std::optional<std::string> key = hit.composite_figi;
        if (!key.has_value() && !hit.figi.empty())
        {
            key = hit.figi;  // indexes have no composite
        }
        if (key.has_value() && std::ranges::find(keys, *key) == keys.end())
        {
            keys.push_back(*key);
        }
    }
    if (keys.empty())
    {
        // An empty data array or the warning is an answer. Hits without an identifier are not.
        out.kind = result.answer == OpenFigiAnswer::NoMatch ? ForwardKind::NoMatch : ForwardKind::Unreachable;
        if (out.kind == ForwardKind::Unreachable)
        {
            out.message = "OpenFIGI returned hits without a FIGI";
        }
        return out;
    }
    if (keys.size() > 1)
    {
        out.kind = ForwardKind::Ambiguous;
        out.message = std::to_string(keys.size()) + " securities";
        return out;
    }
    const std::string& figi = keys.front();
    const OpenFigiHit* chosen = nullptr;
    for (const OpenFigiHit& hit : result.hits)
    {
        const std::string key = hit.composite_figi.value_or(hit.figi);
        if (key != figi)
        {
            continue;
        }
        if (chosen == nullptr || hit.figi == figi)
        {
            chosen = &hit;
        }
    }
    if (!isValidFigi(figi))
    {
        out.kind = ForwardKind::Unreachable;
        out.message = "OpenFIGI returned an invalid FIGI " + figi;
        return out;
    }
    const auto asset_class = classifyOpenFigiHit(*chosen);
    const bool fits = asset_class.has_value() &&
                      (index ? *asset_class == AssetClass::Index : *asset_class != AssetClass::Index);
    if (!fits)
    {
        out.kind = ForwardKind::Unsupported;
        out.message = chosen->market_sector + " / " + chosen->security_type;
        return out;
    }
    out.kind = ForwardKind::Confirmed;
    out.figi = figi;
    out.asset_class = *asset_class;
    out.name = chosen->name;
    return out;
}

ReverseOutcome reduceReverse(const OpenFigiJobResult& result)
{
    ReverseOutcome out;
    out.message = result.message;
    if (result.answer == OpenFigiAnswer::Unreachable)
    {
        out.kind = ReverseKind::Unreachable;
        return out;
    }
    std::vector<std::string> tickers;
    for (const OpenFigiHit& hit : result.hits)
    {
        if (hit.ticker.empty())
        {
            continue;
        }
        std::string ticker = fromOpenFigiTicker(hit.ticker);
        if (std::ranges::none_of(tickers, [&](const std::string& seen) { return sameTicker(seen, ticker); }))
        {
            tickers.push_back(std::move(ticker));
        }
    }
    if (tickers.empty())
    {
        out.kind = result.answer == OpenFigiAnswer::NoMatch ? ReverseKind::NoMatch : ReverseKind::Unreachable;
        if (out.kind == ReverseKind::Unreachable)
        {
            out.message = "OpenFIGI returned hits without a ticker";
        }
        return out;
    }
    if (tickers.size() > 1)
    {
        out.kind = ReverseKind::Ambiguous;
        out.message = std::to_string(tickers.size()) + " tickers";
        return out;
    }
    out.kind = ReverseKind::Ticker;
    out.ticker = tickers.front();
    return out;
}

OpenFigiClient::OpenFigiClient(HttpPost post, bool has_key, SleepFn sleep)
    : post_(std::move(post)), has_key_(has_key), sleep_(std::move(sleep))
{
    if (!sleep_)
    {
        sleep_ = [](std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); };
    }
}

std::size_t OpenFigiClient::batchSize() const noexcept
{
    return has_key_ ? kOpenFigiJobsWithKey : kOpenFigiJobsWithoutKey;
}

void OpenFigiClient::sleepSeconds(int seconds)
{
    if (seconds > 0)
    {
        sleep_(std::chrono::milliseconds(static_cast<std::int64_t>(seconds) * 1000));
    }
}

std::vector<OpenFigiJobResult> OpenFigiClient::map(std::span<const OpenFigiJob> jobs)
{
    std::vector<OpenFigiJobResult> out;
    out.reserve(jobs.size());
    const int window_s = has_key_ ? kOpenFigiWindowWithKeyS : kOpenFigiWindowWithoutKeyS;
    for (std::size_t start = 0; start < jobs.size(); start += batchSize())
    {
        const std::span<const OpenFigiJob> batch = jobs.subspan(start, std::min(batchSize(), jobs.size() - start));
        const std::string body = buildOpenFigiMappingBody(batch);
        auto fail = [&](const std::string& message) {
            for (std::size_t i = 0; i < batch.size(); ++i)
            {
                out.push_back(unreachable(message));
            }
        };
        for (int attempt = 0;; ++attempt)
        {
            sleepSeconds(std::exchange(pending_wait_s_, 0));
            HttpResponse response;
            try
            {
                ++requests_;
                response = post_(kOpenFigiMappingUrl, body);
            }
            catch (const std::exception& ex)
            {
                fail(std::string("OpenFIGI request failed: ") + ex.what());
                break;
            }
            const OpenFigiRateLimit limit = parseOpenFigiRateLimit(response);
            const int reset_s = limit.reset_s.value_or(limit.window_s.value_or(window_s));
            if (limit.remaining.has_value() && *limit.remaining <= 0)
            {
                pending_wait_s_ = reset_s;
            }
            if (response.status == 429)
            {
                if (attempt < kOpenFigi429Retries)
                {
                    pending_wait_s_ = 0;
                    sleepSeconds(reset_s);
                    continue;
                }
                fail("OpenFIGI rate limit (HTTP 429)");
                break;
            }
            if (response.status == 413)
            {
                throw std::runtime_error("OpenFIGI rejected a batch of " + std::to_string(batch.size()) +
                                         " jobs (HTTP 413)");
            }
            if (response.status != 200)
            {
                fail(response.status == 0 ? "OpenFIGI unreachable: " + response.error
                                          : "OpenFIGI HTTP " + std::to_string(response.status));
                break;
            }
            auto parsed = parseOpenFigiMappingResponse(response.body, batch.size());
            if (!parsed.has_value())
            {
                fail("OpenFIGI response is not one result per job");
                break;
            }
            for (OpenFigiJobResult& result : *parsed)
            {
                out.push_back(std::move(result));
            }
            break;
        }
    }
    return out;
}

ForwardOutcome OpenFigiClient::forward(std::string_view symbol)
{
    const OpenFigiJob job = forwardJob(symbol);
    const auto results = map(std::span<const OpenFigiJob>(&job, 1));
    return reduceForward(results.front(), isIndexSymbol(symbol));
}

ReverseOutcome OpenFigiClient::reverse(std::string_view figi)
{
    const OpenFigiJob job = reverseJob(figi);
    const auto results = map(std::span<const OpenFigiJob>(&job, 1));
    return reduceReverse(results.front());
}

}  // namespace terminal
