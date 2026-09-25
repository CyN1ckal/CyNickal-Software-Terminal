// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Time.h"

#include <cstdio>
#include <ctime>
#include <optional>
#include <string>

namespace terminal {

// "2020-08-31 04:00:05 UTC". Empty when the clock cannot be formatted.
[[nodiscard]] inline std::string formatReceivedUtc(UnixSeconds ts)
{
    std::tm parts{};
    if (!tryUtcTm(static_cast<std::time_t>(ts), parts))
    {
        return {};
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d UTC", parts.tm_year + 1900,
                  parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    return buf;
}

// Right-aligned on its own line. No-op without a time.
void drawReceivedStamp(std::optional<UnixSeconds> received);

}  // namespace terminal
