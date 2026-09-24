// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Figi.h"
#include "market_data/Http.h"
#include "market_data/OpenFigi.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <chrono>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Answers POST /v3/mapping from the captured fixtures in tests/fixtures/openfigi.
// Each job is matched against the recorded request jobs by JSON equality, so the
// answers are exactly what OpenFIGI returned on 2026-09-23. A job that was never
// recorded answers {"warning": "No identifier found."} unless a test overrides it.
// A permissive fake ignores the fixtures and confirms every ticker as
// testingFigiFor(store symbol), which is what Store::testingInsertInstrument stores.
class FakeOpenFigi
{
public:
    explicit FakeOpenFigi(bool permissive = false) : permissive_(permissive)
    {
        const std::filesystem::path dir = std::filesystem::path(TERMINAL_MARKET_DATA_FIXTURE_DIR) / "openfigi";
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            const std::string name = entry.path().filename().string();
            const std::string suffix = ".request.json";
            if (name.size() <= suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            {
                continue;
            }
            const std::string stem = name.substr(0, name.size() - suffix.size());
            const nlohmann::json request = nlohmann::json::parse(readFile(entry.path()));
            const nlohmann::json response = nlohmann::json::parse(readFile(dir / (stem + ".response.json")));
            for (std::size_t i = 0; i < request.size(); ++i)
            {
                fixture_answers_[request[i].dump()] = response[i];
            }
        }
    }

    [[nodiscard]] static std::string readFile(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            throw std::runtime_error("missing fixture " + path.string());
        }
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] static nlohmann::json fixture(std::string_view stem)
    {
        const std::filesystem::path dir = std::filesystem::path(TERMINAL_MARKET_DATA_FIXTURE_DIR) / "openfigi";
        return nlohmann::json::parse(readFile(dir / std::string(stem)));
    }

    [[nodiscard]] static std::string key(const terminal::OpenFigiJob& job)
    {
        const std::string one = terminal::buildOpenFigiMappingBody(std::span(&job, 1));
        return nlohmann::json::parse(one)[0].dump();
    }

    void set(const terminal::OpenFigiJob& job, nlohmann::json element) { overrides_[key(job)] = std::move(element); }

    // A forward answer for symbol naming one US composite.
    void setForward(std::string_view symbol,
                    std::string_view figi,
                    std::string_view name,
                    std::string_view security_type = "Common Stock")
    {
        set(terminal::forwardJob(symbol), hitElement(terminal::toOpenFigiTicker(symbol), figi, name, security_type));
    }

    void setNoMatch(const terminal::OpenFigiJob& job) { set(job, nlohmann::json{{"warning", "No identifier found."}}); }

    // A reverse answer: figi's current ticker, in store spelling.
    void setReverse(std::string_view figi, std::string_view symbol, std::string_view name = "TEST")
    {
        set(terminal::reverseJob(figi), hitElement(terminal::toOpenFigiTicker(symbol), figi, name, "Common Stock"));
    }

    [[nodiscard]] static nlohmann::json hitElement(std::string_view ticker,
                                                   std::string_view figi,
                                                   std::string_view name,
                                                   std::string_view security_type)
    {
        nlohmann::json hit;
        hit["figi"] = figi;
        hit["name"] = name;
        hit["ticker"] = ticker;
        hit["exchCode"] = "US";
        hit["compositeFIGI"] = figi;
        hit["securityType"] = security_type;
        hit["marketSector"] = "Equity";
        hit["shareClassFIGI"] = nullptr;
        hit["securityType2"] = security_type;
        hit["securityDescription"] = ticker;
        return nlohmann::json{{"data", nlohmann::json::array({hit})}};
    }

    // Every request returns this status (0 = transport failure) with an empty body.
    void failWith(int status) { fail_status_ = status; }
    void recover() { fail_status_.reset(); }
    // The next n requests return 429 before answering normally.
    void throttle(int n) { throttled_ = n; }

    [[nodiscard]] terminal::HttpPost post()
    {
        return [this](std::string_view url, std::string_view body) { return handle(url, body); };
    }

    int requests = 0;
    int jobs = 0;
    std::vector<std::string> bodies;

private:
    terminal::HttpResponse handle(std::string_view url, std::string_view body)
    {
        ++requests;
        bodies.emplace_back(body);
        if (url != terminal::kOpenFigiMappingUrl)
        {
            throw std::runtime_error("unexpected OpenFIGI URL");
        }
        terminal::HttpResponse response;
        response.headers = {{"ratelimit-limit", "25"}, {"ratelimit-remaining", "24"}, {"ratelimit-reset", "60"}};
        if (fail_status_.has_value())
        {
            response.status = *fail_status_;
            response.error = *fail_status_ == 0 ? "connection refused" : "";
            return response;
        }
        if (throttled_ > 0)
        {
            --throttled_;
            response.status = 429;
            response.headers = {{"ratelimit-remaining", "0"}, {"ratelimit-reset", "7"}};
            return response;
        }
        const nlohmann::json request = nlohmann::json::parse(body);
        nlohmann::json out = nlohmann::json::array();
        for (const nlohmann::json& job : request)
        {
            ++jobs;
            out.push_back(answer(job));
        }
        response.status = 200;
        response.body = out.dump();
        return response;
    }

    [[nodiscard]] nlohmann::json answer(const nlohmann::json& job) const
    {
        if (const auto found = overrides_.find(job.dump()); found != overrides_.end())
        {
            return found->second;
        }
        if (permissive_ && job.value("idType", "") == "TICKER")
        {
            const std::string ticker = job.value("idValue", "");
            const std::string symbol = terminal::fromOpenFigiTicker(ticker);
            const std::string figi = terminal::testingFigiFor(symbol);
            nlohmann::json element = hitElement(ticker, figi, symbol + " TEST", "Common Stock");
            if (job.value("marketSecDes", "") == "Index")
            {
                nlohmann::json& hit = element["data"][0];
                hit["compositeFIGI"] = nullptr;
                hit["exchCode"] = nullptr;
                hit["marketSector"] = "Index";
                hit["securityType"] = "Equity Index";
            }
            return element;
        }
        if (!permissive_)
        {
            if (const auto found = fixture_answers_.find(job.dump()); found != fixture_answers_.end())
            {
                return found->second;
            }
        }
        return nlohmann::json{{"warning", "No identifier found."}};
    }

    bool permissive_ = false;
    std::map<std::string, nlohmann::json> fixture_answers_;
    std::map<std::string, nlohmann::json> overrides_;
    std::optional<int> fail_status_;
    int throttled_ = 0;
};

// A client over the fake that never really sleeps; it records the waits instead.
struct FakeOpenFigiClient
{
    explicit FakeOpenFigiClient(bool permissive = false, bool has_key = false)
        : fake(permissive), client(fake.post(), has_key, [this](std::chrono::milliseconds ms) { slept.push_back(ms); })
    {
    }

    FakeOpenFigi fake;
    std::vector<std::chrono::milliseconds> slept;
    terminal::OpenFigiClient client;
};
