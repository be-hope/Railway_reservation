// test_support.cpp

#include "support/test_support.h"

#include <stdexcept>

#include "dal/sqlite_repositories.h"
#include "fakes/in_memory_store.h"

namespace rrs::test {

namespace {

class MemoryBackend : public Backend {
public:
    DataAccess access() override { return store_.access(); }
    std::string name() const override { return "memory"; }

private:
    fakes::InMemoryStore store_;
};

class SqliteBackend : public Backend {
public:
    explicit SqliteBackend(const std::string& path) {
        auto store = SqliteDataStore::open(path);
        if (!store.ok()) throw std::runtime_error(formatError(store.error()));
        store_ = std::move(store.value());
    }
    DataAccess access() override { return store_->access(); }
    std::string name() const override { return "sqlite"; }

private:
    std::unique_ptr<SqliteDataStore> store_;
};

}  // namespace

Date testToday() { return *Date::fromYmd(2026, 11, 2); }

std::unique_ptr<Backend> makeBackend(const std::string& kind) {
    if (kind == "memory") return std::make_unique<MemoryBackend>();
    return std::make_unique<SqliteBackend>(":memory:");
}

std::unique_ptr<Backend> makeSqliteFileBackend(const std::string& path) {
    return std::make_unique<SqliteBackend>(path);
}

std::vector<std::string> backendKinds() { return {"memory", "sqlite"}; }

int RecordingAuditLogger::count(const std::string& eventType) const {
    int n = 0;
    for (const Entry& e : entries) n += e.eventType == eventType ? 1 : 0;
    return n;
}

Status FaultInjectingReservationRepository::insert(const Reservation& r) {
    if (failNextInserts > 0) {
        --failNextInserts;
        return Status::failure(ErrorCode::Database, "injected insert fault");
    }
    return inner_.insert(r);
}

Status FaultInjectingReservationRepository::update(const Reservation& r) {
    if (failNextUpdates > 0) {
        --failNextUpdates;
        return Status::failure(ErrorCode::Database, "injected update fault");
    }
    if (updatesBeforeFault == 0) {
        updatesBeforeFault = -1;
        return Status::failure(ErrorCode::Database, "injected update fault");
    }
    if (updatesBeforeFault > 0) --updatesBeforeFault;
    return inner_.update(r);
}

Session sessionFor(const std::string& accountId, Role role, const IClock& clock) {
    Session s;
    s.sessionId = "test-" + accountId;
    s.accountId = accountId;
    s.role = role;
    s.expiresAt = clock.now() + 15 * 60;
    return s;
}

Traveller traveller(const std::string& name, int age, char gender, const std::string& idProof) {
    Traveller t;
    t.name = name;
    t.age = age;
    t.gender = gender;
    t.idProofNumber = idProof;
    return t;
}

}  // namespace rrs::test
