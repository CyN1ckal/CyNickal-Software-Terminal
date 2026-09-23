// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/Store.h"

#include "Sqlite.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Schema.h"
#include "market_data/Time.h"
#include "market_data/Types.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

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

template <typename T>
void bindOptionalInt(SqliteStmt& stmt, int idx, const std::optional<T>& value)
{
    if (!value.has_value())
    {
        stmt.bindNull(idx);
        return;
    }
    stmt.bindInt(idx, static_cast<int>(*value));
}

[[nodiscard]] bool isSessionDate(SessionDate date)
{
    using namespace std::chrono;
    if (date < 19000101 || date > 21001231)
    {
        return false;
    }
    const int year_n = date / 10000;
    const int month_n = (date / 100) % 100;
    const int day_n = date % 100;
    const year_month_day ymd{year{year_n}, month{static_cast<unsigned>(month_n)},
                             day{static_cast<unsigned>(day_n)}};
    return ymd.ok();
}

[[nodiscard]] bool isTrimmedNonEmpty(std::string_view text)
{
    if (text.empty())
    {
        return false;
    }
    const auto first = text.find_first_not_of(" \t\r\n");
    const auto last = text.find_last_not_of(" \t\r\n");
    return first == 0 && last == text.size() - 1;
}

[[nodiscard]] bool isStatementPeriodEnd(std::string_view period)
{
    if (period == "TTM")
    {
        return true;
    }
    if (period.size() != 10 || period[4] != '-' || period[7] != '-')
    {
        return false;
    }
    for (const std::size_t index : {0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U})
    {
        const char ch = period[index];
        if (ch < '0' || ch > '9')
        {
            return false;
        }
    }
    const int month = ((period[5] - '0') * 10) + (period[6] - '0');
    const int day = ((period[8] - '0') * 10) + (period[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

void bindStatementValue(SqliteStmt& stmt, int kind_idx, const StatementValue& value)
{
    if (const auto* integer = std::get_if<std::int64_t>(&value))
    {
        stmt.bindText(kind_idx, "int");
        stmt.bindInt64(kind_idx + 1, *integer);
        stmt.bindNull(kind_idx + 2);
        stmt.bindNull(kind_idx + 3);
        return;
    }
    if (const auto* real = std::get_if<double>(&value))
    {
        if (!std::isfinite(*real))
        {
            throw std::runtime_error("statement cell real is not finite");
        }
        stmt.bindText(kind_idx, "real");
        stmt.bindNull(kind_idx + 1);
        stmt.bindDouble(kind_idx + 2, *real);
        stmt.bindNull(kind_idx + 3);
        return;
    }
    if (const auto* text = std::get_if<std::string>(&value))
    {
        if (text->empty())
        {
            throw std::runtime_error("statement cell text is empty");
        }
        stmt.bindText(kind_idx, "text");
        stmt.bindNull(kind_idx + 1);
        stmt.bindNull(kind_idx + 2);
        stmt.bindText(kind_idx + 3, *text);
        return;
    }
    throw std::runtime_error("statement cell has no value");
}

[[nodiscard]] StatementCell statementCellFromStmt(SqliteStmt const& stmt)
{
    StatementCell cell;
    cell.instrument_id = stmt.columnInt64(0);
    cell.statement = statementKindFromSql(stmt.columnText(1));
    cell.timeframe = statementTimeframeFromSql(stmt.columnText(2));
    cell.line_item = stmt.columnText(3);
    cell.period_end = stmt.columnText(4);
    const auto kind = stmt.columnText(5);
    if (kind == "int")
    {
        cell.value = stmt.columnInt64(6);
    }
    else if (kind == "real")
    {
        cell.value = stmt.columnDouble(7);
    }
    else if (kind == "text")
    {
        cell.value = stmt.columnText(8);
    }
    else
    {
        throw std::runtime_error("unknown statement value_kind");
    }
    return cell;
}

[[nodiscard]] std::vector<StatementCell> collectStatementCells(SqliteStmt& sel)
{
    std::vector<StatementCell> out;
    while (sel.stepRow())
    {
        out.push_back(statementCellFromStmt(sel));
    }
    sel.reset();
    return out;
}

void requireTables(const std::vector<std::string>& have,
                   int version,
                   std::span<const std::string_view> required)
{
    for (const auto want : required)
    {
        bool found = false;
        for (const auto& name : have)
        {
            if (name == want)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            throw std::runtime_error("database user_version is " + std::to_string(version) +
                                     " but missing required table '" + std::string(want) + "'");
        }
    }
}

[[nodiscard]] Instrument instrumentFromStmt(SqliteStmt const& stmt)
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

[[nodiscard]] CoverageDay coverageFromStmt(SqliteStmt const& stmt)
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

void requireFinite(double value, const char* what)
{
    if (!std::isfinite(value))
    {
        throw std::runtime_error(std::string(what) + " is not finite");
    }
}

void validateOptionQuote(const OptionQuote& quote, const OptionQuoteBatch& batch, InstrumentId instrument_id)
{
    if (quote.instrument_id != instrument_id || quote.expiration != batch.expiration ||
        quote.expiration_type != batch.expiration_type)
    {
        throw std::runtime_error("option quote does not match its expiration");
    }
    if (!isSessionDate(quote.expiration) || !isTrimmedNonEmpty(quote.vendor_symbol))
    {
        throw std::runtime_error("option quote identity is invalid");
    }
    requireFinite(quote.strike, "option strike");
    requireFinite(quote.bid, "option bid");
    requireFinite(quote.ask, "option ask");
    requireFinite(quote.mid, "option mid");
    requireFinite(quote.last, "option last");
    requireFinite(quote.price_change, "option price change");
    requireFinite(quote.percent_change, "option percent change");
    requireFinite(quote.implied_vol, "option implied vol");
    requireFinite(quote.delta, "option delta");
    requireFinite(quote.rho, "option rho");
    requireFinite(quote.vega, "option vega");
    requireFinite(quote.theta, "option theta");
    requireFinite(quote.moneyness, "option moneyness");
    if (quote.strike <= 0.0 || quote.bid < 0.0 || quote.ask < 0.0 || quote.mid < 0.0 || quote.last < 0.0 ||
        quote.implied_vol < 0.0 || quote.volume < 0 || quote.open_interest < 0 || quote.days_to_expiration < 0)
    {
        throw std::runtime_error("option quote is out of range");
    }
    if (quote.trade_date.has_value() && quote.trade_minute.has_value())
    {
        throw std::runtime_error("option quote has both a trade date and a trade clock");
    }
    if (quote.trade_date.has_value() && !isSessionDate(*quote.trade_date))
    {
        throw std::runtime_error("option quote trade date is invalid");
    }
    if (quote.trade_minute.has_value() && (*quote.trade_minute < 0 || *quote.trade_minute >= 1440))
    {
        throw std::runtime_error("option quote trade clock is invalid");
    }
}

[[nodiscard]] OptionQuote optionQuoteFromStmt(SqliteStmt const& stmt)
{
    OptionQuote row;
    row.instrument_id = stmt.columnInt64(0);
    row.expiration = static_cast<SessionDate>(stmt.columnInt64(1));
    row.expiration_type = optionExpirationTypeFromSql(stmt.columnText(2));
    row.vendor_symbol = stmt.columnText(3);
    row.strike = stmt.columnDouble(4);
    row.right = optionRightFromSql(stmt.columnText(5));
    row.bid = stmt.columnDouble(6);
    row.ask = stmt.columnDouble(7);
    row.mid = stmt.columnDouble(8);
    row.last = stmt.columnDouble(9);
    row.price_change = stmt.columnDouble(10);
    row.percent_change = stmt.columnDouble(11);
    row.volume = stmt.columnInt64(12);
    row.open_interest = stmt.columnInt64(13);
    row.open_interest_change = stmt.columnInt64(14);
    row.implied_vol = stmt.columnDouble(15);
    row.delta = stmt.columnDouble(16);
    row.rho = stmt.columnDouble(17);
    row.vega = stmt.columnDouble(18);
    row.theta = stmt.columnDouble(19);
    row.moneyness = stmt.columnDouble(20);
    row.days_to_expiration = static_cast<int>(stmt.columnInt64(21));
    if (!stmt.columnIsNull(22))
    {
        row.trade_date = static_cast<SessionDate>(stmt.columnInt64(22));
    }
    if (!stmt.columnIsNull(23))
    {
        row.trade_minute = static_cast<int>(stmt.columnInt64(23));
    }
    row.fetched_at = stmt.columnInt64(24);
    return row;
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
    SqliteStmt upsert_statement_snapshot;
    SqliteStmt del_statement_cells;
    SqliteStmt ins_statement_cell;
    mutable SqliteStmt sel_statement_snapshot;
    mutable SqliteStmt sel_statement_cells;
    mutable SqliteStmt sel_statement_line;
    mutable SqliteStmt sel_statement_period;
    SqliteStmt upsert_option_underlying;
    SqliteStmt upsert_option_expiry;
    SqliteStmt insert_option_expiry;
    SqliteStmt del_option_quotes;
    SqliteStmt del_option_expiry;
    SqliteStmt ins_option_quote;
    mutable SqliteStmt sel_option_expiries;
    mutable SqliteStmt sel_option_underlying;
    mutable SqliteStmt sel_option_quotes;

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
        upsert_statement_snapshot.prepare(
            h,
            "INSERT INTO statement_snapshot "
            "(instrument_id, statement, timeframe, source, fetched_at) "
            "VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT (instrument_id, statement, timeframe) DO UPDATE SET "
            "source = excluded.source, fetched_at = excluded.fetched_at");
        del_statement_cells.prepare(
            h,
            "DELETE FROM statement_cell "
            "WHERE instrument_id = ? AND statement = ? AND timeframe = ?");
        ins_statement_cell.prepare(
            h,
            "INSERT INTO statement_cell "
            "(instrument_id, statement, timeframe, line_item, period_end, "
            "value_kind, value_int, value_real, value_text) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        sel_statement_snapshot.prepare(
            h,
            "SELECT source, fetched_at FROM statement_snapshot "
            "WHERE instrument_id = ? AND statement = ? AND timeframe = ?");
        constexpr std::string_view kCellColumns =
            "instrument_id, statement, timeframe, line_item, period_end, "
            "value_kind, value_int, value_real, value_text";
        const std::string cell_order =
            "ORDER BY line_item, CASE period_end WHEN 'TTM' THEN 1 ELSE 0 END, period_end";
        sel_statement_cells.prepare(
            h,
            "SELECT " + std::string(kCellColumns) + " FROM statement_cell "
            "WHERE instrument_id = ? AND statement = ? AND timeframe = ? " + cell_order);
        sel_statement_line.prepare(
            h,
            "SELECT " + std::string(kCellColumns) + " FROM statement_cell "
            "WHERE instrument_id = ? AND statement = ? AND timeframe = ? AND line_item = ? "
            "ORDER BY CASE period_end WHEN 'TTM' THEN 1 ELSE 0 END, period_end");
        sel_statement_period.prepare(
            h,
            "SELECT " + std::string(kCellColumns) + " FROM statement_cell "
            "WHERE instrument_id = ? AND statement = ? AND timeframe = ? AND period_end = ? "
            "ORDER BY line_item");
        upsert_option_underlying.prepare(
            h,
            "INSERT INTO option_underlying "
            "(instrument_id, source, fetched_at, historic_vol_30d, iv_rank_1y, "
            "next_earnings, dividend_ex, earnings_time) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT (instrument_id) DO UPDATE SET "
            "source = excluded.source, fetched_at = excluded.fetched_at, "
            "historic_vol_30d = excluded.historic_vol_30d, iv_rank_1y = excluded.iv_rank_1y, "
            "next_earnings = excluded.next_earnings, dividend_ex = excluded.dividend_ex, "
            "earnings_time = excluded.earnings_time");
        upsert_option_expiry.prepare(
            h,
            "INSERT INTO option_expiry "
            "(instrument_id, expiration, expiration_type, average_iv, fetched_at, source) "
            "VALUES (?, ?, ?, ?, ?, ?) "
            "ON CONFLICT (instrument_id, expiration, expiration_type) DO UPDATE SET "
            "average_iv = excluded.average_iv, fetched_at = excluded.fetched_at, "
            "source = excluded.source");
        insert_option_expiry.prepare(
            h,
            "INSERT INTO option_expiry "
            "(instrument_id, expiration, expiration_type, average_iv, fetched_at, source) "
            "VALUES (?, ?, ?, NULL, NULL, ?) "
            "ON CONFLICT (instrument_id, expiration, expiration_type) DO NOTHING");
        del_option_quotes.prepare(
            h,
            "DELETE FROM option_quote "
            "WHERE instrument_id = ? AND expiration = ? AND expiration_type = ?");
        del_option_expiry.prepare(
            h,
            "DELETE FROM option_expiry "
            "WHERE instrument_id = ? AND expiration = ? AND expiration_type = ?");
        ins_option_quote.prepare(
            h,
            "INSERT INTO option_quote ("
            "instrument_id, expiration, expiration_type, vendor_symbol, strike, right, "
            "bid, ask, mid, last, price_change, percent_change, volume, open_interest, "
            "open_interest_change, implied_vol, delta, rho, vega, theta, moneyness, "
            "days_to_expiration, trade_date, trade_minute, fetched_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        sel_option_expiries.prepare(
            h,
            "SELECT expiration, expiration_type, average_iv, fetched_at, source "
            "FROM option_expiry WHERE instrument_id = ? ORDER BY expiration, expiration_type");
        sel_option_underlying.prepare(
            h,
            "SELECT source, fetched_at, historic_vol_30d, iv_rank_1y, next_earnings, "
            "dividend_ex, earnings_time FROM option_underlying WHERE instrument_id = ?");
        sel_option_quotes.prepare(
            h,
            "SELECT instrument_id, expiration, expiration_type, vendor_symbol, strike, right, "
            "bid, ask, mid, last, price_change, percent_change, volume, open_interest, "
            "open_interest_change, implied_vol, delta, rho, vega, theta, moneyness, "
            "days_to_expiration, trade_date, trade_minute, fetched_at "
            "FROM option_quote WHERE instrument_id = ? AND expiration = ? AND expiration_type = ? "
            "ORDER BY strike, CASE right WHEN 'call' THEN 0 ELSE 1 END, vendor_symbol");
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
    constexpr std::string_view kV1Tables[] = {
        "bar", "corporate_action", "coverage_day", "instrument",};
    if (version >= 1)
    {
        requireTables(tableNames(), version, kV1Tables);
    }
    constexpr std::string_view kV2Tables[] = {"statement_cell", "statement_snapshot"};
    if (version >= 2)
    {
        requireTables(tableNames(), version, kV2Tables);
    }
    if (version < kSchemaUserVersion)
    {
        SqliteTxn txn(impl_->db.handle());
        if (version < 1)
        {
            impl_->db.exec(schemaV1());
        }
        if (version < 2)
        {
            impl_->db.exec(schemaV2());
        }
        if (version < 3)
        {
            impl_->db.exec(schemaV3());
        }
        impl_->db.setUserVersion(kSchemaUserVersion);
        txn.commit();
    }
    constexpr std::string_view kAllTables[] = {"bar",
                                                "corporate_action",
                                                "coverage_day",
                                                "instrument",
                                                "option_expiry",
                                                "option_quote",
                                                "option_underlying",
                                                "statement_cell",
                                                "statement_snapshot",};
    requireTables(tableNames(), userVersion(), kAllTables);
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

void Store::testingCreateSchemaV1(const std::filesystem::path& path)
{
    SqliteDb db(path);
    if (db.userVersion() != 0)
    {
        throw std::runtime_error("testingCreateSchemaV1 requires user_version 0");
    }
    db.exec(schemaV1());
    db.setUserVersion(1);
}

void Store::testingCreateSchemaV2(const std::filesystem::path& path)
{
    SqliteDb db(path);
    if (db.userVersion() != 0)
    {
        throw std::runtime_error("testingCreateSchemaV2 requires user_version 0");
    }
    db.exec(schemaV1());
    db.exec(schemaV2());
    db.setUserVersion(2);
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

void Store::replaceStatement(const StatementSnapshot& snapshot, std::span<const StatementCell> cells)
{
    if (snapshot.instrument_id <= 0)
    {
        throw std::runtime_error("statement snapshot instrument is missing");
    }
    if (snapshot.fetched_at < 0)
    {
        throw std::runtime_error("statement snapshot fetched_at is negative");
    }
    if (!isTrimmedNonEmpty(snapshot.source))
    {
        throw std::runtime_error("statement snapshot source is empty");
    }
    if (!findInstrumentById(snapshot.instrument_id).has_value())
    {
        throw std::runtime_error("statement snapshot instrument not found");
    }

    std::set<std::pair<std::string, std::string>> seen;
    for (const StatementCell& cell : cells)
    {
        if (cell.instrument_id != snapshot.instrument_id || cell.statement != snapshot.statement ||
            cell.timeframe != snapshot.timeframe)
        {
            throw std::runtime_error("statement cell does not match snapshot");
        }
        if (!isTrimmedNonEmpty(cell.line_item))
        {
            throw std::runtime_error("statement cell line_item is empty");
        }
        if (!isStatementPeriodEnd(cell.period_end))
        {
            throw std::runtime_error("statement cell period_end is invalid");
        }
        if (std::holds_alternative<std::monostate>(cell.value))
        {
            throw std::runtime_error("statement cell has no value");
        }
        if (const auto* real = std::get_if<double>(&cell.value))
        {
            if (!std::isfinite(*real))
            {
                throw std::runtime_error("statement cell real is not finite");
            }
        }
        if (const auto* text = std::get_if<std::string>(&cell.value))
        {
            if (text->empty())
            {
                throw std::runtime_error("statement cell text is empty");
            }
        }
        if (!seen.emplace(cell.line_item, cell.period_end).second)
        {
            throw std::runtime_error("duplicate statement cell");
        }
    }

    SqliteTxn txn(impl_->db.handle());
    auto& upsert = impl_->upsert_statement_snapshot;
    upsert.reset();
    upsert.bindInt64(1, snapshot.instrument_id);
    upsert.bindText(2, toSql(snapshot.statement));
    upsert.bindText(3, toSql(snapshot.timeframe));
    upsert.bindText(4, snapshot.source);
    upsert.bindInt64(5, snapshot.fetched_at);
    upsert.stepDone();
    upsert.reset();

    auto& del = impl_->del_statement_cells;
    del.reset();
    del.bindInt64(1, snapshot.instrument_id);
    del.bindText(2, toSql(snapshot.statement));
    del.bindText(3, toSql(snapshot.timeframe));
    del.stepDone();
    del.reset();

    auto& ins = impl_->ins_statement_cell;
    for (const StatementCell& cell : cells)
    {
        ins.reset();
        ins.bindInt64(1, cell.instrument_id);
        ins.bindText(2, toSql(cell.statement));
        ins.bindText(3, toSql(cell.timeframe));
        ins.bindText(4, cell.line_item);
        ins.bindText(5, cell.period_end);
        bindStatementValue(ins, 6, cell.value);
        ins.stepDone();
        ins.reset();
    }
    txn.commit();
}

std::optional<StatementSnapshot> Store::findStatementSnapshot(InstrumentId id,
                                                             StatementKind statement,
                                                             StatementTimeframe timeframe) const
{
    auto& sel = impl_->sel_statement_snapshot;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindText(2, toSql(statement));
    sel.bindText(3, toSql(timeframe));
    std::optional<StatementSnapshot> row;
    if (sel.stepRow())
    {
        row = StatementSnapshot{};
        row->instrument_id = id;
        row->statement = statement;
        row->timeframe = timeframe;
        row->source = sel.columnText(0);
        row->fetched_at = sel.columnInt64(1);
    }
    sel.reset();
    return row;
}

std::vector<StatementCell> Store::queryStatementCells(InstrumentId id,
                                                      StatementKind statement,
                                                      StatementTimeframe timeframe) const
{
    auto& sel = impl_->sel_statement_cells;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindText(2, toSql(statement));
    sel.bindText(3, toSql(timeframe));
    return collectStatementCells(sel);
}

std::vector<StatementCell> Store::queryStatementLine(InstrumentId id,
                                                     StatementKind statement,
                                                     StatementTimeframe timeframe,
                                                     std::string_view line_item) const
{
    auto& sel = impl_->sel_statement_line;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindText(2, toSql(statement));
    sel.bindText(3, toSql(timeframe));
    sel.bindText(4, line_item);
    return collectStatementCells(sel);
}

std::vector<StatementCell> Store::queryStatementPeriod(InstrumentId id,
                                                       StatementKind statement,
                                                       StatementTimeframe timeframe,
                                                       std::string_view period_end) const
{
    auto& sel = impl_->sel_statement_period;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindText(2, toSql(statement));
    sel.bindText(3, toSql(timeframe));
    sel.bindText(4, period_end);
    return collectStatementCells(sel);
}

void Store::replaceOptionChain(const OptionChainWrite& write)
{
    if (write.instrument_id <= 0)
    {
        throw std::runtime_error("option chain instrument is missing");
    }
    if (write.fetched_at < 0 || !isTrimmedNonEmpty(write.source))
    {
        throw std::runtime_error("option chain fetch identity is invalid");
    }
    if (!findInstrumentById(write.instrument_id).has_value())
    {
        throw std::runtime_error("option chain instrument not found");
    }
    if (write.has_underlying)
    {
        if (write.underlying.instrument_id != write.instrument_id || write.underlying.fetched_at != write.fetched_at ||
            write.underlying.source != write.source)
        {
            throw std::runtime_error("option underlying does not match the chain");
        }
        if (write.underlying.historic_vol_30d.has_value())
        {
            requireFinite(*write.underlying.historic_vol_30d, "historic vol");
            if (*write.underlying.historic_vol_30d < 0.0)
            {
                throw std::runtime_error("historic vol is out of range");
            }
        }
        if (write.underlying.iv_rank_1y.has_value())
        {
            requireFinite(*write.underlying.iv_rank_1y, "iv rank");
            if (*write.underlying.iv_rank_1y < 0.0)
            {
                throw std::runtime_error("iv rank is out of range");
            }
        }
        if (write.underlying.next_earnings.has_value() && !isSessionDate(*write.underlying.next_earnings))
        {
            throw std::runtime_error("option earnings date is invalid");
        }
        if (write.underlying.dividend_ex.has_value() && !isSessionDate(*write.underlying.dividend_ex))
        {
            throw std::runtime_error("option dividend date is invalid");
        }
        if (write.underlying.earnings_time.has_value() && !isTrimmedNonEmpty(*write.underlying.earnings_time))
        {
            throw std::runtime_error("option earnings time is empty");
        }
    }

    std::set<std::pair<SessionDate, OptionExpirationType>> slices;
    for (const OptionExpiry& row : write.calendar)
    {
        if (!isSessionDate(row.expiration))
        {
            throw std::runtime_error("option expiration is invalid");
        }
        if (!slices.emplace(row.expiration, row.expiration_type).second)
        {
            throw std::runtime_error("option expiration is repeated");
        }
    }
    std::set<std::string> symbols;
    for (const OptionQuoteBatch& batch : write.batches)
    {
        if (!isSessionDate(batch.expiration))
        {
            throw std::runtime_error("option expiration is invalid");
        }
        if (!slices.emplace(batch.expiration, batch.expiration_type).second &&
            std::ranges::count_if(write.batches, [&](const OptionQuoteBatch& other) {
                return other.expiration == batch.expiration && other.expiration_type == batch.expiration_type;
            }) > 1)
        {
            throw std::runtime_error("option expiration is repeated");
        }
        if (batch.average_iv.has_value())
        {
            requireFinite(*batch.average_iv, "average iv");
            if (*batch.average_iv < 0.0)
            {
                throw std::runtime_error("average iv is out of range");
            }
        }
        for (const OptionQuote& quote : batch.quotes)
        {
            validateOptionQuote(quote, batch, write.instrument_id);
            if (!symbols.emplace(quote.vendor_symbol).second)
            {
                throw std::runtime_error("option contract symbol is repeated");
            }
        }
    }
    std::set<std::pair<SessionDate, OptionExpirationType>> keep;
    if (write.replace_calendar)
    {
        for (const OptionExpiry& row : write.calendar)
        {
            keep.emplace(row.expiration, row.expiration_type);
        }
    }
    for (const OptionQuoteBatch& batch : write.batches)
    {
        keep.emplace(batch.expiration, batch.expiration_type);
    }

    SqliteTxn txn(impl_->db.handle());
    if (write.has_underlying)
    {
        auto& upsert = impl_->upsert_option_underlying;
        upsert.reset();
        upsert.bindInt64(1, write.instrument_id);
        upsert.bindText(2, write.source);
        upsert.bindInt64(3, write.fetched_at);
        bindOptionalDouble(upsert, 4, write.underlying.historic_vol_30d);
        bindOptionalDouble(upsert, 5, write.underlying.iv_rank_1y);
        bindOptionalInt(upsert, 6, write.underlying.next_earnings);
        bindOptionalInt(upsert, 7, write.underlying.dividend_ex);
        bindOptionalText(upsert, 8, write.underlying.earnings_time);
        upsert.stepDone();
        upsert.reset();
    }
    for (const OptionQuoteBatch& batch : write.batches)
    {
        auto& expiry = impl_->upsert_option_expiry;
        expiry.reset();
        expiry.bindInt64(1, write.instrument_id);
        expiry.bindInt(2, batch.expiration);
        expiry.bindText(3, toSql(batch.expiration_type));
        bindOptionalDouble(expiry, 4, batch.average_iv);
        expiry.bindInt64(5, write.fetched_at);
        expiry.bindText(6, write.source);
        expiry.stepDone();
        expiry.reset();

        auto& del = impl_->del_option_quotes;
        del.reset();
        del.bindInt64(1, write.instrument_id);
        del.bindInt(2, batch.expiration);
        del.bindText(3, toSql(batch.expiration_type));
        del.stepDone();
        del.reset();

        auto& ins = impl_->ins_option_quote;
        for (const OptionQuote& quote : batch.quotes)
        {
            ins.reset();
            ins.bindInt64(1, quote.instrument_id);
            ins.bindInt(2, quote.expiration);
            ins.bindText(3, toSql(quote.expiration_type));
            ins.bindText(4, quote.vendor_symbol);
            ins.bindDouble(5, quote.strike);
            ins.bindText(6, toSql(quote.right));
            ins.bindDouble(7, quote.bid);
            ins.bindDouble(8, quote.ask);
            ins.bindDouble(9, quote.mid);
            ins.bindDouble(10, quote.last);
            ins.bindDouble(11, quote.price_change);
            ins.bindDouble(12, quote.percent_change);
            ins.bindInt64(13, quote.volume);
            ins.bindInt64(14, quote.open_interest);
            ins.bindInt64(15, quote.open_interest_change);
            ins.bindDouble(16, quote.implied_vol);
            ins.bindDouble(17, quote.delta);
            ins.bindDouble(18, quote.rho);
            ins.bindDouble(19, quote.vega);
            ins.bindDouble(20, quote.theta);
            ins.bindDouble(21, quote.moneyness);
            ins.bindInt(22, quote.days_to_expiration);
            bindOptionalInt(ins, 23, quote.trade_date);
            bindOptionalInt(ins, 24, quote.trade_minute);
            ins.bindInt64(25, write.fetched_at);
            ins.stepDone();
            ins.reset();
        }
    }
    if (write.replace_calendar)
    {
        auto& insert = impl_->insert_option_expiry;
        for (const OptionExpiry& row : write.calendar)
        {
            insert.reset();
            insert.bindInt64(1, write.instrument_id);
            insert.bindInt(2, row.expiration);
            insert.bindText(3, toSql(row.expiration_type));
            insert.bindText(4, write.source);
            insert.stepDone();
            insert.reset();
        }
        std::vector<std::pair<SessionDate, OptionExpirationType>> drop;
        auto& listed = impl_->sel_option_expiries;
        listed.reset();
        listed.bindInt64(1, write.instrument_id);
        while (listed.stepRow())
        {
            const auto expiration = static_cast<SessionDate>(listed.columnInt64(0));
            const auto type = optionExpirationTypeFromSql(listed.columnText(1));
            if (!keep.contains(std::pair{expiration, type}))
            {
                drop.emplace_back(expiration, type);
            }
        }
        listed.reset();
        auto& del = impl_->del_option_expiry;
        for (const auto& [expiration, type] : drop)
        {
            del.reset();
            del.bindInt64(1, write.instrument_id);
            del.bindInt(2, expiration);
            del.bindText(3, toSql(type));
            del.stepDone();
            del.reset();
        }
    }
    txn.commit();
}

std::optional<OptionUnderlying> Store::findOptionUnderlying(InstrumentId id) const
{
    auto& sel = impl_->sel_option_underlying;
    sel.reset();
    sel.bindInt64(1, id);
    std::optional<OptionUnderlying> row;
    if (sel.stepRow())
    {
        row = OptionUnderlying{};
        row->instrument_id = id;
        row->source = sel.columnText(0);
        row->fetched_at = sel.columnInt64(1);
        if (!sel.columnIsNull(2))
        {
            row->historic_vol_30d = sel.columnDouble(2);
        }
        if (!sel.columnIsNull(3))
        {
            row->iv_rank_1y = sel.columnDouble(3);
        }
        if (!sel.columnIsNull(4))
        {
            row->next_earnings = static_cast<SessionDate>(sel.columnInt64(4));
        }
        if (!sel.columnIsNull(5))
        {
            row->dividend_ex = static_cast<SessionDate>(sel.columnInt64(5));
        }
        if (!sel.columnIsNull(6))
        {
            row->earnings_time = sel.columnText(6);
        }
    }
    sel.reset();
    return row;
}

std::vector<OptionExpiry> Store::queryOptionExpiries(InstrumentId id) const
{
    auto& sel = impl_->sel_option_expiries;
    sel.reset();
    sel.bindInt64(1, id);
    std::vector<OptionExpiry> rows;
    while (sel.stepRow())
    {
        OptionExpiry row;
        row.instrument_id = id;
        row.expiration = static_cast<SessionDate>(sel.columnInt64(0));
        row.expiration_type = optionExpirationTypeFromSql(sel.columnText(1));
        if (!sel.columnIsNull(2))
        {
            row.average_iv = sel.columnDouble(2);
        }
        if (!sel.columnIsNull(3))
        {
            row.fetched_at = sel.columnInt64(3);
        }
        row.source = sel.columnText(4);
        rows.push_back(std::move(row));
    }
    sel.reset();
    return rows;
}

std::vector<OptionQuote> Store::queryOptionQuotes(InstrumentId id,
                                                  SessionDate expiration,
                                                  OptionExpirationType expiration_type) const
{
    auto& sel = impl_->sel_option_quotes;
    sel.reset();
    sel.bindInt64(1, id);
    sel.bindInt(2, expiration);
    sel.bindText(3, toSql(expiration_type));
    std::vector<OptionQuote> rows;
    while (sel.stepRow())
    {
        rows.push_back(optionQuoteFromStmt(sel));
    }
    sel.reset();
    return rows;
}

}  // namespace terminal


