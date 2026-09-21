// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartLoad.h"

#include "chart/CChartTransform.h"
#include "market_data/Time.h"

#include <cctype>
#include <exception>
#include <string>

namespace terminal {
namespace {

int clampSessionCount(int session_count) noexcept
{
    if (session_count < kChartMinSessionCount)
    {
        return kChartMinSessionCount;
    }
    if (session_count > kChartMaxSessionCount)
    {
        return kChartMaxSessionCount;
    }
    return session_count;
}

ChartLoadResult caughtAsStatus(const std::exception& ex)
{
    ChartLoadResult out;
    out.message = ex.what();
    out.status = isStoreBusyError(out.message) ? ChartLoadStatus::Busy : ChartLoadStatus::Error;
    return out;
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
            out.message = "Open Chart Settings to choose a symbol.";
            return out;
        }
        if (!isChartSettingsSupported(settings))
        {
            ChartLoadResult out;
            out.status = ChartLoadStatus::Unsupported;
            out.message = "candlestick bars and Days to Load only.";
            return out;
        }

        const int session_count = clampSessionCount(settings.session_count);
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

        const std::vector<CoverageDay> days = store.queryCoverageDays(id, kTimeframe1m);
        std::vector<CoverageDay> collected;
        collected.reserve(static_cast<std::size_t>(session_count));
        for (const CoverageDay& day : days)
        {
            if (day.bar_count <= 0)
            {
                continue;
            }
            collected.push_back(day);
            if (static_cast<int>(collected.size()) >= session_count)
            {
                break;
            }
        }
        if (collected.empty())
        {
            out.status = ChartLoadStatus::Empty;
            out.message = "no 1m bars for " + symbol;
            return out;
        }

        const SessionDate last_session = collected.front().session_date;
        const SessionDate first_session = collected.back().session_date;
        out.last_session = last_session;
        out.first_session = first_session;
        out.sessions_used = static_cast<int>(collected.size());
        out.ts_begin = usRthUtcWindow(tz, first_session).start;
        out.ts_end = usRthUtcWindow(tz, last_session).end;
        out.bars = store.queryBars(id, kTimeframe1m, out.ts_begin, out.ts_end);
        if (chartNeedsBarTransform(settings))
        {
            out.bars = transformChartBars(out.bars, settings.period, tz);
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

}  // namespace terminal
