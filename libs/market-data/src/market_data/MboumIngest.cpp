#include "market_data/MboumIngest.h"

#include "market_data/MboumJson.h"
#include "market_data/MboumMap.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Time.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace myapp {
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

}  // namespace myapp
