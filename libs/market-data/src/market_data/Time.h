#pragma once

#include "market_data/Types.h"

#include <chrono>
#include <optional>
#include <string_view>

namespace myapp {

UnixSeconds nowUtc();

// Data errors → nullopt (unparseable string, spring DST gap).
// Throws std::runtime_error only if iana_tz is empty or missing from tzdb.
std::optional<UnixSeconds> naiveLocalToUtc(std::string_view iana_tz, std::string_view naive);

// RFC3339 / ISO-8601 UTC, e.g. "2020-08-31T04:00:00.000Z". nullopt if unparseable.
std::optional<UnixSeconds> parseRfc3339Utc(std::string_view text);

// "MM/DD/YYYY" or "MM/DD/YY" (yy >= 70 → 19xx else 20xx) at 00:00:00 UTC.
// nullopt if unparseable.
std::optional<UnixSeconds> parseUsDateToUtcMidnight(std::string_view text);

// "$0.050", "0.3", "1,234.56". nullopt if empty/unparseable.
std::optional<double> parseMoneyAmount(std::string_view text);

// Throws if iana_tz is empty or missing from tzdb.
SessionDate utcToSessionDate(std::string_view iana_tz, UnixSeconds ts);

struct UtcWindow
{
    UnixSeconds start{};
    UnixSeconds end{};
};

// Throws if iana_tz is empty/missing from tzdb, or session_date is not a valid civil date.
UtcWindow sessionUtcWindow(std::string_view iana_tz, SessionDate session_date);

// [09:30, 16:00) local in iana_tz, converted to UTC. Same throw rules as sessionUtcWindow.
UtcWindow usRthUtcWindow(std::string_view iana_tz, SessionDate session_date);

bool isUsRthLocal(std::chrono::hh_mm_ss<std::chrono::seconds> local_hms) noexcept;
bool isUsRthAt(std::string_view iana_tz, UnixSeconds ts);

inline SessionDate toSessionDate(std::chrono::year_month_day ymd) noexcept
{
    const int y = static_cast<int>(ymd.year());
    const unsigned m = static_cast<unsigned>(ymd.month());
    const unsigned d = static_cast<unsigned>(ymd.day());
    return static_cast<SessionDate>(y * 10000 + static_cast<int>(m) * 100 + static_cast<int>(d));
}

}  // namespace myapp
