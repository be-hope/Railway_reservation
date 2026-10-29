// sqlite_user_repository.cpp
// Account storage (used by AuthService, issues #58/#59) and SqliteDataStore.

#include "dal/sqlite_repositories.h"

namespace rrs {

namespace {

const char* const kAccountSelect =
    "SELECT account_id, role, login_id, name, age, gender, contact_number, email, password_hash, "
    "salt, id_proof_type, id_proof_number, failed_attempts, locked_until FROM account ";

Account readAccount(const Statement& st) {
    Account a;
    a.accountId = st.text(0);
    a.role = parseRole(st.text(1)).value_or(Role::Passenger);
    a.loginId = st.text(2);
    a.name = st.text(3);
    a.age = st.isNull(4) ? 0 : static_cast<int>(st.integer(4));
    std::string g = st.text(5);
    a.gender = g.empty() ? 'O' : g[0];
    a.contactNumber = st.text(6);
    a.email = st.text(7);
    a.passwordHash = st.text(8);
    a.salt = st.text(9);
    a.idProofType = st.text(10);
    a.idProofNumber = st.text(11);
    a.failedAttempts = static_cast<int>(st.integer(12));
    a.lockedUntil = st.integer(13);
    return a;
}

}  // namespace

Status SqliteUserRepository::insert(const Account& a) {
    // Duplicate check first so the caller gets E1002 instead of a raw
    // constraint error (FR-004); the UNIQUE constraints remain the backstop.
    Statement dup(db_,
                  "SELECT 1 FROM account WHERE login_id = ? OR contact_number = ? OR email = ?");
    dup.bind(1, a.loginId).bindOptional(2, a.contactNumber).bindOptional(3, a.email);
    if (dup.step()) return Status::failure(ErrorCode::DuplicateUser);
    if (!dup.status().ok()) return dup.status();

    Statement st(db_,
                 "INSERT INTO account(account_id, role, login_id, name, age, gender, contact_number, "
                 "email, password_hash, salt, id_proof_type, id_proof_number, failed_attempts, "
                 "locked_until) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    st.bind(1, a.accountId).bind(2, roleString(a.role)).bind(3, a.loginId).bind(4, a.name);
    if (a.age > 0) st.bind(5, a.age);
    else st.bindNull(5);
    st.bind(6, std::string(1, a.gender)).bindOptional(7, a.contactNumber).bindOptional(8, a.email)
        .bind(9, a.passwordHash).bind(10, a.salt).bindOptional(11, a.idProofType)
        .bindOptional(12, a.idProofNumber).bind(13, a.failedAttempts).bind(14, a.lockedUntil);
    return st.execute();
}

Result<std::optional<Account>> SqliteUserRepository::findOne(const char* where, const std::string& key) {
    std::string sql = std::string(kAccountSelect) + where;
    Statement st(db_, sql.c_str());
    st.bind(1, key);
    if (!st.step()) {
        if (!st.status().ok()) return st.status().error();
        return std::optional<Account>();
    }
    return std::optional<Account>(readAccount(st));
}

Result<std::optional<Account>> SqliteUserRepository::findByLoginId(const std::string& loginId) {
    return findOne("WHERE login_id = ?", loginId);
}

Result<std::optional<Account>> SqliteUserRepository::findById(const std::string& accountId) {
    return findOne("WHERE account_id = ?", accountId);
}

Status SqliteUserRepository::updateAttempts(const std::string& accountId, int failedAttempts,
                                            EpochSeconds lockedUntil) {
    Statement st(db_, "UPDATE account SET failed_attempts = ?, locked_until = ? WHERE account_id = ?");
    return st.bind(1, failedAttempts).bind(2, lockedUntil).bind(3, accountId).execute();
}

Status SqliteUserRepository::updateProfile(const Account& a) {
    Statement st(db_,
                 "UPDATE account SET name = ?, age = ?, gender = ?, contact_number = ?, email = ?, "
                 "id_proof_type = ?, id_proof_number = ? WHERE account_id = ?");
    st.bind(1, a.name);
    if (a.age > 0) st.bind(2, a.age);
    else st.bindNull(2);
    return st.bind(3, std::string(1, a.gender)).bindOptional(4, a.contactNumber)
        .bindOptional(5, a.email).bindOptional(6, a.idProofType).bindOptional(7, a.idProofNumber)
        .bind(8, a.accountId)
        .execute();
}

// ---------------------------------------------------------------- SqliteDataStore

SqliteDataStore::SqliteDataStore(std::unique_ptr<SqliteDatabase> db)
    : db_(std::move(db)),
      trains_(*db_),
      config_(*db_),
      reservations_(*db_),
      users_(*db_),
      unitOfWork_(*db_) {}

Result<std::unique_ptr<SqliteDataStore>> SqliteDataStore::open(const std::string& path) {
    auto db = SqliteDatabase::open(path);
    if (!db.ok()) return db.error();
    Status s = db.value()->integrityCheck();
    if (!s.ok()) return s.error();
    s = db.value()->migrate();
    if (!s.ok()) return s.error();
    std::unique_ptr<SqliteDataStore> store(new SqliteDataStore(std::move(db.value())));
    return Result<std::unique_ptr<SqliteDataStore>>(std::move(store));
}

}  // namespace rrs
