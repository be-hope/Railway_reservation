// repositories.h
// Data-access interfaces (component C7, SAD §3.4 / §4.3, CD-02).
// Interface version: 1 (bump on any signature change, SAD §4.3).
//
// The business layer depends only on these interfaces. Two implementations
// exist: the SQLite repositories (src/dal/sqlite_*) used by the application
// and the in-memory fakes (tests/fakes) used by unit tests (NFR-006).
// All operations return Result<T>; a storage failure is E9001 (ERR-DB).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/date.h"
#include "common/domain.h"
#include "common/result.h"

namespace rrs {

// ---------------------------------------------------------------- trains

class ITrainRepository {
public:
    virtual ~ITrainRepository() = default;

    virtual Status saveStation(const Station& station) = 0;
    virtual Result<std::vector<Station>> listStations() = 0;

    // Inserts or replaces the train and its ordered route stops.
    virtual Status saveTrain(const Train& train) = 0;
    virtual Status setTrainActive(const std::string& trainNumber, bool active) = 0;
    virtual Result<std::optional<Train>> findTrain(const std::string& trainNumber) = 0;
    virtual Result<std::vector<Train>> listTrains() = 0;

    // Active trains with an active schedule on `date` that stop at `source`
    // before `destination` (FR-005). Uses ix_route_stop_station/ix_schedule_date.
    virtual Result<std::vector<Train>> findTrains(const std::string& source,
                                                  const std::string& destination,
                                                  const Date& date) = 0;

    // Schedules: one row per journey date a train runs (SRS §1.4 "Schedule").
    virtual Status addSchedule(const std::string& trainNumber, const Date& date) = 0;
    virtual Status removeSchedule(const std::string& trainNumber, const Date& date) = 0;
    virtual Result<bool> hasSchedule(const std::string& trainNumber, const Date& date) = 0;
    virtual Result<std::vector<Date>> listSchedules(const std::string& trainNumber) = 0;
};

// ---------------------------------------------------------------- configuration

class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;

    virtual Status saveClassConfig(const ClassConfig& config) = 0;
    virtual Result<std::optional<ClassConfig>> findClassConfig(const std::string& trainNumber,
                                                               ClassCode classCode) = 0;
    virtual Result<std::vector<ClassConfig>> listClassConfigs(const std::string& trainNumber) = 0;
};

// ---------------------------------------------------------------- reservations

// Current load of one train/date/class, computed from active reservations.
struct TripOccupancy {
    int confirmedTravellers = 0;            // = seats in use
    int racTravellers = 0;
    int waitlistTickets = 0;
    std::vector<std::string> occupiedSeats; // seat numbers of active tickets
};

class IReservationRepository {
public:
    virtual ~IReservationRepository() = default;

    // Inserts the reservation and its travellers. Fails with E9001 if the PNR,
    // the request_id or a seat (ux_seat) is already taken.
    virtual Status insert(const Reservation& reservation) = 0;
    // Updates status, waitlist position, refund, cancellation time and the
    // seat number of every traveller (seat released = empty).
    virtual Status update(const Reservation& reservation) = 0;

    virtual Result<std::optional<Reservation>> findByPnr(const std::string& pnr) = 0;
    virtual Result<std::optional<Reservation>> findByRequestId(const std::string& requestId) = 0;
    virtual Result<bool> pnrExists(const std::string& pnr) = 0;

    virtual Result<TripOccupancy> occupancy(const std::string& trainNumber, const Date& date,
                                            ClassCode classCode) = 0;
    // Active (non-cancelled) reservations of one trip ordered by queueSeq.
    virtual Result<std::vector<Reservation>> listActiveForTrip(const std::string& trainNumber,
                                                               const Date& date,
                                                               ClassCode classCode) = 0;
    // All reservations owned by an account, ordered by journey date then PNR.
    virtual Result<std::vector<Reservation>> listForAccount(const std::string& accountId) = 0;
    // All reservations with from <= journey date <= to (reports, FR-025).
    virtual Result<std::vector<Reservation>> listByJourneyDate(const Date& from, const Date& to) = 0;
    // Active reservations of a train on or after `fromDate` (E5001 checks).
    virtual Result<int> countActiveForTrain(const std::string& trainNumber, const Date& fromDate) = 0;

    // Next value of the global booking sequence (strict queue order, BR-07).
    virtual Result<std::int64_t> nextQueueSeq() = 0;
};

// ---------------------------------------------------------------- accounts

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    // Fails with E1002 when login id, contact number or e-mail already exists.
    virtual Status insert(const Account& account) = 0;
    virtual Result<std::optional<Account>> findByLoginId(const std::string& loginId) = 0;
    virtual Result<std::optional<Account>> findById(const std::string& accountId) = 0;
    virtual Status updateAttempts(const std::string& accountId, int failedAttempts,
                                  EpochSeconds lockedUntil) = 0;
    virtual Status updateProfile(const Account& account) = 0;
};

// ---------------------------------------------------------------- transactions

// One unit of work = one SQLite transaction (BEGIN IMMEDIATE ... COMMIT).
class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;
    virtual Status begin() = 0;
    virtual Status commit() = 0;
    virtual Status rollback() = 0;
};

// RAII guard: rolls back unless commit() succeeded.
class TransactionGuard {
public:
    explicit TransactionGuard(IUnitOfWork& uow) : uow_(uow) { status_ = uow_.begin(); }
    ~TransactionGuard() {
        if (status_.ok() && !done_) uow_.rollback();
    }
    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;

    const Status& started() const { return status_; }
    Status commit() {
        done_ = true;
        Status s = uow_.commit();
        if (!s.ok()) uow_.rollback();
        return s;
    }

private:
    IUnitOfWork& uow_;
    Status status_;
    bool done_ = false;
};

// The four repositories plus the unit of work (SAD "IDataAccess").
struct DataAccess {
    ITrainRepository& trains;
    IConfigRepository& config;
    IReservationRepository& reservations;
    IUserRepository& users;
    IUnitOfWork& unitOfWork;
};

}  // namespace rrs
