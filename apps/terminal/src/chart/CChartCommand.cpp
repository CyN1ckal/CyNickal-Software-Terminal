// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartCommand.h"

#include "IngestDefaults.h"
#include "chart/CChartLoad.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Time.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <string>

namespace terminal {
namespace {

enum class PeriodUnit : std::uint8_t
{
    Minute,
    Hour,
    Day,
    Unsupported
};

constexpr int kChartSymbolMaxLen = 31;
constexpr int kChartDownloadWeekDays = 5;
constexpr int kChartDownloadWeekSpan = 7;
constexpr int kChartIntradayDownloadMinDays = 21;
constexpr int kChartIntradayDownloadPadDays = 7;

[[nodiscard]] int calendarDaysBackForSessions(SessionDate today, int sessions) noexcept
{
    try
    {
        using std::chrono::days;
        using std::chrono::Saturday;
        using std::chrono::Sunday;
        using std::chrono::sys_days;
        using std::chrono::weekday;
        using std::chrono::year_month_day;
        sys_days cursor{sessionDateToYmd(today)};
        int seen = 0;
        int stepped = 0;
        const int limit = sessions * 3;
        while (stepped < limit)
        {
            const year_month_day ymd{cursor};
            const weekday wd{cursor};
            if (wd != Saturday && wd != Sunday && !isNyseHoliday(ymd))
            {
                ++seen;
            }
            if (seen >= sessions)
            {
                return stepped;
            }
            cursor -= days{1};
            ++stepped;
        }
        return stepped;
    }
    catch (const std::exception&)
    {
        return sessions * 2;
    }
}

[[nodiscard]] std::string trimCopy(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
    {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    {
        --end;
    }
    return std::string{text.substr(begin, end - begin)};
}

[[nodiscard]] std::string lowerCopy(std::string_view text)
{
    std::string out{text};
    for (char& ch : out)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

[[nodiscard]] bool isChartTicker(std::string_view symbol)
{
    if (symbol.empty() || symbol.size() > static_cast<std::size_t>(kChartSymbolMaxLen))
    {
        return false;
    }
    if (std::isalpha(static_cast<unsigned char>(symbol.front())) == 0)
    {
        return false;
    }
    return std::ranges::all_of(symbol, [](char ch) {
        const auto u = static_cast<unsigned char>(ch);
        return std::isalnum(u) != 0 || ch == '.' || ch == '-';
    });
}

// false: the suffix is not a bar-period unit, so the buffer is a symbol attempt.
[[nodiscard]] bool classifyUnit(std::string_view lower, PeriodUnit& unit)
{
    if (lower == "m" || lower == "min" || lower == "mins" || lower == "minute" || lower == "minutes")
    {
        unit = PeriodUnit::Minute;
        return true;
    }
    if (lower == "h" || lower == "hr" || lower == "hrs" || lower == "hour" || lower == "hours")
    {
        unit = PeriodUnit::Hour;
        return true;
    }
    if (lower == "d" || lower == "day" || lower == "days")
    {
        unit = PeriodUnit::Day;
        return true;
    }
    if (lower == "s" || lower == "sec" || lower == "secs" || lower == "second" ||
        lower == "seconds" || lower == "t" || lower == "v" || lower == "r" || lower == "rm")
    {
        unit = PeriodUnit::Unsupported;
        return true;
    }
    return false;
}

[[nodiscard]] PeriodUnit inferUnit(ChartBarPeriod current) noexcept
{
    switch (current)
    {
    case ChartBarPeriod::Minute1:
    case ChartBarPeriod::Minute5:
    case ChartBarPeriod::Minute15:
        return PeriodUnit::Minute;
    case ChartBarPeriod::Hour1:
        return PeriodUnit::Hour;
    case ChartBarPeriod::Day1:
        return PeriodUnit::Day;
    }
    return PeriodUnit::Minute;
}

[[nodiscard]] const char* unitLetter(PeriodUnit unit) noexcept
{
    switch (unit)
    {
    case PeriodUnit::Minute:
        return "m";
    case PeriodUnit::Hour:
        return "h";
    case PeriodUnit::Day:
        return "d";
    case PeriodUnit::Unsupported:
        return "";
    }
    return "";
}

[[nodiscard]] bool mapPeriod(int value, PeriodUnit unit, ChartBarPeriod& period) noexcept
{
    if (unit == PeriodUnit::Minute && value == 1)
    {
        period = ChartBarPeriod::Minute1;
        return true;
    }
    if (unit == PeriodUnit::Minute && value == 5)
    {
        period = ChartBarPeriod::Minute5;
        return true;
    }
    if (unit == PeriodUnit::Minute && value == 15)
    {
        period = ChartBarPeriod::Minute15;
        return true;
    }
    if (unit == PeriodUnit::Hour && value == 1)
    {
        period = ChartBarPeriod::Hour1;
        return true;
    }
    if (unit == PeriodUnit::Day && value == 1)
    {
        period = ChartBarPeriod::Day1;
        return true;
    }
    return false;
}

struct PeriodAttempt
{
    bool is_period{false};
    bool supported{false};
    ChartBarPeriod period{ChartBarPeriod::Minute1};
    std::string message;
};

[[nodiscard]] PeriodAttempt parsePeriodAttempt(std::string_view text, ChartBarPeriod current)
{
    PeriodAttempt out;
    if (text.empty() || std::isdigit(static_cast<unsigned char>(text.front())) == 0)
    {
        return out;
    }

    int value = 0;
    std::size_t i = 0;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0)
    {
        if (value > 100000)
        {
            out.is_period = true;
            out.message = std::string{text} + " is not a chart period";
            return out;
        }
        value = value * 10 + (text[i] - '0');
        ++i;
    }
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) != 0)
    {
        ++i;
    }
    const std::string_view unit_raw = text.substr(i);
    for (const char ch : unit_raw)
    {
        if (std::isalpha(static_cast<unsigned char>(ch)) == 0)
        {
            return out;
        }
    }

    PeriodUnit unit = PeriodUnit::Minute;
    if (unit_raw.empty())
    {
        unit = inferUnit(current);
    }
    else if (!classifyUnit(lowerCopy(unit_raw), unit))
    {
        return out;
    }

    out.is_period = true;
    if (unit == PeriodUnit::Unsupported)
    {
        out.message = std::string{text} + " is not a chart period";
        return out;
    }
    if (mapPeriod(value, unit, out.period))
    {
        out.supported = true;
        return out;
    }
    out.message = std::to_string(value) + unitLetter(unit) + " is not a chart period";
    return out;
}

}  // namespace

ChartCommand parseChartCommand(std::string_view text, ChartBarPeriod current)
{
    ChartCommand command;
    std::string body = trimCopy(text);
    if (body.empty())
    {
        command.kind = ChartCommandKind::Empty;
        return command;
    }
    if (body.front() == '/')
    {
        body = trimCopy(std::string_view{body}.substr(1));
        if (body.empty())
        {
            command.kind = ChartCommandKind::Rejected;
            command.message = "enter a symbol";
            return command;
        }
    }
    else
    {
        const PeriodAttempt attempt = parsePeriodAttempt(body, current);
        if (attempt.is_period)
        {
            if (attempt.supported)
            {
                command.kind = ChartCommandKind::Period;
                command.period = attempt.period;
                return command;
            }
            command.kind = ChartCommandKind::Rejected;
            command.message = attempt.message;
            return command;
        }
    }

    command.symbol = normalizeChartSymbol(body);
    if (!isChartTicker(command.symbol))
    {
        command.kind = ChartCommandKind::Rejected;
        command.message = "invalid symbol";
        command.symbol.clear();
        return command;
    }
    command.kind = ChartCommandKind::Symbol;
    return command;
}

int chartDownloadLookbackDays(const CChartSettings& settings, SessionDate today) noexcept
{
    int sessions = chartSessionCount(settings);
    const int cap = chartMaxSessionCount(settings.period);
    if (sessions < kChartMinSessionCount)
    {
        sessions = kChartMinSessionCount;
    }
    if (sessions > cap)
    {
        sessions = cap;
    }
    if (chartPeriodIsHistorical(settings.period))
    {
        const int spanned = calendarDaysBackForSessions(today, sessions);
        return spanned > kIngestDefaultDailyDays ? spanned : kIngestDefaultDailyDays;
    }
    const int spanned =
        sessions * kChartDownloadWeekSpan / kChartDownloadWeekDays + kChartIntradayDownloadPadDays;
    return spanned > kChartIntradayDownloadMinDays ? spanned : kChartIntradayDownloadMinDays;
}

ChartDownloadRequest chartDownloadWindow(const CChartSettings& settings, SessionDate today)
{
    ChartDownloadRequest request;
    request.symbol = normalizeChartSymbol(settings.symbol);
    request.timeframe_s =
        chartPeriodIsHistorical(settings.period) ? kTimeframe1d : kTimeframe1m;
    request.to = today;
    const int lookback = chartDownloadLookbackDays(settings, today);
    const auto ymd = sessionDateToYmd(today);
    request.from =
        toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{lookback});
    if (request.from > request.to)
    {
        request.from = request.to;
    }
    return request;
}

}  // namespace terminal
