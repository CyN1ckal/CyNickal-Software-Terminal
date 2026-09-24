// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "CurlClient.h"

#include "market_data/OpenFigi.h"
#include "market_data/Secrets.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// A bearer-less CurlClient and the OpenFigiClient that posts through it. The optional
// secrets.json "openfigi" key is sent as X-OPENFIGI-APIKEY. It is never logged,
// never placed in a URL, and never written to SQLite.
class OpenFigiSession
{
public:
    explicit OpenFigiSession(const std::filesystem::path& secrets_path)
        : key_(loadOptionalSecret(secrets_path, "openfigi")),
          http_(std::string_view{}),
          client_([this](std::string_view url, std::string_view body) { return http_.post(url, body, headers_); },
                  key_.has_value())
    {
        if (key_.has_value())
        {
            headers_.push_back("X-OPENFIGI-APIKEY: " + *key_);
        }
    }

    OpenFigiSession(const OpenFigiSession&) = delete;
    OpenFigiSession& operator=(const OpenFigiSession&) = delete;
    OpenFigiSession(OpenFigiSession&&) = delete;
    OpenFigiSession& operator=(OpenFigiSession&&) = delete;
    ~OpenFigiSession() = default;

    [[nodiscard]] OpenFigiClient& client() noexcept { return client_; }
    [[nodiscard]] bool hasKey() const noexcept { return key_.has_value(); }

private:
    std::optional<std::string> key_;
    std::vector<std::string> headers_;
    CurlClient http_;
    OpenFigiClient client_;
};

}  // namespace terminal
