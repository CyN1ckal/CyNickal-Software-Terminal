// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <chrono>
#include <string_view>
#include <vector>

namespace terminal {

[[nodiscard]] std::chrono::year_month_day sessionDateToYmd(SessionDate session_date);
[[nodiscard]] bool isNyseHoliday(std::chrono::year_month_day ymd);
[[nodiscard]] bool isNyseHoliday(SessionDate session_date);

// Inclusive weekday sessions that are not NYSE holidays. Invalid civil dates throw.
[[nodiscard]] std::vector<SessionDate> nyseSessions(SessionDate from, SessionDate to);

[[nodiscard]] bool sessionStillOpen(std::string_view iana_tz, SessionDate session_date, UnixSeconds now_utc);

}  // namespace terminal
