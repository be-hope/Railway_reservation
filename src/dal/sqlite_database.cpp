// sqlite_database.cpp

#include "dal/sqlite_database.h"

#include <sqlite3.h>

#include "dal/schema.h"

namespace rrs {

// ---------------------------------------------------------------- Statement

Statement::Statement(SqliteDatabase& db, const char* sql) : db_(db) {
    lastRc_ = sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt_, nullptr);
    if (lastRc_ != SQLITE_OK) {
        error_ = db_.lastErrorMessage();
        stmt_ = nullptr;
    }
}

Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

Statement& Statement::bind(int index, const std::string& value) {
    if (stmt_ && error_.empty()) {
        lastRc_ = sqlite3_bind_text(stmt_, index, value.c_str(),
                                    static_cast<int>(value.size()), SQLITE_TRANSIENT);
        if (lastRc_ != SQLITE_OK) error_ = db_.lastErrorMessage();
    }
    return *this;
}

Statement& Statement::bind(int index, std::int64_t value) {
    if (stmt_ && error_.empty()) {
        lastRc_ = sqlite3_bind_int64(stmt_, index, static_cast<sqlite3_int64>(value));
        if (lastRc_ != SQLITE_OK) error_ = db_.lastErrorMessage();
    }
    return *this;
}

Statement& Statement::bindNull(int index) {
    if (stmt_ && error_.empty()) {
        lastRc_ = sqlite3_bind_null(stmt_, index);
        if (lastRc_ != SQLITE_OK) error_ = db_.lastErrorMessage();
    }
    return *this;
}

Statement& Statement::bindOptional(int index, const std::string& value) {
    return value.empty() ? bindNull(index) : bind(index, value);
}

bool Statement::step() {
    if (!stmt_ || !error_.empty()) return false;
    lastRc_ = sqlite3_step(stmt_);
    if (lastRc_ == SQLITE_ROW) return true;
    if (lastRc_ != SQLITE_DONE) error_ = db_.lastErrorMessage();
    return false;
}

Status Statement::execute() {
    while (step()) {
    }
    return status();
}

Status Statement::status() const {
    if (!stmt_ || !error_.empty()) {
        return Status::failure(ErrorCode::Database, error_.empty() ? "statement not prepared" : error_);
    }
    return Status::success();
}

std::string Statement::text(int column) const {
    const unsigned char* t = sqlite3_column_text(stmt_, column);
    return t ? reinterpret_cast<const char*>(t) : std::string();
}

std::int64_t Statement::integer(int column) const {
    return static_cast<std::int64_t>(sqlite3_column_int64(stmt_, column));
}

bool Statement::isNull(int column) const {
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

// ---------------------------------------------------------------- SqliteDatabase

Result<std::unique_ptr<SqliteDatabase>> SqliteDatabase::open(const std::string& path) {
    sqlite3* raw = nullptr;
    int rc = sqlite3_open_v2(path.c_str(), &raw,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = raw ? sqlite3_errmsg(raw) : "cannot open database";
        if (raw) sqlite3_close(raw);
        return Result<std::unique_ptr<SqliteDatabase>>::failure(ErrorCode::Database, msg);
    }
    std::unique_ptr<SqliteDatabase> db(new SqliteDatabase(raw));
    sqlite3_busy_timeout(raw, 2000);
    // SAD §3.6: referential integrity on, rollback journal, synchronous FULL.
    Status s = db->exec("PRAGMA foreign_keys = ON; PRAGMA synchronous = FULL;");
    if (!s.ok()) return s.error();
    return Result<std::unique_ptr<SqliteDatabase>>(std::move(db));
}

SqliteDatabase::~SqliteDatabase() {
    if (db_) sqlite3_close(db_);
}

Status SqliteDatabase::exec(const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "exec failed";
        sqlite3_free(err);
        return Status::failure(ErrorCode::Database, msg);
    }
    return Status::success();
}

Status SqliteDatabase::migrate() {
    Status s = exec(kSchemaSql);
    if (!s.ok()) return s;
    Statement count(*this, "SELECT COUNT(*) FROM schema_version");
    if (!count.step()) return count.status();
    if (count.integer(0) == 0) {
        Statement ins(*this, "INSERT INTO schema_version(version) VALUES (?)");
        return ins.bind(1, kSchemaVersion).execute();
    }
    return Status::success();
}

Status SqliteDatabase::integrityCheck() {
    Statement check(*this, "PRAGMA integrity_check");
    if (!check.step()) return check.status();
    std::string result = check.text(0);
    if (result != "ok") return Status::failure(ErrorCode::Database, "integrity check: " + result);
    return Status::success();
}

int SqliteDatabase::changes() const {
    return db_ ? sqlite3_changes(db_) : 0;
}

std::string SqliteDatabase::lastErrorMessage() const {
    return db_ ? sqlite3_errmsg(db_) : "no database";
}

// ---------------------------------------------------------------- SqliteUnitOfWork

Status SqliteUnitOfWork::begin() {
    if (active_) return Status::failure(ErrorCode::Internal, "nested transaction");
    Status s = db_.exec("BEGIN IMMEDIATE");
    active_ = s.ok();
    return s;
}

Status SqliteUnitOfWork::commit() {
    if (!active_) return Status::failure(ErrorCode::Internal, "no transaction");
    Status s = db_.exec("COMMIT");
    if (s.ok()) active_ = false;
    return s;
}

Status SqliteUnitOfWork::rollback() {
    if (!active_) return Status::success();
    active_ = false;
    return db_.exec("ROLLBACK");
}

}  // namespace rrs
