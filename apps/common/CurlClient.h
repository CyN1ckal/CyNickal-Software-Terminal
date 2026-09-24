// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Http.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace terminal {

class CurlClient
{
public:
    // An empty bearer sends no Authorization header (the OpenFIGI client).
    explicit CurlClient(std::string_view bearer);
    ~CurlClient();

    CurlClient(const CurlClient&) = delete;
    CurlClient& operator=(const CurlClient&) = delete;
    CurlClient(CurlClient&&) = delete;
    CurlClient& operator=(CurlClient&&) = delete;

    [[nodiscard]] HttpResponse get(std::string_view url);
    [[nodiscard]] HttpResponse getWithRetry(std::string_view url);
    // POST with Content-Type: application/json plus extra_headers ("Name: value").
    // Never sends the bearer. Response headers are captured with lowercase names.
    [[nodiscard]] HttpResponse post(std::string_view url,
                                    std::string_view body,
                                    const std::vector<std::string>& extra_headers = {});

private:
    void appendHeader(const char* line);

    void* easy_ = nullptr;
    void* headers_ = nullptr;
    std::string auth_;
    bool global_inited_ = false;
};

}  // namespace terminal
