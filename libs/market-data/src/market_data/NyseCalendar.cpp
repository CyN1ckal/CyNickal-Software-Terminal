#include "market_data/NyseCalendar.h"

#include "market_data/Time.h"

#include <chrono>
#include <stdexcept>
#include <string>

namespace terminal {
namespace {

using namespace std::chrono;

[[nodiscard]] year_month_day observed(year_month_day ymd)
{
    const weekday wd{sys_days{ymd}};
    if (wd == Saturday)
    {
        return year_month_day{sys_days{ymd} - days{1}};
    }
    if (wd == Sunday)
    {
        return year_month_day{sys_days{ymd} + days{1}};
    }
    return ymd;
}

[[nodiscard]] year_month_day nthWeekday(year y, month m, weekday wd, int n)
{
    const sys_days first{y / m / 1};
    const auto delta = static_cast<int>((wd - weekday{first}).count());
    return year_month_day{first + days{delta + 7 * (n - 1)}};
}

[[nodiscard]] year_month_day lastWeekdayOfMonth(year y, month m, weekday wd)
{
    const sys_days last_day{y / m / std::chrono::last};
    const auto delta = static_cast<int>((weekday{last_day} - wd).count());
    return year_month_day{last_day - days{delta}};
}

// Anonymous Gregorian Easter Sunday.
[[nodiscard]] year_month_day easterSunday(int y)
{
    const int a = y % 19;
    const int b = y / 100;
    const int c = y % 100;
    const int d = b / 4;
    const int e = b % 4;
    const int f = (b + 8) / 25;
    const int g = (b - f + 1) / 3;
    const int h = (19 * a + b - d - g + 15) % 30;
    const int i = c / 4;
    const int k = c % 4;
    const int l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    const int month_n = (h + l - 7 * m + 114) / 31;
    const int dom = ((h + l - 7 * m + 114) % 31) + 1;
    return year{y} / std::chrono::month{static_cast<unsigned>(month_n)} /
           std::chrono::day{static_cast<unsigned>(dom)};
}

}  // namespace

year_month_day sessionDateToYmd(SessionDate session_date)
{
    const int y = session_date / 10000;
    const int m = (session_date / 100) % 100;
    const int d = session_date % 100;
    const year_month_day ymd{year{y}, month{static_cast<unsigned>(m)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok())
    {
        throw std::runtime_error("invalid session_date");
    }
    return ymd;
}

bool isNyseHoliday(year_month_day ymd)
{
    if (!ymd.ok())
    {
        return false;
    }
    const int y = static_cast<int>(ymd.year());
    const year yy{y};
    if (ymd == observed(yy / January / 1) || ymd == observed((yy + years{1}) / January / 1))
    {
        // Saturday 1 Jan is observed the preceding Friday (previous year).
        return true;
    }
    if (ymd == nthWeekday(yy, January, Monday, 3))
    {
        return true;
    }
    if (ymd == nthWeekday(yy, February, Monday, 3))
    {
        return true;
    }
    if (ymd == year_month_day{sys_days{easterSunday(y)} - days{2}})
    {
        return true;
    }
    if (ymd == lastWeekdayOfMonth(yy, May, Monday))
    {
        return true;
    }
    if (y >= 2022 && ymd == observed(yy / June / 19))
    {
        return true;
    }
    if (ymd == observed(yy / July / 4))
    {
        return true;
    }
    if (ymd == nthWeekday(yy, September, Monday, 1))
    {
        return true;
    }
    if (ymd == nthWeekday(yy, November, Thursday, 4))
    {
        return true;
    }
    if (ymd == observed(yy / December / 25))
    {
        return true;
    }
    return false;
}

bool isNyseHoliday(SessionDate session_date)
{
    return isNyseHoliday(sessionDateToYmd(session_date));
}

std::vector<SessionDate> nyseSessions(SessionDate from, SessionDate to)
{
    if (from > to)
    {
        return {};
    }
    std::vector<SessionDate> out;
    sys_days cursor{sessionDateToYmd(from)};
    const sys_days last{sessionDateToYmd(to)};
    for (; cursor <= last; cursor += days{1})
    {
        const year_month_day ymd{cursor};
        const weekday wd{cursor};
        if (wd == Saturday || wd == Sunday)
        {
            continue;
        }
        if (isNyseHoliday(ymd))
        {
            continue;
        }
        out.push_back(toSessionDate(ymd));
    }
    return out;
}

bool sessionStillOpen(std::string_view iana_tz, SessionDate session_date, UnixSeconds now_utc)
{
    if (utcToSessionDate(iana_tz, now_utc) != session_date)
    {
        return false;
    }
    using namespace std::chrono;
    const zoned_time zt{std::string(iana_tz), sys_seconds{seconds{now_utc}}};
    const auto local = zt.get_local_time();
    const hh_mm_ss<seconds> tod{local - floor<days>(local)};
    const int mins = static_cast<int>(tod.hours().count()) * 60 + static_cast<int>(tod.minutes().count());
    return mins < 16 * 60;
}

}  // namespace terminal
