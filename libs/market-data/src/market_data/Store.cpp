// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/Store.h"

#include "Sqlite.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Schema.h"
#include "market_data/Time.h"
#include "market_data/Types.h"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {

namespace {

constexpr int kWriterBusyTimeoutMs = 5000;
constexpr int kReaderBusyTimeoutMs = 0;

[[nodiscard]] std::optional<std::string> coerceExchange(std::optional<std::string_view> exchange)
{
    if (!exchange.has_value())
    {
        return std::nullopt;
    }
    const auto first = exchange->find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
    {
        return std::nullopt;
    }
    const auto last = exchange->find_last_not_of(" \t\r\n");
    return std::string(exchange->substr(first, last - first + 1));
}

void bindOptionalText(SqliteStmt& stmt, int idx, const std::optional<std::string>& value)
{
    if (!value.has_value())
    {
        stmt.bindNull(idx);
        return;
    }
    stmt.bindText(idx, *value);
}

void bindOptionalInt64(SqliteStmt& stmt, int idx, const std::optional<UnixSeconds>& value)
{
    if (!value.has_value())
    {
        stmt.bindNull(idx);
        return;
    }
    stmt.bindInt64(idx, *value);
}

void bindOptionalDouble(SqliteStmt& stmt, int idx, const std::optional<double>& value)
{
    if (!value.has_value())
    {
        stmt.bindNull(idx);
        return;
    }
    stmt.bindDouble(idx, *value);
}

[[nodiscard]] Instrument instrumentFromStmt(SqliteStmt& stmt)
{
    Instrument row;
    row.id = stmt.columnInt64(0);
    row.symbol = stmt.columnText(1);
    if (!stmt.columnIsNull(2))
    {
        row.exchange = stmt.columnText(2);
    }
    row.asset_class = assetClassFromSql(stmt.columnText(3));
    row.currency = stmt.columnText(4);
    row.timezone = stmt.columnText(5);
    if (!stmt.columnIsNull(6))
    {
        row.name = stmt.columnText(6);
    }
    if (!stmt.columnIsNull(7))
    {
        row.listed_at = stmt.columnInt64(7);
    }
    if (!stmt.columnIsNull(8))
    {
        row.delisted_at = stmt.columnInt64(8);
    }
    row.created_at = stmt.columnInt64(9);
    return row;
}

[[nodiscard]] CoverageDay coverageFromStmt(SqliteStmt& stmt)
{
    CoverageDay row;
    row.instrument_id = stmt.columnInt64(0);
    row.timeframe_s = static_cast<int>(stmt.columnInt64(1));
    row.session_date = static_cast<SessionDate>(stmt.columnInt64(2));
    if (!stmt.columnIsNull(3))
    {
        row.first_ts = stmt.columnInt64(3);
    }
    if (!stmt.columnIsNull(4))
    {
        row.last_ts = stmt.columnInt64(4);
    }
    row.bar_count = static_cast<int>(stmt.columnInt64(5));
    if (!stmt.columnIsNull(6))
    {
        row.expected_count = static_cast<int>(stmt.columnInt64(6));
    }
    row.status = coverageStatusFromSql(stmt.columnText(7));
    row.source = stmt.columnText(8);
    row.ingested_at = stmt.columnInt64(9);
    return row;
}

[[nodiscard]] CoverageStatus statusFromCounts(int bar_count,
                                              std::optional<int> expected_count,
                                              bool session_still_open)
{
    if (session_still_open)
    {
        return CoverageStatus::Partial;
    }
    if (!expected_count.has_value())
    {
        return bar_count > 0 ? CoverageStatus::Partial : CoverageStatus::Missing;
    }
    if (bar_count == *expected_count)
    {
        return CoverageStatus::Complete;
    }
    if (bar_count == 0)
    {
        return CoverageStatus::Missing;
    }
    return CoverageStatus::Partial;
}

}  // namespace

struct Store::Impl
{
    std::filesystem::path path;
    StoreMode mode{StoreMode::Writer};
    SqliteDb db;
    mutable SqliteStmt sel_instrument_symbol;
    mutable SqliteStmt sel_instruments_symbol;
    mutable SqliteStmt sel_instrument_id;
    SqliteStmt ins_instrument;
    SqliteStmt upd_instrument;
    SqliteStmt count_bars;
    SqliteStmt ins_bar;
    mutable SqliteStmt sel_bars;
    SqliteStmt ins_coverage;
    mutable SqliteStmt sel_coverage_incomplete;
    mutable SqliteStmt sel_coverage_days;
    mutable SqliteStmt sel_coverage_summary;
    mutable SqliteStmt sel_coverage_one;
    mutable SqliteStmt sel_bar_stats;
    SqliteStmt sel_corp;
    SqliteStmt ins_corp;
    SqliteStmt upd_corp;
    mutable SqliteStmt sel_corp_range;

    explicit Impl(std::filesystem::path db_path, StoreMode store_mode)
        : path(std::move(db_path)), mode(store_mode), db(path)
    {
    }

    void prepare()
    {
        sqlite3* h = db.handle();
        sel_instrument_symbol.prepare(
            h,
            "SELECT id, symbol, exchange, asset_class, currency, timezone, name, "
            "listed_at, delisted_at, created_at FROM instrument "
            "WHERE symbol = ? COLLATE NOCASE AND ifnull(exchange, '') = ifnull(?, '')");
        sel_instruments_symbol.prepare(
            h,
            "SELECT id, symbol, exchange, asset_class, currency, timezone, name, "
            "listed_at, delisted_at, created_at FROM instrument "
            "WHERE symbol = ? COLLATE NOCASE ORDER BY id");
        sel_instrument_id.prepare(
            h,
            "SELECT id, symbol, exchange, asset_class, currency, timezone, name, "
            "listed_at, delisted_at, created_at FROM instrument WHERE id = ?");
        ins_instrument.prepare(
            h,
            "INSERT INTO instrument (symbol, exchange, asset_class, currency, timezone, name, "
            "listed_at, delisted_at, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        upd_instrument.prepare(
            h,
            "UPDATE instrument SET exchange = ?, asset_class = ?, currency = ?, timezone = ?, "
            "name = ?, listed_at = ?, delisted_at = ? WHERE id = ?");
        count_bars.prepare(h, "SELECT COUNT(*) FROM bar WHERE instrument_id = ?");
        ins_bar.prepare(
            h,
            "INSERT INTO bar (instrument_id, timeframe_s, ts, open, high, low, close, volume) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT (instrument_id, timeframe_s, ts) DO UPDATE SET "
            "open = excluded.open, high = excluded.high, low = excluded.low, "
            "close = excluded.close, volume = excluded.volume");
        sel_bars.prepare(
            h,
            "SELECT ts, open, high, low, close, volume FROM bar "
            "WHERE instrument_id = ? AND timeframe_s = ? AND ts >= ? AND ts < ? ORDER BY ts");
        ins_coverage.prepare(
            h,
            "INSERT INTO coverage_day (instrument_id, timeframe_s, session_date, first_ts, last_ts, "
            "bar_count, expected_count, status, source, ingested_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT (instrument_id, timeframe_s, session_date) DO UPDATE SET "
            "first_ts = excluded.first_ts, last_ts = excluded.last_ts, "
            "bar_count = excluded.bar_count, expected_count = excluded.expected_count, "
            "status = excluded.status, source = excluded.source, ingested_at = excluded.ingested_at");
        sel_coverage_incomplete.prepare(
            h,
            "SELECT instrument_id, timeframe_s, session_date, first_ts, last_ts, "
            "bar_count, expected_count, status, source, ingested_at FROM coverage_day "
            "WHERE instrument_id = ? AND timeframe_s = ? AND status != 'complete' "
            "ORDER BY session_date");
        sel_coverage_days.prepare(
            h,
            "SELECT instrument_id, timeframe_s, session_date, first_ts, last_ts, "
            "bar_count, expected_count, status, source, ingested_at FROM coverage_day "
            "WHERE instrument_id = ? AND timeframe_s = ? ORDER BY session_date DESC");
        sel_coverage_summary.prepare(
            h,
            "SELECT i.id, i.symbol, i.exchange, i.asset_class, i.currency, i.timezone, i.name, "
            "i.listed_at, i.delisted_at, i.created_at, "
            "MIN(c.session_date), MAX(c.session_date), "
            "COALESCE(SUM(c.bar_count), 0), COUNT(c.session_date), "
            "COALESCE(SUM(CASE WHEN c.status = 'complete' THEN 1 ELSE 0 END), 0), "
            "COALESCE(SUM(CASE WHEN c.status = 'partial' THEN 1 ELSE 0 END), 0), "
            "COALESCE(SUM(CASE WHEN c.status = 'missing' THEN 1 ELSE 0 END), 0), "
            "COALESCE(SUM(CASE WHEN c.status = 'error' THEN 1 ELSE 0 END), 0), "
            "MAX(c.ingested_at) "
            "FROM instrument i "
            "LEFT JOIN coverage_day c ON c.instrument_id = i.id AND c.timeframe_s = ? "
            "GROUP BY i.id "
            "ORDER BY i.symbol COLLATE NOCASE, i.id");
        sel_coverage_one.prepare(
            h,
            "SELECT instrument_id, timeframe_s, session_date, first_ts, last_ts, "
            "bar_count, expected_count, status, source, ingested_at FROM coverage_day "
            "WHERE instrument_id = ? AND timeframe_s = ? AND session_date = ?");
        sel_bar_stats.prepare(
            h,
            "SELECT MIN(ts), MAX(ts), COUNT(*) FROM bar "
            "WHERE instrument_id = ? AND timeframe_s = ? AND ts >= ? AND ts < ?");
        sel_corp.prepare(
            h,
            "SELECT id, currency, source FROM corporate_action "
            "WHERE instrument_id = ? AND ex_ts = ? AND type = ? "
            "AND ifnull(split_ratio, 0) = ifnull(?, 0) AND ifnull(amount, 0) = ifnull(?, 0)");
        ins_corp.prepare(
            h,
            "INSERT INTO corporate_action (instrument_id, ex_ts, type, split_ratio, amount, "
            "currency, source) VALUES (?, ?, ?, ?, ?, ?, ?)");
        upd_corp.prepare(h, "UPDATE corporate_action SET currency = ?, source = ? WHERE id = ?");
        sel_corp_range.prepare(
            h,
            "SELECT id, instrument_id, ex_ts, type, split_ratio, amount, currency, source "
            "FROM corporate_action WHERE instrument_id = ? AND ex_ts > ? AND ex_ts <= ? "
            "ORDER BY ex_ts");
    }
};

Store::Store(std::filesystem::path db_path, StoreMode mode)
    : impl_(std::make_unique<Impl>(std::move(db_path), mode))
{
    const int timeout = mode == StoreMode::Writer ? kWriterBusyTimeoutMs : kReaderBusyTimeoutMs;
    impl_->db.applyConnectionPragmas(timeout);
    const int version = impl_->db.userVersion();
    if (version > kSchemaUserVersion)
    {
        throw std::runtime_error("database user_version exceeds this binary");
    }
    if (version == 0)
    {
        SqliteTxn txn(impl_->db.handle());
        impl_->db.exec(schemaV1());
        impl_->db.setUserVersion(kSchemaUserVersion);
        txn.commit();
    }
    const auto tables = tableNames();
    constexpr std::string_view required[] = {"bar", "corporate_action", "coverage_day", "instrument"};
    for (const auto want : required)
    {
        bool found = false;
        for (const auto& name : tables)
        {
            if (name == want)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            throw std::runtime_error("database user_version is " + std::to_string(userVersion()) +
                                     " but missing required table '" + std::string(want) + "'");
        }
    }
    impl_->prepare();
}

Store::~Store() = default;

const std::filesystem::path& Store::path() const noexcept
{
    return impl_->path;
}

int Store::userVersion() const
{
    return impl_->db.userVersion();
}

std::vector<std::string> Store::tableNames() const
{
    SqliteStmt stmt(impl_->db.handle(),
                    "SELECT name FROM sqlite_master WHERE type = 'table' "
                    "AND name NOT LIKE 'sqlite_%' ORDER BY name");
    std::vector<std::string> names;
    while (stmt.stepRow())
    {
        names.push_back(stmt.columnText(0));
    }
    stmt.reset();
    return names;
}

bool Store::foreignKeysEnabled() const
{
    SqliteStmt stmt(impl_->db.handle(), "PRAGMA foreign_keys");
    if (!stmt.stepRow())
    {
        throw std::runtime_error("PRAGMA foreign_keys returned no row");
    }
    const bool enabled = stmt.columnInt64(0) != 0;
    stmt.reset();
    return enabled;
}

void Store::testingSetUserVersion(const std::filesystem::path& path, int version)
{
    SqliteDb db(path);
    db.setUserVersion(version);
}

InstrumentId Store::upsertInstrument(const Instrument& instrument)
{
    const auto exchange = coerceExchange(
        instrument.exchange.has_value() ? std::optional<std::string_view>(*instrument.exchange)
                                        : std::nullopt);
    SqliteTxn txn(impl_->db.handle());
    auto& sel = impl_->sel_instrument_symbol;
    sel.reset();
    sel.bindText(1, instrument.symbol);
    if (exchange.has_value())
    {
        sel.bindText(2, *exchange);
    }
    else
    {
        sel.bindNull(2);
    }
    std::optional<Instrument> existing;
    if (sel.stepRow())
    {
        existing = instrumentFromStmt(sel);
    }
    sel.reset();

    if (existing.has_value())
    {
        if (instrument.timezone != existing->timezone)
        {
            impl_->count_bars.reset();
            impl_->count_bars.bindInt64(1, existing->id);
            if (!impl_->count_bars.stepRow())
            {
                impl_->count_bars.reset();
                throw std::runtime_error("COUNT(*) returned no row");
            }
            const auto n = impl_->count_bars.columnInt64(0);
            impl_->count_bars.reset();
            if (n > 0)
            {
                throw std::runtime_error("cannot change timezone after bars exist");
            }
        }
        auto& upd = impl_->upd_instrument;
        upd.reset();
        if (exchange.has_value())
        {
            upd.bindText(1, *exchange);
        }
        else
        {
            upd.bindNull(1);
        }
        upd.bindText(2, toSql(instrument.asset_class));
        upd.bindText(3, instrument.currency);
        upd.bindText(4, instrument.timezone);
        bindOptionalText(upd, 5, instrument.name);
        bindOptionalInt64(upd, 6, instrument.listed_at);
        bindOptionalInt64(upd, 7, instrument.delisted_at);
        upd.bindInt64(8, existing->id);
        upd.stepDone();
        upd.reset();
        txn.commit();
        return existing->id;
    }

    auto& ins = impl_->ins_instrument;
    ins.reset();
    ins.bindText(1, instrument.symbol);
    if (exchange.has_value())
    {
        ins.bindText(2, *exchange);
    }
    else
    {
        ins.bindNull(2);
    }
    ins.bindText(3, toSql(instrument.asset_class));
    ins.bindText(4, instrument.currency.empty() ? "USD" : instrument.currency);
    ins.bindText(5, instrument.timezone.empty() ? "America/New_York" : instrument.timezone);
    bindOptionalText(ins, 6, instrument.name);
    bindOptionalInt64(ins, 7, instrument.listed_at);
    bindOptionalInt64(ins, 8, instrument.delisted_at);
    ins.bindInt64(9, instrument.created_at != 0 ? instrument.created_at : nowUtc());
    ins.stepDone();
    const auto id = impl_->db.lastInsertRowid();
    ins.reset();
    txn.commit();
    return id;
}

std::optional<Instrument> Store::findInstrument(std::string_view symbol,
                                                std::optional<std::string_view> exchange) const
{
    const auto coerced = coerceExchange(exchange);
    auto& sel = impl_->sel_instrument_symbol;
    sel.reset();
    sel.bindText(1, symbol);
    if (coerced.has_value())
    {
        sel.bindText(2, *coerced);
    }
    else
    {
        sel.bindNull(2);
    }
    std::optional<Instrument> row;
    if (sel.stepRow())
    {
        row = instrumentFromStmt(sel);
    }
    sel.reset();
    return row;
}

std::vector<Instrument> Store::findInstrumentsBySymbol(std::string_view symbol) const
{
    auto& sel = impl_->sel_instruments_symbol;
    sel.reset();
    sel.bindText(1, symbol);
    std::vector<Instrument> rows;
    while (sel.stepRow())
    {
        rows.push_back(instrumentFromStmt(sel));
    }
    sel.reset();
    return rows;
}

std::optional<Instrument> Store::findInstrumentById(InstrumentId id) const
{
    auto& sel = impl_->sel_instrument_id;
    sel.reset();
    sel.bindInt64(1, id);
    std::optional<Instrument> row;
    if (sel.stepRow())
    {
        row = instrumentFromStmt(sel);
    }
    sel.reset();
    return row;
}

UpsertBarsResult Store::upsertBars(std::span<const Bar> bars)
{
    SqliteTxn txn(impl_->db.handle());
    auto result = upsertBarsUnlocked(bars, nowUtc(), nullptr, 0, 0, std::nullopt);
    txn.commit();
    return result;
}

std::vector<Bar> Store::queryBars(InstrumentId id,
                                  int timeframe_s,
                                  UnixSeconds ts_begin,
                                  UnixSeconds ts_end) const
{
    auto& sel = impl_->sel_bars;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindInt(2, timeframe_s);
    sel.bindInt64(3, ts_begin);
    sel.bindInt64(4, ts_end);
    std::vector<Bar> out;
    while (sel.stepRow())
    {
        Bar bar;
        bar.instrument_id = id;
        bar.timeframe_s = timeframe_s;
        bar.ts = sel.columnInt64(0);
        bar.open = sel.columnDouble(1);
        bar.high = sel.columnDouble(2);
        bar.low = sel.columnDouble(3);
        bar.close = sel.columnDouble(4);
        bar.volume = sel.columnDouble(5);
        out.push_back(bar);
    }
    sel.reset();
    return out;
}

void Store::upsertCoverage(const CoverageDay& row)
{
    SqliteTxn txn(impl_->db.handle());
    auto& ins = impl_->ins_coverage;
    ins.reset();
    ins.bindInt64(1, row.instrument_id);
    ins.bindInt(2, row.timeframe_s);
    ins.bindInt(3, row.session_date);
    bindOptionalInt64(ins, 4, row.first_ts);
    bindOptionalInt64(ins, 5, row.last_ts);
    ins.bindInt(6, row.bar_count);
    if (row.expected_count.has_value())
    {
        ins.bindInt(7, *row.expected_count);
    }
    else
    {
        ins.bindNull(7);
    }
    ins.bindText(8, toSql(row.status));
    ins.bindText(9, row.source.empty() ? "mboum" : row.source);
    ins.bindInt64(10, row.ingested_at != 0 ? row.ingested_at : nowUtc());
    ins.stepDone();
    ins.reset();
    txn.commit();
}

std::vector<CoverageDay> Store::queryIncompleteCoverage(InstrumentId id, int timeframe_s) const
{
    auto& sel = impl_->sel_coverage_incomplete;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindInt(2, timeframe_s);
    std::vector<CoverageDay> out;
    while (sel.stepRow())
    {
        out.push_back(coverageFromStmt(sel));
    }
    sel.reset();
    return out;
}

std::vector<CoverageDay> Store::queryCoverageDays(InstrumentId id, int timeframe_s) const
{
    auto& sel = impl_->sel_coverage_days;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindInt(2, timeframe_s);
    std::vector<CoverageDay> out;
    while (sel.stepRow())
    {
        out.push_back(coverageFromStmt(sel));
    }
    sel.reset();
    return out;
}

std::vector<CoverageSummary> Store::queryCoverageSummaries(int timeframe_s) const
{
    auto& sel = impl_->sel_coverage_summary;
    sel.reset();
    sel.bindInt(1, timeframe_s);
    std::vector<CoverageSummary> out;
    while (sel.stepRow())
    {
        CoverageSummary row;
        row.instrument = instrumentFromStmt(sel);
        row.timeframe_s = timeframe_s;
        if (!sel.columnIsNull(10))
        {
            row.first_session = static_cast<SessionDate>(sel.columnInt64(10));
        }
        if (!sel.columnIsNull(11))
        {
            row.last_session = static_cast<SessionDate>(sel.columnInt64(11));
        }
        row.bar_count = static_cast<int>(sel.columnInt64(12));
        row.session_count = static_cast<int>(sel.columnInt64(13));
        row.complete_count = static_cast<int>(sel.columnInt64(14));
        row.partial_count = static_cast<int>(sel.columnInt64(15));
        row.missing_count = static_cast<int>(sel.columnInt64(16));
        row.error_count = static_cast<int>(sel.columnInt64(17));
        if (!sel.columnIsNull(18))
        {
            row.last_ingested_at = sel.columnInt64(18);
        }
        out.push_back(std::move(row));
    }
    sel.reset();
    return out;
}

std::optional<CoverageDay> Store::findCoverage(InstrumentId id,
                                               int timeframe_s,
                                               SessionDate session_date) const
{
    auto& sel = impl_->sel_coverage_one;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindInt(2, timeframe_s);
    sel.bindInt(3, session_date);
    std::optional<CoverageDay> row;
    if (sel.stepRow())
    {
        row = coverageFromStmt(sel);
    }
    sel.reset();
    return row;
}

CoverageDay Store::refreshCoverageFromBars(InstrumentId id,
                                           int timeframe_s,
                                           SessionDate session_date,
                                           std::optional<int> expected_count,
                                           bool session_still_open)
{
    SqliteTxn txn(impl_->db.handle());
    auto row = refreshCoverageFromBarsUnlocked(
        id, timeframe_s, session_date, expected_count, session_still_open);
    txn.commit();
    return row;
}

IngestSessionResult Store::ingestSession(std::span<const Bar> bars,
                                         InstrumentId id,
                                         int timeframe_s,
                                         SessionDate session_date,
                                         std::optional<int> expected_count,
                                         bool session_still_open)
{
    const auto inst = findInstrumentById(id);
    if (!inst.has_value())
    {
        throw std::runtime_error("ingestSession: unknown instrument");
    }
    SqliteTxn txn(impl_->db.handle());
    IngestSessionResult result;
    result.bars = upsertBarsUnlocked(bars, nowUtc(), &*inst, timeframe_s, session_date, expected_count);
    result.coverage = refreshCoverageFromBarsUnlocked(
        id, timeframe_s, session_date, expected_count, session_still_open);
    txn.commit();
    return result;
}

IngestDailyRangeResult Store::ingestDailyRange(std::span<const Bar> bars,
                                               InstrumentId id,
                                               SessionDate from,
                                               SessionDate to)
{
    if (from > to)
    {
        throw std::runtime_error("ingestDailyRange from is after to");
    }
    const auto found = findInstrumentById(id);
    if (!found.has_value())
    {
        throw std::runtime_error("ingestDailyRange: unknown instrument");
    }
    const Instrument& instrument = *found;
    using namespace std::chrono;
    const UnixSeconds now = nowUtc();
    SqliteTxn txn(impl_->db.handle());
    IngestDailyRangeResult result;
    result.bars = upsertBarsUnlocked(
        bars, now, &instrument, kTimeframe1d, from, kUsRthExpected1d, to);

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
        const SessionDate date = toSessionDate(ymd);
        const bool holiday = isNyseHoliday(ymd);
        const std::optional<int> expected =
            holiday ? std::optional<int>{0} : std::optional<int>{kUsRthExpected1d};
        const bool still_open = !holiday && sessionStillOpen(instrument.timezone, date, now);
        result.coverage.push_back(
            refreshCoverageFromBarsUnlocked(id, kTimeframe1d, date, expected, still_open));
    }
    txn.commit();
    return result;
}

UpsertBarsResult Store::upsertBarsUnlocked(std::span<const Bar> bars,
                                           UnixSeconds now,
                                           const Instrument* session_filter,
                                           int timeframe_s,
                                           SessionDate session_date,
                                           std::optional<int> expected_count,
                                           std::optional<SessionDate> session_date_end)
{
    UpsertBarsResult result;
    const bool filter_rth = session_filter != nullptr && expected_count == kUsRthExpected1m;
    UtcWindow day_window{};
    if (session_filter != nullptr)
    {
        day_window = sessionUtcWindow(session_filter->timezone, session_date);
        if (session_date_end.has_value())
        {
            day_window.end = sessionUtcWindow(session_filter->timezone, *session_date_end).end;
        }
    }
    const UtcWindow rth_window =
        filter_rth ? usRthUtcWindow(session_filter->timezone, session_date) : UtcWindow{};
    auto& ins = impl_->ins_bar;
    for (const Bar& bar : bars)
    {
        if (isFormingBar(bar, now))
        {
            continue;
        }
        if (!isValidBar(bar))
        {
            ++result.rejected;
            continue;
        }
        if (session_filter != nullptr)
        {
            if (bar.instrument_id != session_filter->id || bar.timeframe_s != timeframe_s)
            {
                ++result.rejected;
                continue;
            }
            if (bar.ts < day_window.start || bar.ts >= day_window.end)
            {
                ++result.rejected;
                continue;
            }
            if (filter_rth && (bar.ts < rth_window.start || bar.ts >= rth_window.end))
            {
                ++result.rejected;
                continue;
            }
        }
        ins.reset();
        ins.bindInt64(1, bar.instrument_id);
        ins.bindInt(2, bar.timeframe_s);
        ins.bindInt64(3, bar.ts);
        ins.bindDouble(4, bar.open);
        ins.bindDouble(5, bar.high);
        ins.bindDouble(6, bar.low);
        ins.bindDouble(7, bar.close);
        ins.bindDouble(8, bar.volume);
        const auto rc = ins.stepDoneOrConstraint();
        ins.reset();
        if (rc == SqliteStmt::Constraint::ForeignKey)
        {
            throw std::runtime_error("bar instrument_id is not in instrument");
        }
        if (rc == SqliteStmt::Constraint::Check)
        {
            ++result.rejected;
            continue;
        }
        ++result.written;
    }
    return result;
}

CoverageDay Store::refreshCoverageFromBarsUnlocked(InstrumentId id,
                                                   int timeframe_s,
                                                   SessionDate session_date,
                                                   std::optional<int> expected_count,
                                                   bool session_still_open)
{
    const auto inst = findInstrumentById(id);
    if (!inst.has_value())
    {
        throw std::runtime_error("refreshCoverageFromBars: unknown instrument");
    }
    const UtcWindow window = sessionUtcWindow(inst->timezone, session_date);
    auto& sel = impl_->sel_bar_stats;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindInt(2, timeframe_s);
    sel.bindInt64(3, window.start);
    sel.bindInt64(4, window.end);
    if (!sel.stepRow())
    {
        sel.reset();
        throw std::runtime_error("bar stats query returned no row");
    }
    CoverageDay row;
    row.instrument_id = id;
    row.timeframe_s = timeframe_s;
    row.session_date = session_date;
    row.bar_count = static_cast<int>(sel.columnInt64(2));
    if (row.bar_count > 0)
    {
        row.first_ts = sel.columnInt64(0);
        row.last_ts = sel.columnInt64(1);
    }
    sel.reset();
    row.expected_count = expected_count;
    row.status = statusFromCounts(row.bar_count, expected_count, session_still_open);
    row.source = "mboum";
    row.ingested_at = nowUtc();

    auto& ins = impl_->ins_coverage;
    ins.reset();
    ins.bindInt64(1, row.instrument_id);
    ins.bindInt(2, row.timeframe_s);
    ins.bindInt(3, row.session_date);
    bindOptionalInt64(ins, 4, row.first_ts);
    bindOptionalInt64(ins, 5, row.last_ts);
    ins.bindInt(6, row.bar_count);
    if (row.expected_count.has_value())
    {
        ins.bindInt(7, *row.expected_count);
    }
    else
    {
        ins.bindNull(7);
    }
    ins.bindText(8, toSql(row.status));
    ins.bindText(9, row.source);
    ins.bindInt64(10, row.ingested_at);
    ins.stepDone();
    ins.reset();
    return row;
}

void Store::upsertCorporateAction(const CorporateAction& action)
{
    SqliteTxn txn(impl_->db.handle());
    auto& sel = impl_->sel_corp;
    sel.reset();
    sel.bindInt64(1, action.instrument_id);
    sel.bindInt64(2, action.ex_ts);
    sel.bindText(3, toSql(action.type));
    bindOptionalDouble(sel, 4, action.split_ratio);
    bindOptionalDouble(sel, 5, action.amount);
    if (sel.stepRow())
    {
        const auto id = sel.columnInt64(0);
        sel.reset();
        auto& upd = impl_->upd_corp;
        upd.reset();
        bindOptionalText(upd, 1, action.currency);
        upd.bindText(2, action.source.empty() ? "mboum" : action.source);
        upd.bindInt64(3, id);
        upd.stepDone();
        upd.reset();
        txn.commit();
        return;
    }
    sel.reset();
    auto& ins = impl_->ins_corp;
    ins.reset();
    ins.bindInt64(1, action.instrument_id);
    ins.bindInt64(2, action.ex_ts);
    ins.bindText(3, toSql(action.type));
    bindOptionalDouble(ins, 4, action.split_ratio);
    bindOptionalDouble(ins, 5, action.amount);
    bindOptionalText(ins, 6, action.currency);
    ins.bindText(7, action.source.empty() ? "mboum" : action.source);
    ins.stepDone();
    ins.reset();
    txn.commit();
}

std::vector<CorporateAction> Store::queryCorporateActions(InstrumentId id,
                                                           UnixSeconds from_ex_ts,
                                                           UnixSeconds to_ex_ts) const
{
    auto& sel = impl_->sel_corp_range;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindInt64(2, from_ex_ts);
    sel.bindInt64(3, to_ex_ts);
    std::vector<CorporateAction> out;
    while (sel.stepRow())
    {
        CorporateAction row;
        row.id = sel.columnInt64(0);
        row.instrument_id = sel.columnInt64(1);
        row.ex_ts = sel.columnInt64(2);
        row.type = corporateActionTypeFromSql(sel.columnText(3));
        if (!sel.columnIsNull(4))
        {
            row.split_ratio = sel.columnDouble(4);
        }
        if (!sel.columnIsNull(5))
        {
            row.amount = sel.columnDouble(5);
        }
        if (!sel.columnIsNull(6))
        {
            row.currency = sel.columnText(6);
        }
        row.source = sel.columnText(7);
        out.push_back(row);
    }
    sel.reset();
    return out;
}

}  // namespace terminal


