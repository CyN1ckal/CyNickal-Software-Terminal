// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/MboumIngest.h"

#include "market_data/MboumJson.h"
#include "market_data/MboumMap.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Time.h"

#include <algorithm>
#include <chrono>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

namespace terminal {
namespace {

using namespace std::chrono;

void writeHolidayComplete(Store& store, InstrumentId id, SessionDate session_date)
{
    CoverageDay row;
    row.instrument_id = id;
    row.timeframe_s = kTimeframe1m;
    row.session_date = session_date;
    row.bar_count = 0;
    row.expected_count = 0;
    row.status = CoverageStatus::Complete;
    row.source = "mboum";
    row.ingested_at = nowUtc();
    store.upsertCoverage(row);
}

void writeHttpError(Store& store, InstrumentId id, SessionDate session_date)
{
    CoverageDay row;
    if (const auto existing = store.findCoverage(id, kTimeframe1m, session_date))
    {
        row = *existing;
    }
    row.instrument_id = id;
    row.timeframe_s = kTimeframe1m;
    row.session_date = session_date;
    row.status = CoverageStatus::Error;
    row.expected_count = kUsRthExpected1m;
    row.source = "mboum";
    row.ingested_at = nowUtc();
    store.upsertCoverage(row);
}

[[nodiscard]] InstrumentId ensureInstrument(Store& store, std::string_view symbol)
{
    const auto found = store.findInstrumentsBySymbol(symbol);
    if (found.size() > 1)
    {
        throw std::runtime_error("multiple instruments named " + std::string(symbol));
    }
    if (found.size() == 1)
    {
        return found.front().id;
    }
    Instrument inst;
    inst.symbol = std::string(symbol);
    inst.asset_class = AssetClass::Equity;
    inst.currency = "USD";
    inst.timezone = "America/New_York";
    return store.upsertInstrument(inst);
}

[[nodiscard]] SessionDate dayBefore(SessionDate date)
{
    return toSessionDate(year_month_day{sys_days{sessionDateToYmd(date)} - days{1}});
}

[[nodiscard]] std::vector<CoverageDay> writeDailyRangeError(Store& store,
                                                            InstrumentId id,
                                                            SessionDate from,
                                                            SessionDate to)
{
    std::vector<CoverageDay> out;
    sys_days cursor{sessionDateToYmd(from)};
    const sys_days last{sessionDateToYmd(to)};
    for (; cursor <= last; cursor += days{1})
    {
        const weekday wd{cursor};
        if (wd == Saturday || wd == Sunday)
        {
            continue;
        }
        const year_month_day ymd{cursor};
        if (isNyseHoliday(ymd))
        {
            continue;
        }
        const SessionDate date = toSessionDate(ymd);
        CoverageDay row;
        if (const auto existing = store.findCoverage(id, kTimeframe1d, date); existing.has_value())
        {
            const CoverageDay& prior = *existing;
            if (prior.status == CoverageStatus::Complete)
            {
                continue;
            }
            row = prior;
        }
        row.instrument_id = id;
        row.timeframe_s = kTimeframe1d;
        row.session_date = date;
        row.status = CoverageStatus::Error;
        row.expected_count = kUsRthExpected1d;
        row.source = "mboum";
        row.ingested_at = nowUtc();
        store.upsertCoverage(row);
        out.push_back(row);
    }
    return out;
}

[[nodiscard]] bool dailyRangeIsComplete(Store& store,
                                        InstrumentId id,
                                        std::string_view timezone,
                                        SessionDate from,
                                        SessionDate to,
                                        UnixSeconds now)
{
    return std::ranges::all_of(nyseSessions(from, to), [&](SessionDate date) {
        if (sessionStillOpen(timezone, date, now))
        {
            return false;
        }
        const auto existing = store.findCoverage(id, kTimeframe1d, date);
        if (!existing.has_value())
        {
            return false;
        }
        const CoverageDay& row = *existing;
        return row.status == CoverageStatus::Complete;
    });
}

void emitCoverageDays(const std::vector<CoverageDay>& rows,
                      int http_status,
                      IngestSymbolResult& result,
                      const IngestDayCallback& on_day)
{
    for (const CoverageDay& row : rows)
    {
        IngestDayResult day;
        day.session_date = row.session_date;
        day.status = row.status;
        day.bar_count = row.bar_count;
        day.http_status = http_status;
        result.days.push_back(day);
        if (on_day)
        {
            on_day(day);
        }
    }
}

}  // namespace

IngestSymbolResult ingestSymbol(Store& store,
                                const HttpGet& get,
                                std::string_view symbol,
                                SessionDate from,
                                SessionDate to,
                                IngestDayCallback on_day)
{
    if (symbol.empty())
    {
        throw std::runtime_error("ingest symbol is empty");
    }
    IngestSymbolResult result;
    result.instrument_id = ensureInstrument(store, symbol);
    const auto inst = store.findInstrumentById(result.instrument_id);
    if (!inst.has_value())
    {
        throw std::runtime_error("instrument missing after upsert");
    }

    auto emit = [&](const IngestDayResult& day) {
        result.days.push_back(day);
        if (on_day)
        {
            on_day(day);
        }
    };

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
        const SessionDate session_date = toSessionDate(ymd);
        IngestDayResult day;
        day.session_date = session_date;

        if (isNyseHoliday(ymd))
        {
            writeHolidayComplete(store, result.instrument_id, session_date);
            day.status = CoverageStatus::Complete;
            day.bar_count = 0;
            emit(day);
            continue;
        }

        if (const auto existing = store.findCoverage(result.instrument_id, kTimeframe1m, session_date))
        {
            if (existing->status == CoverageStatus::Complete)
            {
                day.status = CoverageStatus::Complete;
                day.bar_count = existing->bar_count;
                emit(day);
                continue;
            }
        }

        const bool still_open = sessionStillOpen(inst->timezone, session_date, nowUtc());
        const std::string url = mboumV3HistoricalUrl(symbol, session_date);
        HttpResponse http;
        try
        {
            http = get(url);
        }
        catch (const std::exception& ex)
        {
            writeHttpError(store, result.instrument_id, session_date);
            day.status = CoverageStatus::Error;
            emit(day);
            continue;
        }
        day.http_status = http.status;

        if (http.status == 401 || http.status == 403)
        {
            throw std::runtime_error("MBoum authentication failed (HTTP " + std::to_string(http.status) + ")");
        }

        if (http.status != 200)
        {
            bool no_data = false;
            try
            {
                no_data = parseMboumV3Historical(http.body).no_data;
            }
            catch (const std::exception&)
            {
                no_data = false;
            }
            if (no_data)
            {
                const auto ingested = store.ingestSession(
                    {}, result.instrument_id, kTimeframe1m, session_date, kUsRthExpected1m, still_open);
                day.status = ingested.coverage.status;
                day.bar_count = ingested.coverage.bar_count;
                emit(day);
                continue;
            }
            writeHttpError(store, result.instrument_id, session_date);
            day.status = CoverageStatus::Error;
            emit(day);
            continue;
        }

        MboumV3Page page;
        try
        {
            page = parseMboumV3Historical(http.body);
        }
        catch (const std::exception&)
        {
            writeHttpError(store, result.instrument_id, session_date);
            day.status = CoverageStatus::Error;
            emit(day);
            continue;
        }

        if (page.splits)
        {
            writeHttpError(store, result.instrument_id, session_date);
            day.status = CoverageStatus::Error;
            emit(day);
            continue;
        }

        if (page.no_data)
        {
            const auto ingested = store.ingestSession(
                {}, result.instrument_id, kTimeframe1m, session_date, kUsRthExpected1m, still_open);
            day.status = ingested.coverage.status;
            day.bar_count = ingested.coverage.bar_count;
            emit(day);
            continue;
        }

        std::vector<Bar> bars;
        bars.reserve(page.bars.size());
        const UnixSeconds now = nowUtc();
        for (const auto& row : page.bars)
        {
            if (auto mapped = mapV3Bar(*inst, row, now))
            {
                bars.push_back(*mapped);
            }
        }
        const auto ingested = store.ingestSession(
            bars, result.instrument_id, kTimeframe1m, session_date, kUsRthExpected1m, still_open);
        day.status = ingested.coverage.status;
        day.bar_count = ingested.coverage.bar_count;
        emit(day);
    }
    return result;
}

IngestSymbolResult ingestDailySymbol(Store& store,
                                     const HttpGet& get,
                                     std::string_view symbol,
                                     SessionDate from,
                                     SessionDate to,
                                     const IngestDayCallback& on_day)
{
    if (symbol.empty())
    {
        throw std::runtime_error("ingest symbol is empty");
    }
    if (from > to)
    {
        throw std::runtime_error("ingest from is after to");
    }
    IngestSymbolResult result;
    result.instrument_id = ensureInstrument(store, symbol);
    const auto found = store.findInstrumentById(result.instrument_id);
    if (!found.has_value())
    {
        throw std::runtime_error("instrument missing after upsert");
    }
    const Instrument& inst = *found;
    const UnixSeconds now = nowUtc();

    if (dailyRangeIsComplete(store, result.instrument_id, inst.timezone, from, to, now))
    {
        sys_days cursor{sessionDateToYmd(from)};
        const sys_days last{sessionDateToYmd(to)};
        for (; cursor <= last; cursor += days{1})
        {
            const weekday wd{cursor};
            if (wd == Saturday || wd == Sunday)
            {
                continue;
            }
            const SessionDate date = toSessionDate(year_month_day{cursor});
            if (const auto existing = store.findCoverage(result.instrument_id, kTimeframe1d, date);
                existing.has_value())
            {
                emitCoverageDays({*existing}, 0, result, on_day);
            }
        }
        return result;
    }

    SessionDate end = to;
    while (end >= from)
    {
        const std::string url = mboumV3DailyUrl(symbol, from, end);
        HttpResponse http;
        try
        {
            http = get(url);
        }
        catch (const std::exception&)
        {
            emitCoverageDays(writeDailyRangeError(store, result.instrument_id, from, end),
                             0,
                             result,
                             on_day);
            break;
        }

        if (http.status == 401 || http.status == 403)
        {
            throw std::runtime_error("MBoum authentication failed (HTTP " +
                                     std::to_string(http.status) + ")");
        }

        MboumV3DailyPage page;
        bool parsed = false;
        try
        {
            page = parseMboumV3Daily(http.body);
            parsed = true;
        }
        catch (const std::exception&)
        {
            parsed = false;
        }

        if (http.status == 404 || (parsed && page.no_data))
        {
            break;
        }
        if (http.status != 200 || !parsed || page.splits)
        {
            emitCoverageDays(writeDailyRangeError(store, result.instrument_id, from, end),
                             http.status,
                             result,
                             on_day);
            break;
        }

        std::vector<Bar> bars;
        bars.reserve(page.bars.size());
        SessionDate page_from = 0;
        SessionDate page_to = 0;
        for (const auto& row : page.bars)
        {
            auto mapped = mapV3DailyBar(inst, row, now);
            if (!mapped.has_value())
            {
                continue;
            }
            const Bar bar = *mapped;
            const SessionDate session = utcToSessionDate(inst.timezone, bar.ts);
            if (page_from == 0 || session < page_from)
            {
                page_from = session;
            }
            if (page_to == 0 || session > page_to)
            {
                page_to = session;
            }
            bars.push_back(bar);
        }
        if (bars.empty())
        {
            break;
        }

        const auto ingested = store.ingestDailyRange(bars, result.instrument_id, page_from, page_to);
        emitCoverageDays(ingested.coverage, http.status, result, on_day);

        if (static_cast<int>(page.bars.size()) < kMboumDailyPageLimit)
        {
            break;
        }
        const SessionDate older = dayBefore(page_from);
        if (older < from || older >= end)
        {
            break;
        }
        end = older;
    }
    return result;
}

}  // namespace terminal
