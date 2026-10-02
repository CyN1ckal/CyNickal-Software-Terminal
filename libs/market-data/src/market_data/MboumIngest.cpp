// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/MboumIngest.h"

#include "market_data/Identity.h"
#include "market_data/MboumJson.h"
#include "market_data/MboumMap.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Time.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <map>
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

// Split events for an instrument ensureInstrument already resolved. The request uses the
// stored ticker, so a caller's lowercase or slash spelling never reaches MBoum.
[[nodiscard]] int ingestSplitsFor(Store& store, const HttpGet& get, const Instrument& instrument)
{
    int upserted = 0;
    HttpResponse http;
    try
    {
        http = get(mboumV1SplitsUrl(instrument.symbol));
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("MBoum splits request failed: ") + ex.what());
    }
    if (http.status == 401 || http.status == 403)
    {
        throw std::runtime_error("MBoum authentication failed (HTTP " + std::to_string(http.status) +
                                 ")");
    }
    if (http.status != 200)
    {
        throw std::runtime_error("MBoum splits request failed (HTTP " + std::to_string(http.status) +
                                 ")");
    }

    std::vector<MboumV1SplitEvent> events;
    try
    {
        events = parseMboumV1SplitEvents(http.body);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("MBoum splits parse failed: ") + ex.what());
    }

    for (const MboumV1SplitEvent& event : events)
    {
        const UnixSeconds from_ex = event.ex_ts > 0 ? event.ex_ts - 1 : event.ex_ts;
        const std::vector<CorporateAction> existing =
            store.queryCorporateActions(instrument.id, from_ex, event.ex_ts);
        bool blocked = false;
        for (const CorporateAction& row : existing)
        {
            if (row.type != CorporateActionType::Split || row.ex_ts != event.ex_ts)
            {
                continue;
            }
            const double stored = row.split_ratio.value_or(0.0);
            if (stored != event.split_ratio)
            {
                blocked = true;
            }
            break;
        }
        if (blocked)
        {
            continue;
        }
        CorporateAction action;
        action.instrument_id = instrument.id;
        action.ex_ts = event.ex_ts;
        action.type = CorporateActionType::Split;
        action.split_ratio = event.split_ratio;
        action.source = "mboum";
        store.upsertCorporateAction(action);
        ++upserted;
    }
    return upserted;
}

// ensureInstrument, then the stored row. MBoum requests use instrument.symbol, which is
// the canonical spelling of the ticker that was just confirmed.
[[nodiscard]] Instrument resolveForIngest(Store& store,
                                          OpenFigiClient& figi,
                                          std::string_view symbol,
                                          std::string& notice)
{
    ResolvedInstrument resolved = ensureInstrument(store, figi, symbol, nowUtc());
    notice = std::move(resolved.notice);
    auto instrument = store.findInstrumentById(resolved.id);
    if (!instrument.has_value())
    {
        throw std::runtime_error("instrument missing after upsert");
    }
    return *std::move(instrument);
}

void emitIngestDay(IngestSymbolResult& result,
                   const IngestDayCallback& on_day,
                   SessionDate session_date,
                   CoverageStatus status,
                   int bar_count,
                   int http_status)
{
    IngestDayResult day;
    day.session_date = session_date;
    day.status = status;
    day.bar_count = bar_count;
    day.http_status = http_status;
    result.days.push_back(day);
    if (on_day)
    {
        on_day(day);
    }
}

[[nodiscard]] bool minuteCoverageComplete(const Store& store, InstrumentId id, SessionDate session_date)
{
    const std::optional<CoverageDay> existing = store.findCoverage(id, kTimeframe1m, session_date);
    if (!existing.has_value())
    {
        return false;
    }
    const CoverageDay& row = existing.value();
    return row.status == CoverageStatus::Complete;
}

void emitStoredMinute(const Store& store,
                      IngestSymbolResult& result,
                      const IngestDayCallback& on_day,
                      SessionDate session_date,
                      int http_status)
{
    const std::optional<CoverageDay> existing = store.findCoverage(result.instrument_id, kTimeframe1m, session_date);
    if (!existing.has_value())
    {
        return;
    }
    const CoverageDay& row = existing.value();
    emitIngestDay(result, on_day, session_date, row.status, row.bar_count, http_status);
}

void commitMinuteSession(Store& store,
                         const Instrument& inst,
                         IngestSymbolResult& result,
                         const IngestDayCallback& on_day,
                         SessionDate session_date,
                         const std::vector<Bar>& bars,
                         int http_status)
{
    const bool still_open = sessionStillOpen(inst.timezone, session_date, nowUtc());
    if (!still_open && minuteCoverageComplete(store, result.instrument_id, session_date))
    {
        emitStoredMinute(store, result, on_day, session_date, http_status);
        return;
    }
    const auto ingested = store.ingestSession(
        bars, result.instrument_id, kTimeframe1m, session_date, kUsRthExpected1m, still_open);
    emitIngestDay(result, on_day, session_date, ingested.coverage.status, ingested.coverage.bar_count, http_status);
}

void failMinuteWindow(Store& store,
                      const Instrument& inst,
                      IngestSymbolResult& result,
                      const IngestDayCallback& on_day,
                      SessionDate from,
                      SessionDate to,
                      int http_status)
{
    for (const SessionDate session_date : nyseSessions(from, to))
    {
        if (!sessionStillOpen(inst.timezone, session_date, nowUtc()) &&
            minuteCoverageComplete(store, result.instrument_id, session_date))
        {
            emitStoredMinute(store, result, on_day, session_date, http_status);
            continue;
        }
        writeHttpError(store, result.instrument_id, session_date);
        emitIngestDay(result, on_day, session_date, CoverageStatus::Error, 0, http_status);
    }
}

// Oldest raw minute in the page. A full page cuts this session when the first
// bar is after 09:30 local, so that session is fetched again as the next end.
struct MinutePageEdge
{
    bool parsed{false};
    bool full{false};
    bool cut{false};
    SessionDate oldest{};
};

[[nodiscard]] MinutePageEdge minutePageEdge(const Instrument& inst, const MboumV3Page& page)
{
    MinutePageEdge edge;
    edge.full = page.bars.size() >= static_cast<std::size_t>(kMboumIntradayPageLimit);
    UnixSeconds oldest_ts = 0;
    for (const MboumV3BarRow& row : page.bars)
    {
        const std::optional<UnixSeconds> parsed = naiveLocalToUtc(inst.timezone, row.datetime);
        if (!parsed.has_value())
        {
            continue;
        }
        const UnixSeconds ts = parsed.value();
        if (!edge.parsed || ts < oldest_ts)
        {
            oldest_ts = ts;
            edge.oldest = utcToSessionDate(inst.timezone, ts);
            edge.parsed = true;
        }
    }
    if (edge.parsed && edge.full)
    {
        const UtcWindow rth = usRthUtcWindow(inst.timezone, edge.oldest);
        edge.cut = oldest_ts > rth.start;
    }
    return edge;
}

[[nodiscard]] std::map<SessionDate, std::vector<Bar>> groupRthBars(const Instrument& inst, const MboumV3Page& page)
{
    std::map<SessionDate, std::vector<Bar>> grouped;
    const UnixSeconds now = nowUtc();
    for (const MboumV3BarRow& row : page.bars)
    {
        const std::optional<Bar> mapped = mapV3Bar(inst, row, now);
        if (!mapped.has_value())
        {
            continue;
        }
        const Bar bar = mapped.value();
        grouped[utcToSessionDate(inst.timezone, bar.ts)].push_back(bar);
    }
    return grouped;
}

void commitMinuteSpan(Store& store,
                      const Instrument& inst,
                      IngestSymbolResult& result,
                      const IngestDayCallback& on_day,
                      SessionDate from,
                      SessionDate to,
                      const MinutePageEdge& edge,
                      const std::map<SessionDate, std::vector<Bar>>& grouped,
                      int http_status)
{
    const std::vector<Bar> empty;
    for (const SessionDate session_date : nyseSessions(from, to))
    {
        if (edge.full && edge.parsed && edge.cut && session_date <= edge.oldest)
        {
            continue;
        }
        if (edge.full && edge.parsed && !edge.cut && session_date < edge.oldest)
        {
            continue;
        }
        const auto found = grouped.find(session_date);
        const std::vector<Bar>& bars = found == grouped.end() ? empty : found->second;
        commitMinuteSession(store, inst, result, on_day, session_date, bars, http_status);
    }
}

// Four inclusive calendar dates. Extended hours are under 1000 bars a day, so a
// chunk stays near one 4000-bar page and the vendor is not asked to scan years.
[[nodiscard]] SessionDate minuteChunkStart(SessionDate from, SessionDate end)
{
    constexpr int kChunkSpanDays = 3;
    const SessionDate back =
        toSessionDate(year_month_day{sys_days{sessionDateToYmd(end)} - days{kChunkSpanDays}});
    return back < from ? from : back;
}

[[nodiscard]] bool minuteSpanComplete(const Store& store, InstrumentId id, SessionDate from, SessionDate to)
{
    return std::ranges::all_of(nyseSessions(from, to), [&](SessionDate session_date) {
        return minuteCoverageComplete(store, id, session_date);
    });
}

struct MinuteFetch
{
    HttpResponse http;
    MboumV3Page page;
    bool parsed{false};
    bool transport_error{false};
};

[[nodiscard]] MinuteFetch fetchMinutePage(const HttpGet& get, std::string_view url)
{
    MinuteFetch out;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        out = MinuteFetch{};
        try
        {
            out.http = get(url);
        }
        catch (const std::exception&)
        {
            out.transport_error = true;
            continue;
        }
        if (out.http.status == 401 || out.http.status == 403)
        {
            throw std::runtime_error("MBoum authentication failed (HTTP " + std::to_string(out.http.status) +
                                     ")");
        }
        try
        {
            out.page = parseMboumV3Historical(out.http.body);
            out.parsed = true;
        }
        catch (const std::exception&)
        {
            out.parsed = false;
        }
        const bool empty_history =
            out.http.status == 404 || (out.parsed && out.page.no_data && !out.page.fetch_failed);
        const bool retry = out.transport_error || !out.parsed || out.page.fetch_failed || empty_history;
        if (!retry)
        {
            return out;
        }
    }
    return out;
}

}  // namespace

IngestSymbolResult ingestSymbol(Store& store,
                                const HttpGet& get,
                                OpenFigiClient& figi,
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
    const Instrument inst = resolveForIngest(store, figi, symbol, result.identity_notice);
    result.instrument_id = inst.id;

    sys_days cursor{sessionDateToYmd(from)};
    const sys_days last{sessionDateToYmd(to)};
    for (; cursor <= last; cursor += days{1})
    {
        const year_month_day ymd{cursor};
        const weekday wd{cursor};
        if (wd == Saturday || wd == Sunday || !isNyseHoliday(ymd))
        {
            continue;
        }
        const SessionDate session_date = toSessionDate(ymd);
        writeHolidayComplete(store, result.instrument_id, session_date);
        emitIngestDay(result, on_day, session_date, CoverageStatus::Complete, 0, 0);
    }

    const std::vector<SessionDate> sessions = nyseSessions(from, to);
    SessionDate end = 0;
    for (const SessionDate session_date : sessions)
    {
        if (!minuteCoverageComplete(store, result.instrument_id, session_date))
        {
            end = session_date;
        }
    }
    if (end == 0)
    {
        for (const SessionDate session_date : sessions)
        {
            emitStoredMinute(store, result, on_day, session_date, 0);
        }
        return result;
    }

    // One symbol-year is a few hundred chunks. Stop rather than spin if a page never moves.
    // One empty vendor chunk is not the end of history; stop after three in a row.
    constexpr int kMaxMinutePages = 2000;
    int empty_streak = 0;
    for (int page_index = 0; page_index < kMaxMinutePages && end >= from; ++page_index)
    {
        const SessionDate start = minuteChunkStart(from, end);
        if (minuteSpanComplete(store, result.instrument_id, start, end))
        {
            if (start <= from)
            {
                break;
            }
            const SessionDate next = dayBefore(start);
            if (next >= end)
            {
                break;
            }
            end = next;
            continue;
        }

        const std::string url = mboumV3HistoricalUrl(inst.symbol, start, end, kMboumIntradayPageLimit);
        const MinuteFetch fetched = fetchMinutePage(get, url);
        const bool no_history =
            fetched.http.status == 404 || (fetched.parsed && fetched.page.no_data && !fetched.page.fetch_failed);
        if (no_history)
        {
            ++empty_streak;
            if (empty_streak >= 3 || start <= from)
            {
                break;
            }
            const SessionDate next = dayBefore(start);
            if (next >= end)
            {
                break;
            }
            end = next;
            continue;
        }
        empty_streak = 0;
        const bool bad = fetched.transport_error || !fetched.parsed || fetched.http.status != 200 ||
                         fetched.page.splits || fetched.page.fetch_failed;
        if (bad)
        {
            failMinuteWindow(store, inst, result, on_day, start, end, fetched.http.status);
            if (start <= from)
            {
                break;
            }
            const SessionDate next = dayBefore(start);
            if (next >= end)
            {
                break;
            }
            end = next;
            continue;
        }

        const MinutePageEdge edge = minutePageEdge(inst, fetched.page);
        const std::map<SessionDate, std::vector<Bar>> grouped = groupRthBars(inst, fetched.page);
        if (!edge.full)
        {
            commitMinuteSpan(store, inst, result, on_day, start, end, edge, grouped, fetched.http.status);
            if (start <= from)
            {
                break;
            }
            const SessionDate next = dayBefore(start);
            if (next >= end)
            {
                break;
            }
            end = next;
            continue;
        }
        if (!edge.parsed)
        {
            throw std::runtime_error("1-minute ingest page did not move backward");
        }
        commitMinuteSpan(store, inst, result, on_day, start, end, edge, grouped, fetched.http.status);

        const SessionDate next = edge.cut ? edge.oldest : dayBefore(edge.oldest);
        if (next >= end)
        {
            throw std::runtime_error("1-minute ingest page did not move backward");
        }
        end = next;
    }
    return result;
}

IngestSymbolResult ingestDailySymbol(Store& store,
                                     const HttpGet& get,
                                     OpenFigiClient& figi,
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
    const Instrument inst = resolveForIngest(store, figi, symbol, result.identity_notice);
    result.instrument_id = inst.id;
    const UnixSeconds now = nowUtc();
    // Split events are independent of bar coverage. A complete daily range still needs this fetch.
    // They use the instrument resolved above; resolving again could pick a different id.
    (void)ingestSplitsFor(store, get, inst);

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
        const std::string url = mboumV3DailyUrl(inst.symbol, from, end);
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

IngestSplitsResult ingestSplits(Store& store, const HttpGet& get, OpenFigiClient& figi, std::string_view symbol)
{
    if (symbol.empty())
    {
        throw std::runtime_error("ingest symbol is empty");
    }
    IngestSplitsResult result;
    const Instrument instrument = resolveForIngest(store, figi, symbol, result.identity_notice);
    result.instrument_id = instrument.id;
    result.upserted = ingestSplitsFor(store, get, instrument);
    return result;
}

IngestStatementResult ingestStatement(Store& store,
                                      const HttpGet& get,
                                      OpenFigiClient& figi,
                                      std::string_view symbol,
                                      StatementKind statement,
                                      StatementTimeframe timeframe)
{
    if (symbol.empty())
    {
        throw std::runtime_error("ingest symbol is empty");
    }
    IngestStatementResult result;
    const Instrument instrument = resolveForIngest(store, figi, symbol, result.identity_notice);
    result.instrument_id = instrument.id;

    HttpResponse http;
    try
    {
        http = get(mboumV2StatementUrl(instrument.symbol, statement, timeframe));
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("MBoum statement request failed: ") + ex.what());
    }
    if (http.status == 401 || http.status == 403)
    {
        throw std::runtime_error("MBoum authentication failed (HTTP " + std::to_string(http.status) +
                                 ")");
    }
    if (http.status != 200)
    {
        throw std::runtime_error("MBoum statement request failed (HTTP " + std::to_string(http.status) +
                                 ")");
    }

    MboumV2Statement page;
    try
    {
        page = parseMboumV2Statement(http.body);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("MBoum statement parse failed: ") + ex.what());
    }

    std::vector<StatementCell> cells;
    cells.reserve(page.cells.size());
    for (const MboumV2StatementCell& raw : page.cells)
    {
        StatementCell cell;
        cell.instrument_id = result.instrument_id;
        cell.statement = statement;
        cell.timeframe = timeframe;
        cell.line_item = raw.line_item;
        cell.period_end = raw.period_end;
        cell.value = raw.value;
        cells.push_back(std::move(cell));
    }

    StatementSnapshot snapshot;
    snapshot.instrument_id = result.instrument_id;
    snapshot.statement = statement;
    snapshot.timeframe = timeframe;
    snapshot.source = "mboum";
    snapshot.fetched_at = nowUtc();
    store.replaceStatement(snapshot, cells);
    result.cell_count = static_cast<int>(cells.size());
    result.no_data = page.no_data;
    return result;
}

namespace {

[[nodiscard]] bool sameOptionUnderlying(std::string_view requested, std::string_view base)
{
    if (requested == base)
    {
        return true;
    }
    return !base.empty() && base.front() == '$' && base.substr(1) == requested;
}

}  // namespace

IngestOptionsResult ingestOptions(Store& store,
                                  const HttpGet& get,
                                  OpenFigiClient& figi,
                                  std::string_view symbol,
                                  SessionDate expiration)
{
    if (symbol.empty() || symbol.find_first_not_of(" \t\r\n") != 0 ||
        symbol.find_last_not_of(" \t\r\n") != symbol.size() - 1)
    {
        throw std::runtime_error("ingest symbol is empty");
    }
    HttpResponse http;
    try
    {
        http = get(mboumV3OptionsUrl(symbol, expiration));
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("MBoum options request failed: ") + ex.what());
    }
    if (http.status == 401 || http.status == 403)
    {
        throw std::runtime_error("MBoum authentication failed (HTTP " + std::to_string(http.status) + ")");
    }
    if (http.status != 200)
    {
        throw std::runtime_error("MBoum options request failed (HTTP " + std::to_string(http.status) + ")");
    }

    MboumV3Options page;
    try
    {
        page = parseMboumV3Options(http.body);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("MBoum options parse failed: ") + ex.what());
    }

    IngestOptionsResult result;
    result.no_data = page.no_data;
    if (page.no_data)
    {
        return result;
    }
    const std::string canonical = page.base_symbol.empty() ? std::string(symbol) : page.base_symbol;
    if (!page.base_symbol.empty() && !sameOptionUnderlying(symbol, page.base_symbol))
    {
        throw std::runtime_error("MBoum options underlying does not match " + std::string(symbol));
    }
    result.symbol = canonical;
    {
        ResolvedInstrument resolved = ensureInstrument(store, figi, canonical, nowUtc());
        result.instrument_id = resolved.id;
        result.identity_notice = std::move(resolved.notice);
    }

    const UnixSeconds fetched_at = nowUtc();
    OptionChainWrite write;
    write.instrument_id = result.instrument_id;
    write.source = "mboum";
    write.fetched_at = fetched_at;
    write.replace_calendar = page.has_calendar;
    write.calendar.reserve(page.calendar.size());
    for (const MboumV3OptionExpiry& row : page.calendar)
    {
        OptionExpiry expiry;
        expiry.instrument_id = result.instrument_id;
        expiry.expiration = row.expiration;
        expiry.expiration_type = row.expiration_type;
        expiry.source = "mboum";
        write.calendar.push_back(std::move(expiry));
    }
    if (!page.groups.empty())
    {
        write.has_underlying = true;
        write.underlying.instrument_id = result.instrument_id;
        write.underlying.source = "mboum";
        write.underlying.fetched_at = fetched_at;
        write.underlying.historic_vol_30d = page.historic_vol_30d;
        write.underlying.iv_rank_1y = page.iv_rank_1y;
        write.underlying.next_earnings = page.next_earnings;
        write.underlying.dividend_ex = page.dividend_ex;
        write.underlying.earnings_time = page.earnings_time;
    }
    write.batches.reserve(page.groups.size());
    for (const MboumV3OptionGroup& group : page.groups)
    {
        OptionQuoteBatch batch;
        batch.expiration = group.expiration;
        batch.expiration_type = group.expiration_type;
        batch.average_iv = group.average_iv;
        batch.quotes.reserve(group.contracts.size());
        for (const MboumV3OptionContract& raw : group.contracts)
        {
            OptionQuote quote;
            quote.instrument_id = result.instrument_id;
            quote.expiration = raw.expiration;
            quote.expiration_type = raw.expiration_type;
            quote.vendor_symbol = raw.vendor_symbol;
            quote.strike = raw.strike;
            quote.right = raw.right;
            quote.bid = raw.bid;
            quote.ask = raw.ask;
            quote.mid = raw.mid;
            quote.last = raw.last;
            quote.price_change = raw.price_change;
            quote.percent_change = raw.percent_change;
            quote.volume = raw.volume;
            quote.open_interest = raw.open_interest;
            quote.open_interest_change = raw.open_interest_change;
            quote.implied_vol = raw.implied_vol;
            quote.delta = raw.delta;
            quote.rho = raw.rho;
            quote.vega = raw.vega;
            quote.theta = raw.theta;
            quote.moneyness = raw.moneyness;
            quote.days_to_expiration = raw.days_to_expiration;
            quote.trade_date = raw.trade_date;
            quote.trade_minute = raw.trade_minute;
            quote.fetched_at = fetched_at;
            batch.quotes.push_back(std::move(quote));
        }
        result.quote_count += static_cast<int>(batch.quotes.size());
        write.batches.push_back(std::move(batch));
    }
    store.replaceOptionChain(write);
    result.expiration_count = static_cast<int>(page.calendar.size());
    return result;
}

}  // namespace terminal
