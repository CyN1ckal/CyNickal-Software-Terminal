// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "Sqlite.h"

#include "sqlite3.h"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] std::string sqliteError(sqlite3* db, std::string_view prefix, std::string_view sql = {})
{
    std::string message;
    message += prefix;
    message += ": ";
    message += db != nullptr ? sqlite3_errmsg(db) : sqlite3_errstr(SQLITE_NOMEM);
    if (!sql.empty())
    {
        message += " [";
        message += sql;
        message += "]";
    }
    return message;
}

void checkOk(int rc, sqlite3* db, std::string_view prefix, std::string_view sql = {})
{
    if (rc != SQLITE_OK)
    {
        throw std::runtime_error(sqliteError(db, prefix, sql));
    }
}

}  // namespace

SqliteDb::SqliteDb(const std::filesystem::path& path)
{
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX |
                      SQLITE_OPEN_URI;
    const int rc = sqlite3_open_v2(path.string().c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK)
    {
        const std::string message = sqliteError(db_, "sqlite3_open_v2", path.string());
        if (db_ != nullptr)
        {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error(message);
    }
}

SqliteDb::~SqliteDb() noexcept
{
    if (db_ == nullptr)
    {
        return;
    }
    const int rc = sqlite3_close(db_);
    if (rc == SQLITE_BUSY)
    {
        sqlite3_close_v2(db_);
    }
    else if (rc != SQLITE_OK)
    {
        std::fprintf(stderr, "sqlite3_close: %s\n", sqlite3_errmsg(db_));
    }
    db_ = nullptr;
}

void SqliteDb::exec(std::string_view sql)
{
    const std::string owned(sql);
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, owned.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::string message = "sqlite3_exec: ";
        message += err != nullptr ? err : sqlite3_errmsg(db_);
        message += " [";
        message += owned;
        message += "]";
        sqlite3_free(err);
        throw std::runtime_error(message);
    }
}

int SqliteDb::userVersion() const
{
    SqliteStmt stmt(db_, "PRAGMA user_version");
    if (!stmt.stepRow())
    {
        throw std::runtime_error("PRAGMA user_version returned no row");
    }
    const auto version = static_cast<int>(stmt.columnInt64(0));
    stmt.reset();
    return version;
}

void SqliteDb::setUserVersion(int version)
{
    exec("PRAGMA user_version = " + std::to_string(version));
}

void SqliteDb::applyConnectionPragmas(int busy_timeout_ms)
{
    sqlite3_extended_result_codes(db_, 1);
    exec("PRAGMA foreign_keys = ON");
    exec("PRAGMA journal_mode = WAL");
    exec("PRAGMA synchronous = NORMAL");
    exec("PRAGMA busy_timeout = " + std::to_string(busy_timeout_ms));
}

std::int64_t SqliteDb::lastInsertRowid() const
{
    return sqlite3_last_insert_rowid(db_);
}

int SqliteDb::extendedError() const
{
    return sqlite3_extended_errcode(db_);
}

SqliteStmt::SqliteStmt(sqlite3* db, std::string_view sql)
{
    prepare(db, sql);
}

SqliteStmt::~SqliteStmt() noexcept
{
    if (stmt_ != nullptr)
    {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

SqliteStmt::SqliteStmt(SqliteStmt&& other) noexcept : stmt_(other.stmt_)
{
    other.stmt_ = nullptr;
}

SqliteStmt& SqliteStmt::operator=(SqliteStmt&& other) noexcept
{
    if (this != &other)
    {
        if (stmt_ != nullptr)
        {
            sqlite3_finalize(stmt_);
        }
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void SqliteStmt::prepare(sqlite3* db, std::string_view sql)
{
    if (stmt_ != nullptr)
    {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
    const std::string owned(sql);
    const int rc = sqlite3_prepare_v2(db, owned.c_str(), -1, &stmt_, nullptr);
    checkOk(rc, db, "sqlite3_prepare_v2", owned);
}

void SqliteStmt::bindNull(int idx)
{
    checkOk(sqlite3_bind_null(stmt_, idx), sqlite3_db_handle(stmt_), "sqlite3_bind_null");
}

void SqliteStmt::bindInt(int idx, int value)
{
    checkOk(sqlite3_bind_int(stmt_, idx, value), sqlite3_db_handle(stmt_), "sqlite3_bind_int");
}

void SqliteStmt::bindInt64(int idx, std::int64_t value)
{
    checkOk(sqlite3_bind_int64(stmt_, idx, value), sqlite3_db_handle(stmt_), "sqlite3_bind_int64");
}

void SqliteStmt::bindDouble(int idx, double value)
{
    checkOk(sqlite3_bind_double(stmt_, idx, value), sqlite3_db_handle(stmt_), "sqlite3_bind_double");
}

void SqliteStmt::bindText(int idx, std::string_view value)
{
    const char* data = value.empty() ? "" : value.data();
    // SQLITE_TRANSIENT is ((void(*)(void*))-1); sqlite copies the bytes.
    checkOk(sqlite3_bind_text(stmt_,
                              idx,
                              data,
                              static_cast<int>(value.size()),
                              SQLITE_TRANSIENT),  // NOLINT(performance-no-int-to-ptr)
            sqlite3_db_handle(stmt_),
            "sqlite3_bind_text");
}

bool SqliteStmt::stepRow()
{
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)
    {
        return true;
    }
    if (rc == SQLITE_DONE)
    {
        return false;
    }
    throw std::runtime_error(sqliteError(sqlite3_db_handle(stmt_), "sqlite3_step"));
}

void SqliteStmt::stepDone()
{
    const int rc = sqlite3_step(stmt_);
    if (rc != SQLITE_DONE)
    {
        throw std::runtime_error(sqliteError(sqlite3_db_handle(stmt_), "sqlite3_step"));
    }
}

SqliteStmt::Constraint SqliteStmt::stepDoneOrConstraint()
{
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_DONE)
    {
        return Constraint::None;
    }
    sqlite3* db = sqlite3_db_handle(stmt_);
    const int ext = sqlite3_extended_errcode(db);
    if (ext == SQLITE_CONSTRAINT_CHECK)
    {
        return Constraint::Check;
    }
    if (ext == SQLITE_CONSTRAINT_FOREIGNKEY)
    {
        return Constraint::ForeignKey;
    }
    throw std::runtime_error(sqliteError(db, "sqlite3_step"));
}

void SqliteStmt::reset() noexcept
{
    if (stmt_ != nullptr)
    {
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
    }
}

std::int64_t SqliteStmt::columnInt64(int idx) const
{
    return sqlite3_column_int64(stmt_, idx);
}

double SqliteStmt::columnDouble(int idx) const
{
    return sqlite3_column_double(stmt_, idx);
}

std::string SqliteStmt::columnText(int idx) const
{
    const auto* text = sqlite3_column_text(stmt_, idx);
    if (text == nullptr)
    {
        return {};
    }
    const int bytes = sqlite3_column_bytes(stmt_, idx);
    return {reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes)};
}

bool SqliteStmt::columnIsNull(int idx) const
{
    return sqlite3_column_type(stmt_, idx) == SQLITE_NULL;
}

SqliteTxn::SqliteTxn(sqlite3* db) : db_(db)
{
    if (sqlite3_get_autocommit(db_) == 0)
    {
        throw std::runtime_error("nested transaction");
    }
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::string message = "BEGIN IMMEDIATE: ";
        message += err != nullptr ? err : sqlite3_errmsg(db_);
        sqlite3_free(err);
        throw std::runtime_error(message);
    }
}

SqliteTxn::~SqliteTxn() noexcept
{
    if (db_ == nullptr || committed_)
    {
        return;
    }
    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
}

void SqliteTxn::commit()
{
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::string message = "COMMIT: ";
        message += err != nullptr ? err : sqlite3_errmsg(db_);
        sqlite3_free(err);
        throw std::runtime_error(message);
    }
    committed_ = true;
}

}  // namespace terminal
