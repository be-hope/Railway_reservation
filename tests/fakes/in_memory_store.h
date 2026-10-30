// in_memory_store.h
// In-memory fakes of the repository interfaces (SAD CD-02, NFR-006). They
// enforce the same integrity rules as the SQLite schema (unique PNR, unique
// request_id, one active traveller per seat) so service tests behave the same
// on both backends. begin()/rollback() snapshot and restore the whole state.

#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "dal/repositories.h"

namespace rrs::fakes {

struct StoreState {
    std::map<std::string, Station> stations;
    std::map<std::string, Train> trains;
    std::set<std::pair<std::string, std::string>> schedules;   // (train, iso date), active only
    std::map<std::pair<std::string, int>, ClassConfig> configs; // (train, class)
    std::map<std::string, Reservation> reservations;            // by PNR
    std::map<std::string, Account> accounts;                    // by id
};

class InMemoryStore;

class InMemoryTrainRepository : public ITrainRepository {
public:
    explicit InMemoryTrainRepository(StoreState& s) : s_(s) {}
    Status saveStation(const Station& station) override;
    Result<std::vector<Station>> listStations() override;
    Status saveTrain(const Train& train) override;
    Status setTrainActive(const std::string& trainNumber, bool active) override;
    Result<std::optional<Train>> findTrain(const std::string& trainNumber) override;
    Result<std::vector<Train>> listTrains() override;
    Result<std::vector<Train>> findTrains(const std::string& source, const std::string& destination,
                                          const Date& date) override;
    Status addSchedule(const std::string& trainNumber, const Date& date) override;
    Status removeSchedule(const std::string& trainNumber, const Date& date) override;
    Result<bool> hasSchedule(const std::string& trainNumber, const Date& date) override;
    Result<std::vector<Date>> listSchedules(const std::string& trainNumber) override;

private:
    StoreState& s_;
};

class InMemoryConfigRepository : public IConfigRepository {
public:
    explicit InMemoryConfigRepository(StoreState& s) : s_(s) {}
    Status saveClassConfig(const ClassConfig& config) override;
    Result<std::optional<ClassConfig>> findClassConfig(const std::string& trainNumber,
                                                       ClassCode classCode) override;
    Result<std::vector<ClassConfig>> listClassConfigs(const std::string& trainNumber) override;

private:
    StoreState& s_;
};

class InMemoryReservationRepository : public IReservationRepository {
public:
    explicit InMemoryReservationRepository(StoreState& s) : s_(s) {}
    Status insert(const Reservation& reservation) override;
    Status update(const Reservation& reservation) override;
    Result<std::optional<Reservation>> findByPnr(const std::string& pnr) override;
    Result<std::optional<Reservation>> findByRequestId(const std::string& requestId) override;
    Result<bool> pnrExists(const std::string& pnr) override;
    Result<TripOccupancy> occupancy(const std::string& trainNumber, const Date& date,
                                    ClassCode classCode) override;
    Result<std::vector<Reservation>> listActiveForTrip(const std::string& trainNumber, const Date& date,
                                                       ClassCode classCode) override;
    Result<std::vector<Reservation>> listForAccount(const std::string& accountId) override;
    Result<std::vector<Reservation>> listByJourneyDate(const Date& from, const Date& to) override;
    Result<int> countActiveForTrain(const std::string& trainNumber, const Date& fromDate) override;
    Result<std::int64_t> nextQueueSeq() override;

private:
    Status checkSeats(const Reservation& r, const std::string& ignorePnr) const;
    StoreState& s_;
};

class InMemoryUserRepository : public IUserRepository {
public:
    explicit InMemoryUserRepository(StoreState& s) : s_(s) {}
    Status insert(const Account& account) override;
    Result<std::optional<Account>> findByLoginId(const std::string& loginId) override;
    Result<std::optional<Account>> findById(const std::string& accountId) override;
    Status updateAttempts(const std::string& accountId, int failedAttempts,
                          EpochSeconds lockedUntil) override;
    Status updateProfile(const Account& account) override;

private:
    StoreState& s_;
};

class InMemoryUnitOfWork : public IUnitOfWork {
public:
    explicit InMemoryUnitOfWork(StoreState& s) : s_(s) {}
    Status begin() override;
    Status commit() override;
    Status rollback() override;

private:
    StoreState& s_;
    std::unique_ptr<StoreState> snapshot_;
};

class InMemoryStore {
public:
    InMemoryStore() : trains_(state_), config_(state_), reservations_(state_), users_(state_), uow_(state_) {}
    DataAccess access() { return DataAccess{trains_, config_, reservations_, users_, uow_}; }
    StoreState& state() { return state_; }

private:
    StoreState state_;
    InMemoryTrainRepository trains_;
    InMemoryConfigRepository config_;
    InMemoryReservationRepository reservations_;
    InMemoryUserRepository users_;
    InMemoryUnitOfWork uow_;
};

}  // namespace rrs::fakes
