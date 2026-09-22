// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartLoad.h"

#include "chart/CChartTransform.h"
#include "market_data/Adjust.h"
#include "market_data/Time.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace terminal {
namespace {

int clampSessionCount(int session_count, ChartBarPeriod period) noexcept
{
    if (session_count < kChartMinSessionCount)
    {
        return kChartMinSessionCount;
    }
    const int max_sessions = chartMaxSessionCount(period);
    if (session_count > max_sessions)
    {
        return max_sessions;
    }
    return session_count;
}

[[nodiscard]] std::vector<CoverageDay> collectSessions(const std::vector<CoverageDay>& days,
                                                       int session_count)
{
    std::vector<CoverageDay> collected;
    collected.reserve(static_cast<std::size_t>(session_count));
    for (const CoverageDay& day : days)
    {
        if (day.bar_count <= 0)
        {
            continue;
        }
        collected.push_back(day);
        if (std::cmp_greater_equal(collected.size(), session_count))
        {
            break;
        }
    }
    return collected;
}

ChartLoadResult caughtAsStatus(const std::exception& ex)
{
    ChartLoadResult out;
    out.message = ex.what();
    out.status = isStoreBusyError(out.message) ? ChartLoadStatus::Busy : ChartLoadStatus::Error;
    return out;
}

[[nodiscard]] bool coverageHasBars(const Store& store, InstrumentId id, int timeframe_s)
{
    const std::vector<CoverageDay> days = store.queryCoverageDays(id, timeframe_s);
    return std::ranges::any_of(days, [](const CoverageDay& day) { return day.bar_count > 0; });
}

}  // namespace

std::string normalizeChartSymbol(std::string_view symbol)
{
    std::size_t begin = 0;
    while (begin < symbol.size() &&
           std::isspace(static_cast<unsigned char>(symbol[begin])) != 0)
    {
        ++begin;
    }
    std::size_t end = symbol.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(symbol[end - 1])) != 0)
    {
        --end;
    }
    std::string out(symbol.substr(begin, end - begin));
    for (char& ch : out)
    {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool isStoreBusyError(std::string_view what) noexcept
{
    return what.find("busy") != std::string_view::npos ||
           what.find("locked") != std::string_view::npos;
}

ChartLoadResult loadChartBars(const Store& store, const CChartSettings& settings)
{
    try
    {
        const std::string symbol = normalizeChartSymbol(settings.symbol);
        if (symbol.empty())
        {
            ChartLoadResult out;
            out.status = ChartLoadStatus::Unconfigured;
            out.message = "Type a symbol and press Enter, or open Chart Settings.";
            return out;
        }
        if (!isChartSettingsSupported(settings))
        {
            ChartLoadResult out;
            out.status = ChartLoadStatus::Unsupported;
            out.message = "candlestick bars and Days to Load only.";
            return out;
        }

        const int session_count = clampSessionCount(chartSessionCount(settings), settings.period);
        const std::vector<Instrument> found = store.findInstrumentsBySymbol(symbol);
        if (found.empty())
        {
            ChartLoadResult out;
            out.status = ChartLoadStatus::UnknownSymbol;
            out.message = "unknown symbol " + symbol;
            return out;
        }
        if (found.size() > 1)
        {
            ChartLoadResult out;
            out.status = ChartLoadStatus::AmbiguousSymbol;
            out.message = "multiple instruments named " + symbol;
            return out;
        }

        const Instrument& instrument = found.front();
        ChartLoadResult out;
        out.instrument = instrument;
        const InstrumentId id = instrument.id;
        const std::string_view tz = instrument.timezone.empty()
                                        ? std::string_view{"America/New_York"}
                                        : std::string_view{instrument.timezone};

        std::vector<CoverageDay> collected;
        const bool daily = settings.period == ChartBarPeriod::Day1;
        if (daily)
        {
            // Historical charts read the stored daily series only. 1-minute rows are not a substitute.
            collected = collectSessions(store.queryCoverageDays(id, kTimeframe1d), session_count);
        }
        else
        {
            collected = collectSessions(store.queryCoverageDays(id, kTimeframe1m), session_count);
        }
        if (collected.empty())
        {
            out.status = ChartLoadStatus::Empty;
            out.message = std::string{"no "} + chartPeriodCode(settings.period) + " bars for " +
                          symbol;
            return out;
        }

        const SessionDate last_session = collected.front().session_date;
        const SessionDate first_session = collected.back().session_date;
        out.last_session = last_session;
        out.first_session = first_session;
        out.sessions_used = static_cast<int>(collected.size());
        out.ts_begin = usRthUtcWindow(tz, first_session).start;
        out.ts_end = usRthUtcWindow(tz, last_session).end;
        if (daily)
        {
            out.bars = store.queryBars(id, kTimeframe1d, out.ts_begin, out.ts_end);
            if (!out.bars.empty())
            {
                const std::vector<CorporateAction> actions =
                    store.queryCorporateActions(id, 0, out.bars.back().ts);
                out.bars = adjustBarsForSplits(std::move(out.bars), actions);
            }
        }
        else
        {
            out.bars = store.queryBars(id, kTimeframe1m, out.ts_begin, out.ts_end);
            if (chartNeedsBarTransform(settings))
            {
                out.bars = transformChartBars(out.bars, settings.period, tz);
            }
        }
        if (out.bars.empty())
        {
            out.status = ChartLoadStatus::Empty;
            out.message = std::string{"no "} + chartPeriodCode(settings.period) + " bars for " +
                          symbol;
            return out;
        }

        out.status = ChartLoadStatus::Ready;
        out.message = symbol + "  " + chartPeriodCode(settings.period) + "  " +
                      formatSessionDate(first_session) + " .. " + formatSessionDate(last_session) +
                      "  " + std::to_string(out.sessions_used) + " of " +
                      std::to_string(session_count) + " sessions  " +
                      std::to_string(out.bars.size()) + " bars";
        return out;
    }
    catch (const std::exception& ex)
    {
        return caughtAsStatus(ex);
    }
}

std::optional<ChartDownloadRequest> chartDownloadRequest(const Store& store,
                                                         const CChartSettings& settings,
                                                         SessionDate today)
{
    ChartDownloadRequest window = chartDownloadWindow(settings, today);
    if (window.symbol.empty() || !isChartSettingsSupported(settings))
    {
        return std::nullopt;
    }

    const std::vector<Instrument> found = store.findInstrumentsBySymbol(window.symbol);
    if (found.size() > 1)
    {
        return std::nullopt;
    }
    if (found.size() == 1)
    {
        const InstrumentId id = found.front().id;
        const int timeframe_s =
            settings.period == ChartBarPeriod::Day1 ? kTimeframe1d : kTimeframe1m;
        const bool has_bars = coverageHasBars(store, id, timeframe_s);
        if (has_bars)
        {
            return std::nullopt;
        }
    }
    return window;
}

}  // namespace terminal
