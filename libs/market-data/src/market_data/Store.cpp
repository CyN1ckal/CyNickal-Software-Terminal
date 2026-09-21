#include "market_data/Store.h"

#include "Sqlite.h"
#include "market_data/Schema.h"
#include "market_data/Types.h"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace myapp {

namespace {

constexpr int kWriterBusyTimeoutMs = 5000;
constexpr int kReaderBusyTimeoutMs = 0;

[[nodiscard]] UnixSeconds nowUtc()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

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

}  // namespace

struct Store::Impl
{
    std::filesystem::path path;
    StoreMode mode{StoreMode::Writer};
    SqliteDb db;
    mutable SqliteStmt sel_instrument_symbol;
    mutable SqliteStmt sel_instrument_id;
    SqliteStmt ins_instrument;
    SqliteStmt upd_instrument;
    SqliteStmt count_bars;
    SqliteStmt ins_bar;
    mutable SqliteStmt sel_bars;

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
    UpsertBarsResult result;
    const UnixSeconds now = nowUtc();
    SqliteTxn txn(impl_->db.handle());
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

}  // namespace myapp
