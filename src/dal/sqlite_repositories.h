// sqlite_repositories.h
// SQLite implementations of the repository interfaces (component C7).

#pragma once

#include <memory>
#include <string>

#include "dal/repositories.h"
#include "dal/sqlite_database.h"

namespace rrs {

class SqliteTrainRepository : public ITrainRepository {
public:
    explicit SqliteTrainRepository(SqliteDatabase& db) : db_(db) {}

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
    Result<std::vector<RouteStop>> loadStops(const std::string& trainNumber);
    SqliteDatabase& db_;
};

class SqliteConfigRepository : public IConfigRepository {
public:
    explicit SqliteConfigRepository(SqliteDatabase& db) : db_(db) {}

    Status saveClassConfig(const ClassConfig& config) override;
    Result<std::optional<ClassConfig>> findClassConfig(const std::string& trainNumber,
                                                       ClassCode classCode) override;
    Result<std::vector<ClassConfig>> listClassConfigs(const std::string& trainNumber) override;

private:
    SqliteDatabase& db_;
};

class SqliteReservationRepository : public IReservationRepository {
public:
    explicit SqliteReservationRepository(SqliteDatabase& db) : db_(db) {}

    Status insert(const Reservation& reservation) override;
    Status update(const Reservation& reservation) override;
    Result<std::optional<Reservation>> findByPnr(const std::string& pnr) override;
    Result<std::optional<Reservation>> findByRequestId(const std::string& requestId) override;
    Result<bool> pnrExists(const std::string& pnr) override;
    Result<TripOccupancy> occupancy(const std::string& trainNumber, const Date& date,
                                    ClassCode classCode) override;
    Result<std::vector<Reservation>> listActiveForTrip(const std::string& trainNumber,
                                                       const Date& date,
                                                       ClassCode classCode) override;
    Result<std::vector<Reservation>> listForAccount(const std::string& accountId) override;
    Result<std::vector<Reservation>> listByJourneyDate(const Date& from, const Date& to) override;
    Result<int> countActiveForTrain(const std::string& trainNumber, const Date& fromDate) override;
    Result<std::int64_t> nextQueueSeq() override;

private:
    // Runs a SELECT over reservation columns (see kReservationColumns) and
    // loads the travellers of every row.
    Result<std::vector<Reservation>> query(Statement& stmt);
    Status loadTravellers(Reservation& r);
    SqliteDatabase& db_;
};

class SqliteUserRepository : public IUserRepository {
public:
    explicit SqliteUserRepository(SqliteDatabase& db) : db_(db) {}

    Status insert(const Account& account) override;
    Result<std::optional<Account>> findByLoginId(const std::string& loginId) override;
    Result<std::optional<Account>> findById(const std::string& accountId) override;
    Status updateAttempts(const std::string& accountId, int failedAttempts,
                          EpochSeconds lockedUntil) override;
    Status updateProfile(const Account& account) override;

private:
    Result<std::optional<Account>> findOne(const char* sql, const std::string& key);
    SqliteDatabase& db_;
};

// Owns a connection plus one instance of every repository; hands out the
// DataAccess bundle used to construct services.
class SqliteDataStore {
public:
    // Opens the file, runs the integrity check and creates missing tables.
    static Result<std::unique_ptr<SqliteDataStore>> open(const std::string& path);

    DataAccess access() { return DataAccess{trains_, config_, reservations_, users_, unitOfWork_}; }
    SqliteDatabase& database() { return *db_; }

private:
    explicit SqliteDataStore(std::unique_ptr<SqliteDatabase> db);
    std::unique_ptr<SqliteDatabase> db_;
    SqliteTrainRepository trains_;
    SqliteConfigRepository config_;
    SqliteReservationRepository reservations_;
    SqliteUserRepository users_;
    SqliteUnitOfWork unitOfWork_;
};

}  // namespace rrs
