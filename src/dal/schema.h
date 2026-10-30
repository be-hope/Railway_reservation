// schema.h
// SQLite schema of rrs.db (SRS Figure A-3 ER model, Appendix A, SAD §3.6).

#pragma once

namespace rrs {

constexpr int kSchemaVersion = 1;

// DDL executed by SqliteDatabase::migrate(); every statement is idempotent.
extern const char* const kSchemaSql;

}  // namespace rrs
