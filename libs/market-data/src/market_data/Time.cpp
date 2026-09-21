#include "market_data/Time.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>

namespace myapp {
namespace {

using namespace std::chrono;

[[nodiscard]] const time_zone* requireZone(std::string_view iana_tz)
{
    if (iana_tz.empty())
    {
        throw std::runtime_error("empty timezone");
    }
    thread_local std::unordered_map<std::string, const time_zone*> cache;
    const std::string key(iana_tz);
    if (const auto it = cache.find(key); it != cache.end())
    {
        return it->second;
    }
    try
    {
        const time_zone* tz = locate_zone(key);
        cache.emplace(key, tz);
        return tz;
    }
    catch (const std::runtime_error& ex)
    {
        throw std::runtime_error(std::string("missing tzdb zone: ") + key + ": " + ex.what());
    }
}

[[nodiscard]] UnixSeconds toUnix(sys_seconds tp)
{
    return tp.time_since_epoch().count();
}

[[nodiscard]] year_month_day requireSessionYmd(SessionDate session_date)
{
    const int y = session_date / 10000;
    const int mon = (session_date / 100) % 100;
    const int d = session_date % 100;
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(mon)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok())
    {
        throw std::runtime_error("invalid session_date");
    }
    return ymd;
}

[[nodiscard]] UtcWindow localRangeToUtc(const time_zone* tz, local_seconds start_local, local_seconds end_local)
{
    const zoned_time start_z{tz, start_local, choose::earliest};
    const zoned_time end_z{tz, end_local, choose::earliest};
    return UtcWindow{toUnix(floor<seconds>(start_z.get_sys_time())),
                     toUnix(floor<seconds>(end_z.get_sys_time()))};
}

[[nodiscard]] bool parseInt(std::string_view text, int& out)
{
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

[[nodiscard]] std::optional<local_seconds> parseNaiveLocal(std::string_view naive)
{
    // YYYY-MM-DD HH:MM[:SS]
    if (naive.size() < 16)
    {
        return std::nullopt;
    }
    int y = 0;
    int mon = 0;
    int d = 0;
    int h = 0;
    int min = 0;
    int s = 0;
    if (!parseInt(naive.substr(0, 4), y) || naive[4] != '-' || !parseInt(naive.substr(5, 2), mon) ||
        naive[7] != '-' || !parseInt(naive.substr(8, 2), d) || naive[10] != ' ' ||
        !parseInt(naive.substr(11, 2), h) || naive[13] != ':' || !parseInt(naive.substr(14, 2), min))
    {
        return std::nullopt;
    }
    if (naive.size() > 16)
    {
        if (naive.size() != 19 || naive[16] != ':' || !parseInt(naive.substr(17, 2), s))
        {
            return std::nullopt;
        }
    }
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(mon)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok() || h < 0 || h > 23 || min < 0 || min > 59 || s < 0 || s > 59)
    {
        return std::nullopt;
    }
    return local_seconds{local_days{ymd} + hours{h} + minutes{min} + seconds{s}};
}

}  // namespace

UnixSeconds nowUtc()
{
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::optional<UnixSeconds> naiveLocalToUtc(std::string_view iana_tz, std::string_view naive)
{
    const time_zone* tz = requireZone(iana_tz);
    const auto local = parseNaiveLocal(naive);
    if (!local.has_value())
    {
        return std::nullopt;
    }
    const auto info = tz->get_info(*local);
    if (info.result == local_info::nonexistent)
    {
        return std::nullopt;
    }
    const zoned_time zt{tz, *local, choose::earliest};
    return toUnix(floor<seconds>(zt.get_sys_time()));
}

std::optional<UnixSeconds> parseRfc3339Utc(std::string_view text)
{
    // YYYY-MM-DDTHH:MM:SS[.fff]Z
    if (text.size() < 20 || text[10] != 'T' || text.back() != 'Z')
    {
        return std::nullopt;
    }
    const auto core = text.substr(0, 19);
    if (core[4] != '-' || core[7] != '-' || core[13] != ':' || core[16] != ':')
    {
        return std::nullopt;
    }
    int y = 0;
    int mon = 0;
    int d = 0;
    int h = 0;
    int min = 0;
    int s = 0;
    if (!parseInt(core.substr(0, 4), y) || !parseInt(core.substr(5, 2), mon) ||
        !parseInt(core.substr(8, 2), d) || !parseInt(core.substr(11, 2), h) ||
        !parseInt(core.substr(14, 2), min) || !parseInt(core.substr(17, 2), s))
    {
        return std::nullopt;
    }
    if (text.size() > 20)
    {
        if (text[19] != '.')
        {
            return std::nullopt;
        }
        const auto frac = text.substr(20, text.size() - 21);
        if (frac.empty())
        {
            return std::nullopt;
        }
        for (const char c : frac)
        {
            if (c < '0' || c > '9')
            {
                return std::nullopt;
            }
        }
    }
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(mon)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok() || h < 0 || h > 23 || min < 0 || min > 59 || s < 0 || s > 59)
    {
        return std::nullopt;
    }
    const sys_seconds tp{sys_days{ymd} + hours{h} + minutes{min} + seconds{s}};
    return toUnix(tp);
}

std::optional<UnixSeconds> parseUsDateToUtcMidnight(std::string_view text)
{
    // MM/DD/YYYY or MM/DD/YY
    const auto first = text.find('/');
    const auto second = first == std::string_view::npos ? std::string_view::npos : text.find('/', first + 1);
    if (first == std::string_view::npos || second == std::string_view::npos)
    {
        return std::nullopt;
    }
    int mon = 0;
    int d = 0;
    int y = 0;
    if (!parseInt(text.substr(0, first), mon) || !parseInt(text.substr(first + 1, second - first - 1), d) ||
        !parseInt(text.substr(second + 1), y))
    {
        return std::nullopt;
    }
    if (y >= 0 && y < 100)
    {
        y += y >= 70 ? 1900 : 2000;
    }
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(mon)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok())
    {
        return std::nullopt;
    }
    return toUnix(sys_seconds{sys_days{ymd}});
}

std::optional<double> parseMoneyAmount(std::string_view text)
{
    std::string cleaned;
    cleaned.reserve(text.size());
    for (const char c : text)
    {
        if (c == '$' || c == ',' || c == ' ' || c == '\t')
        {
            continue;
        }
        cleaned.push_back(c);
    }
    if (cleaned.empty())
    {
        return std::nullopt;
    }
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(cleaned.data(), cleaned.data() + cleaned.size(), value);
    if (ec != std::errc{} || ptr != cleaned.data() + cleaned.size())
    {
        return std::nullopt;
    }
    return value;
}

SessionDate utcToSessionDate(std::string_view iana_tz, UnixSeconds ts)
{
    const time_zone* tz = requireZone(iana_tz);
    const sys_seconds tp{seconds{ts}};
    const zoned_time zt{tz, tp};
    const year_month_day ymd{floor<days>(zt.get_local_time())};
    return toSessionDate(ymd);
}

UtcWindow sessionUtcWindow(std::string_view iana_tz, SessionDate session_date)
{
    const time_zone* tz = requireZone(iana_tz);
    const year_month_day ymd = requireSessionYmd(session_date);
    const local_seconds start_local{local_days{ymd}};
    const local_seconds end_local{local_days{ymd} + days{1}};
    return localRangeToUtc(tz, start_local, end_local);
}

UtcWindow usRthUtcWindow(std::string_view iana_tz, SessionDate session_date)
{
    const time_zone* tz = requireZone(iana_tz);
    const year_month_day ymd = requireSessionYmd(session_date);
    const local_seconds start_local{local_days{ymd} + hours{9} + minutes{30}};
    const local_seconds end_local{local_days{ymd} + hours{16}};
    return localRangeToUtc(tz, start_local, end_local);
}

bool isUsRthLocal(hh_mm_ss<seconds> local_hms) noexcept
{
    const auto mins = static_cast<int>(local_hms.hours().count()) * 60 +
                      static_cast<int>(local_hms.minutes().count());
    return mins >= (9 * 60 + 30) && mins < (16 * 60);
}

bool isUsRthAt(std::string_view iana_tz, UnixSeconds ts)
{
    const time_zone* tz = requireZone(iana_tz);
    const zoned_time zt{tz, sys_seconds{seconds{ts}}};
    const auto local = zt.get_local_time();
    const hh_mm_ss<seconds> tod{local - floor<days>(local)};
    return isUsRthLocal(tod);
}

SessionDate parseSessionDate(std::string_view text)
{
    std::string digits;
    digits.reserve(8);
    for (const char c : text)
    {
        if (c >= '0' && c <= '9')
        {
            digits.push_back(c);
        }
    }
    if (digits.size() != 8)
    {
        throw std::runtime_error("dates must be YYYYMMDD or YYYY-MM-DD");
    }
    int value = 0;
    const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (ec != std::errc{} || ptr != digits.data() + digits.size())
    {
        throw std::runtime_error("dates must be YYYYMMDD or YYYY-MM-DD");
    }
    const auto date = static_cast<SessionDate>(value);
    (void)requireSessionYmd(date);
    return date;
}

std::string formatSessionDate(SessionDate date)
{
    char buf[16];
    const int y = date / 10000;
    const int mon = (date / 100) % 100;
    const int d = date % 100;
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, mon, d);
    return buf;
}

}  // namespace myapp
