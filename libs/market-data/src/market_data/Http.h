// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace terminal {

struct HttpResponse
{
    int status{};
    std::string body;
    std::string error;
    // Response headers with lowercase names. Clients that do not capture them leave it empty.
    std::vector<std::pair<std::string, std::string>> headers;
};

// market-data owns no sockets. The apps inject these; tests inject fakes.
using HttpGet = std::function<HttpResponse(std::string_view url)>;
using HttpPost = std::function<HttpResponse(std::string_view url, std::string_view body)>;

// Case-insensitive lookup of one response header.
[[nodiscard]] inline std::optional<std::string> findHttpHeader(const HttpResponse& response,
                                                               std::string_view name)
{
    for (const auto& [key, value] : response.headers)
    {
        if (key.size() != name.size())
        {
            continue;
        }
        bool same = true;
        for (std::size_t i = 0; i < key.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(key[i])) !=
                std::tolower(static_cast<unsigned char>(name[i])))
            {
                same = false;
                break;
            }
        }
        if (same)
        {
            return value;
        }
    }
    return std::nullopt;
}

}  // namespace terminal
