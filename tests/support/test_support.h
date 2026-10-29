// test_support.h
// Shared test helpers: storage backends (in-memory fake or SQLite), a fault-
// injecting reservation repository, a recording audit logger and builders.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common/audit.h"
#include "common/clock.h"
#include "common/session.h"
#include "dal/repositories.h"

namespace rrs::test {

// Fixed "today" used by every test (no test reads the real clock).
Date testToday();

// A storage backend: "memory" (fakes) or "sqlite" (in-memory SQLite database).
class Backend {
public:
    virtual ~Backend() = default;
    virtual DataAccess access() = 0;
    virtual std::string name() const = 0;
};
std::unique_ptr<Backend> makeBackend(const std::string& kind);
// Opens/creates an SQLite database file (for restart and persistence tests).
std::unique_ptr<Backend> makeSqliteFileBackend(const std::string& path);
std::vector<std::string> backendKinds();   // {"memory", "sqlite"}

class RecordingAuditLogger : public IAuditLogger {
public:
    struct Entry {
        std::string eventType, actorId, detail;
    };
    void log(const std::string& eventType, const std::string& actorId, const std::string& detail) override {
        entries.push_back({eventType, actorId, detail});
    }
    int count(const std::string& eventType) const;
    std::vector<Entry> entries;
};

// Wraps a reservation repository and fails selected calls (REL-02, rollback tests).
class FaultInjectingReservationRepository : public IReservationRepository {
public:
    explicit FaultInjectingReservationRepository(IReservationRepository& inner) : inner_(inner) {}

    int failNextInserts = 0;   // the next N insert() calls fail with E9001
    int failNextUpdates = 0;   // the next N update() calls fail with E9001

    Status insert(const Reservation& r) override;
    Status update(const Reservation& r) override;
    Result<std::optional<Reservation>> findByPnr(const std::string& pnr) override { return inner_.findByPnr(pnr); }
    Result<std::optional<Reservation>> findByRequestId(const std::string& id) override {
        return inner_.findByRequestId(id);
    }
    Result<bool> pnrExists(const std::string& pnr) override { return inner_.pnrExists(pnr); }
    Result<TripOccupancy> occupancy(const std::string& t, const Date& d, ClassCode c) override {
        return inner_.occupancy(t, d, c);
    }
    Result<std::vector<Reservation>> listActiveForTrip(const std::string& t, const Date& d, ClassCode c) override {
        return inner_.listActiveForTrip(t, d, c);
    }
    Result<std::vector<Reservation>> listForAccount(const std::string& a) override { return inner_.listForAccount(a); }
    Result<std::vector<Reservation>> listByJourneyDate(const Date& f, const Date& t) override {
        return inner_.listByJourneyDate(f, t);
    }
    Result<int> countActiveForTrain(const std::string& t, const Date& f) override {
        return inner_.countActiveForTrain(t, f);
    }
    Result<std::int64_t> nextQueueSeq() override { return inner_.nextQueueSeq(); }

private:
    IReservationRepository& inner_;
};

Session sessionFor(const std::string& accountId, Role role, const IClock& clock);
Traveller traveller(const std::string& name, int age = 30, char gender = 'F',
                    const std::string& idProof = "DL-KA012026001");

}  // namespace rrs::test
