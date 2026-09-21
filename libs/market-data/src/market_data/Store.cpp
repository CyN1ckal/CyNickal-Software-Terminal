#include "market_data/Store.h"

#include "Sqlite.h"
#include "market_data/Schema.h"
#include "market_data/Types.h"

#include <stdexcept>
#include <utility>

namespace myapp {

namespace {

constexpr int kWriterBusyTimeoutMs = 5000;
constexpr int kReaderBusyTimeoutMs = 0;

}  // namespace

struct Store::Impl
{
    std::filesystem::path path;
    StoreMode mode{StoreMode::Writer};
    SqliteDb db;

    explicit Impl(std::filesystem::path db_path, StoreMode store_mode)
        : path(std::move(db_path)), mode(store_mode), db(path)
    {
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

}  // namespace myapp
