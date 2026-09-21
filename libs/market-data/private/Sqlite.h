#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace myapp {

class SqliteDb
{
public:
    explicit SqliteDb(const std::filesystem::path& path);
    ~SqliteDb() noexcept;

    SqliteDb(const SqliteDb&) = delete;
    SqliteDb& operator=(const SqliteDb&) = delete;
    SqliteDb(SqliteDb&&) = delete;
    SqliteDb& operator=(SqliteDb&&) = delete;

    [[nodiscard]] sqlite3* handle() const noexcept { return db_; }
    void exec(std::string_view sql);
    [[nodiscard]] int userVersion() const;
    void setUserVersion(int version);
    void applyConnectionPragmas(int busy_timeout_ms);

private:
    sqlite3* db_ = nullptr;
};

class SqliteStmt
{
public:
    SqliteStmt() = default;
    SqliteStmt(sqlite3* db, std::string_view sql);
    ~SqliteStmt() noexcept;

    SqliteStmt(const SqliteStmt&) = delete;
    SqliteStmt& operator=(const SqliteStmt&) = delete;
    SqliteStmt(SqliteStmt&&) noexcept;
    SqliteStmt& operator=(SqliteStmt&&) noexcept;

    void prepare(sqlite3* db, std::string_view sql);
    void bindNull(int idx);
    void bindInt(int idx, int value);
    void bindInt64(int idx, std::int64_t value);
    void bindDouble(int idx, double value);
    void bindText(int idx, std::string_view value);
    bool stepRow();
    void stepDone();
    void reset() noexcept;

    [[nodiscard]] std::int64_t columnInt64(int idx) const;
    [[nodiscard]] double columnDouble(int idx) const;
    [[nodiscard]] std::string columnText(int idx) const;
    [[nodiscard]] bool columnIsNull(int idx) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

class SqliteTxn
{
public:
    explicit SqliteTxn(sqlite3* db);
    ~SqliteTxn() noexcept;
    SqliteTxn(const SqliteTxn&) = delete;
    SqliteTxn& operator=(const SqliteTxn&) = delete;
    SqliteTxn(SqliteTxn&&) = delete;
    SqliteTxn& operator=(SqliteTxn&&) = delete;
    void commit();

private:
    sqlite3* db_ = nullptr;
    bool committed_ = false;
};

}  // namespace myapp
