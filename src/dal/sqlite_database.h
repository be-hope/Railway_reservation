// sqlite_database.h
// RAII wrapper over one SQLite connection and its prepared statements.
// All SQL uses bound parameters only - no SQL text is built from user input
// (SEC-003, SAD §3.9.5).

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "common/result.h"
#include "dal/repositories.h"

struct sqlite3;
struct sqlite3_stmt;

namespace rrs {

class SqliteDatabase;

// A prepared statement. Bind indexes are 1-based like the SQLite C API.
class Statement {
public:
    Statement(SqliteDatabase& db, const char* sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool prepared() const { return stmt_ != nullptr; }

    Statement& bind(int index, const std::string& value);
    Statement& bind(int index, std::int64_t value);
    Statement& bind(int index, int value) { return bind(index, static_cast<std::int64_t>(value)); }
    Statement& bindNull(int index);
    // Binds NULL when the string is empty.
    Statement& bindOptional(int index, const std::string& value);

    // Advances to the next row; false when done or on error (check status()).
    bool step();
    // Runs a statement that returns no rows.
    Status execute();
    // E9001 if a previous step()/bind failed; carries the SQLite message.
    Status status() const;

    std::string text(int column) const;
    std::int64_t integer(int column) const;
    bool isNull(int column) const;

private:
    SqliteDatabase& db_;
    sqlite3_stmt* stmt_ = nullptr;
    int lastRc_ = 0;
    std::string error_;
};

class SqliteDatabase {
public:
    // Opens (or creates) the database file; ":memory:" gives a private
    // in-memory database for tests.
    static Result<std::unique_ptr<SqliteDatabase>> open(const std::string& path);
    ~SqliteDatabase();
    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;

    // Creates missing tables and indexes (idempotent) and records the version.
    Status migrate();
    // PRAGMA integrity_check on start-up (SAD §4.4).
    Status integrityCheck();
    // Runs SQL without parameters (DDL, PRAGMA, BEGIN/COMMIT).
    Status exec(const char* sql);

    // Rows changed by the last INSERT/UPDATE/DELETE.
    int changes() const;

    sqlite3* handle() { return db_; }
    std::string lastErrorMessage() const;

private:
    explicit SqliteDatabase(sqlite3* db) : db_(db) {}
    sqlite3* db_ = nullptr;
};

// IUnitOfWork backed by BEGIN IMMEDIATE / COMMIT / ROLLBACK (SAD §3.7).
class SqliteUnitOfWork : public IUnitOfWork {
public:
    explicit SqliteUnitOfWork(SqliteDatabase& db) : db_(db) {}
    Status begin() override;
    Status commit() override;
    Status rollback() override;

private:
    SqliteDatabase& db_;
    bool active_ = false;
};

}  // namespace rrs
