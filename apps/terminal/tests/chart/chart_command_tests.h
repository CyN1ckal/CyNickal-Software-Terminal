// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartCommand.h"
#include "IngestDefaults.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Time.h"

#include <chrono>

TEST_CASE("parseChartCommand switches symbol or supported bar period")
{
    using terminal::ChartBarPeriod;
    using terminal::ChartCommandKind;

    const auto qqq = terminal::parseChartCommand("qqq", ChartBarPeriod::Minute1);
    CHECK(qqq.kind == ChartCommandKind::Symbol);
    CHECK(qqq.symbol == "QQQ");

    const auto slashed = terminal::parseChartCommand(" /msft ", ChartBarPeriod::Minute15);
    CHECK(slashed.kind == ChartCommandKind::Symbol);
    CHECK(slashed.symbol == "MSFT");

    const auto brk = terminal::parseChartCommand("brk.b", ChartBarPeriod::Hour1);
    CHECK(brk.kind == ChartCommandKind::Symbol);
    CHECK(brk.symbol == "BRK.B");

    const auto minutes = terminal::parseChartCommand("15m", ChartBarPeriod::Day1);
    CHECK(minutes.kind == ChartCommandKind::Period);
    CHECK(minutes.period == ChartBarPeriod::Minute15);

    const auto minutes_word = terminal::parseChartCommand("15 min", ChartBarPeriod::Minute1);
    CHECK(minutes_word.kind == ChartCommandKind::Period);
    CHECK(minutes_word.period == ChartBarPeriod::Minute15);

    const auto hour = terminal::parseChartCommand("1H", ChartBarPeriod::Minute5);
    CHECK(hour.kind == ChartCommandKind::Period);
    CHECK(hour.period == ChartBarPeriod::Hour1);

    const auto hour_word = terminal::parseChartCommand("1 hr", ChartBarPeriod::Minute1);
    CHECK(hour_word.kind == ChartCommandKind::Period);
    CHECK(hour_word.period == ChartBarPeriod::Hour1);

    const auto day = terminal::parseChartCommand("1 day", ChartBarPeriod::Minute5);
    CHECK(day.kind == ChartCommandKind::Period);
    CHECK(day.period == ChartBarPeriod::Day1);

    const auto daily = terminal::parseChartCommand("1d", ChartBarPeriod::Minute1);
    CHECK(daily.kind == ChartCommandKind::Period);
    CHECK(daily.period == ChartBarPeriod::Day1);

    const auto bare_minute = terminal::parseChartCommand("5", ChartBarPeriod::Minute15);
    CHECK(bare_minute.kind == ChartCommandKind::Period);
    CHECK(bare_minute.period == ChartBarPeriod::Minute5);

    const auto bare_hour = terminal::parseChartCommand("1", ChartBarPeriod::Hour1);
    CHECK(bare_hour.kind == ChartCommandKind::Period);
    CHECK(bare_hour.period == ChartBarPeriod::Hour1);

    const auto bare_day = terminal::parseChartCommand("1", ChartBarPeriod::Day1);
    CHECK(bare_day.kind == ChartCommandKind::Period);
    CHECK(bare_day.period == ChartBarPeriod::Day1);

    const auto five_minute = terminal::parseChartCommand("5m", ChartBarPeriod::Day1);
    CHECK(five_minute.kind == ChartCommandKind::Period);
    CHECK(five_minute.period == ChartBarPeriod::Minute5);

    const auto one_minute = terminal::parseChartCommand("1m", ChartBarPeriod::Day1);
    CHECK(one_minute.kind == ChartCommandKind::Period);
    CHECK(one_minute.period == ChartBarPeriod::Minute1);
}

TEST_CASE("parseChartCommand rejects unsupported periods and blank input")
{
    using terminal::ChartBarPeriod;
    using terminal::ChartCommandKind;

    const auto two_minute = terminal::parseChartCommand("2m", ChartBarPeriod::Minute1);
    CHECK(two_minute.kind == ChartCommandKind::Rejected);
    CHECK(two_minute.message == "2m is not a chart period");

    const auto seconds = terminal::parseChartCommand("10s", ChartBarPeriod::Minute1);
    CHECK(seconds.kind == ChartCommandKind::Rejected);
    CHECK(seconds.message == "10s is not a chart period");

    const auto five_day = terminal::parseChartCommand("5", ChartBarPeriod::Day1);
    CHECK(five_day.kind == ChartCommandKind::Rejected);
    CHECK(five_day.message == "5d is not a chart period");

    const auto blank = terminal::parseChartCommand("   ", ChartBarPeriod::Minute1);
    CHECK(blank.kind == ChartCommandKind::Empty);

    const auto slash = terminal::parseChartCommand("/", ChartBarPeriod::Minute1);
    CHECK(slash.kind == ChartCommandKind::Rejected);
    CHECK(slash.message == "enter a symbol");

    const auto junk = terminal::parseChartCommand("??", ChartBarPeriod::Minute1);
    CHECK(junk.kind == ChartCommandKind::Rejected);
    CHECK(junk.message == "invalid symbol");

    const auto forced_period = terminal::parseChartCommand("/15m", ChartBarPeriod::Minute1);
    CHECK(forced_period.kind == ChartCommandKind::Rejected);
    CHECK(forced_period.message == "invalid symbol");
}

TEST_CASE("chartDownloadLookbackDays covers the session count and the DATA preset")
{
    constexpr terminal::SessionDate kToday = 20260921;
    const auto ymd = terminal::sessionDateToYmd(kToday);
    const auto span_covers = [&](int lookback, int sessions) {
        const terminal::SessionDate from =
            terminal::toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{lookback});
        return terminal::nyseSessions(from, kToday).size() >= static_cast<std::size_t>(sessions);
    };

    terminal::CChartSettings settings;
    CHECK(terminal::chartDownloadLookbackDays(settings, kToday) == 26);

    settings.period = terminal::ChartBarPeriod::Day1;
    const int daily_default = terminal::chartDownloadLookbackDays(settings, kToday);
    CHECK(daily_default >= terminal::kIngestDefaultDailyDays);
    CHECK(span_covers(daily_default, terminal::kChartDefaultHistoricalSessionCount));

    settings.historical_session_count = 1295;
    const int daily_mid = terminal::chartDownloadLookbackDays(settings, kToday);
    CHECK(span_covers(daily_mid, 1295));

    settings.historical_session_count = terminal::kChartMaxHistoricalSessionCount;
    const int daily_max = terminal::chartDownloadLookbackDays(settings, kToday);
    CHECK(daily_max < terminal::kChartMaxHistoricalSessionCount * 2);
    CHECK(span_covers(daily_max, terminal::kChartMaxHistoricalSessionCount));

    settings.period = terminal::ChartBarPeriod::Minute15;
    settings.intraday_session_count = 252;
    CHECK(terminal::chartDownloadLookbackDays(settings, kToday) == 252 * 7 / 5 + 7);

    settings.period = terminal::ChartBarPeriod::Minute1;
    settings.intraday_session_count = 1;
    CHECK(terminal::chartDownloadLookbackDays(settings, kToday) == 21);
}

TEST_CASE("chartDownloadWindow is 1m for intraday and 1d for a daily chart")
{
    constexpr terminal::SessionDate kToday = 20260921;
    terminal::CChartSettings settings;
    settings.symbol = " qqq ";
    settings.period = terminal::ChartBarPeriod::Minute15;
    const terminal::ChartDownloadRequest intraday = terminal::chartDownloadWindow(settings, kToday);
    CHECK(intraday.symbol == "QQQ");
    CHECK(intraday.timeframe_s == terminal::kTimeframe1m);
    CHECK(intraday.to == kToday);
    const auto ymd = terminal::sessionDateToYmd(kToday);
    const auto from = terminal::toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{26});
    CHECK(intraday.from == from);

    settings.period = terminal::ChartBarPeriod::Day1;
    const terminal::ChartDownloadRequest daily = terminal::chartDownloadWindow(settings, kToday);
    CHECK(daily.timeframe_s == terminal::kTimeframe1d);
    CHECK(daily.to == kToday);
    const int daily_lookback = terminal::chartDownloadLookbackDays(settings, kToday);
    const auto daily_from =
        terminal::toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{daily_lookback});
    CHECK(daily.from == daily_from);
}
