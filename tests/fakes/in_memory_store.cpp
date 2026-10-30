// in_memory_store.cpp

#include "fakes/in_memory_store.h"

#include <algorithm>

namespace rrs::fakes {

namespace {

Status dbError(const std::string& what) { return Status::failure(ErrorCode::Database, what); }

int stopIndex(const Train& t, const std::string& code) {
    for (std::size_t i = 0; i < t.stops.size(); ++i) {
        if (t.stops[i].stationCode == code) return static_cast<int>(i);
    }
    return -1;
}

bool sameTrip(const Reservation& r, const std::string& train, const Date& date, ClassCode cls) {
    return r.trainNumber == train && r.journeyDate == date && r.classCode == cls;
}

}  // namespace

// ---------------------------------------------------------------- trains

Status InMemoryTrainRepository::saveStation(const Station& station) {
    s_.stations[station.code] = station;
    return Status::success();
}

Result<std::vector<Station>> InMemoryTrainRepository::listStations() {
    std::vector<Station> out;
    for (const auto& kv : s_.stations) out.push_back(kv.second);
    return out;
}

Status InMemoryTrainRepository::saveTrain(const Train& train) {
    for (const RouteStop& stop : train.stops) {
        if (!s_.stations.count(stop.stationCode)) return dbError("FOREIGN KEY constraint failed");
    }
    s_.trains[train.number] = train;
    return Status::success();
}

Status InMemoryTrainRepository::setTrainActive(const std::string& trainNumber, bool active) {
    auto it = s_.trains.find(trainNumber);
    if (it != s_.trains.end()) it->second.active = active;
    return Status::success();
}

Result<std::optional<Train>> InMemoryTrainRepository::findTrain(const std::string& trainNumber) {
    auto it = s_.trains.find(trainNumber);
    if (it == s_.trains.end()) return std::optional<Train>();
    return std::optional<Train>(it->second);
}

Result<std::vector<Train>> InMemoryTrainRepository::listTrains() {
    std::vector<Train> out;
    for (const auto& kv : s_.trains) out.push_back(kv.second);
    return out;
}

Result<std::vector<Train>> InMemoryTrainRepository::findTrains(const std::string& source,
                                                               const std::string& destination,
                                                               const Date& date) {
    std::vector<Train> out;
    for (const auto& kv : s_.trains) {
        const Train& t = kv.second;
        if (!t.active || !s_.schedules.count({t.number, date.iso()})) continue;
        const int a = stopIndex(t, source), b = stopIndex(t, destination);
        if (a >= 0 && b >= 0 && a < b) out.push_back(t);
    }
    std::stable_sort(out.begin(), out.end(), [&](const Train& x, const Train& y) {
        return x.stops[stopIndex(x, source)].departureMinutes < y.stops[stopIndex(y, source)].departureMinutes;
    });
    return out;
}

Status InMemoryTrainRepository::addSchedule(const std::string& trainNumber, const Date& date) {
    if (!s_.trains.count(trainNumber)) return dbError("FOREIGN KEY constraint failed");
    s_.schedules.insert({trainNumber, date.iso()});
    return Status::success();
}

Status InMemoryTrainRepository::removeSchedule(const std::string& trainNumber, const Date& date) {
    s_.schedules.erase({trainNumber, date.iso()});
    return Status::success();
}

Result<bool> InMemoryTrainRepository::hasSchedule(const std::string& trainNumber, const Date& date) {
    return s_.schedules.count({trainNumber, date.iso()}) > 0;
}

Result<std::vector<Date>> InMemoryTrainRepository::listSchedules(const std::string& trainNumber) {
    std::vector<Date> out;
    for (const auto& s : s_.schedules) {
        if (s.first == trainNumber) out.push_back(*Date::parseIso(s.second));
    }
    return out;
}

// ---------------------------------------------------------------- config

Status InMemoryConfigRepository::saveClassConfig(const ClassConfig& config) {
    if (!s_.trains.count(config.trainNumber)) return dbError("FOREIGN KEY constraint failed");
    if (config.coaches < 1 || config.seatsPerCoach < 1 || config.racQuota < 0 ||
        config.waitlistCapacity < 0 || config.fareMultiplierPercent <= 0) {
        return dbError("CHECK constraint failed: class_config");
    }
    s_.configs[{config.trainNumber, static_cast<int>(config.classCode)}] = config;
    return Status::success();
}

Result<std::optional<ClassConfig>> InMemoryConfigRepository::findClassConfig(const std::string& trainNumber,
                                                                             ClassCode classCode) {
    auto it = s_.configs.find({trainNumber, static_cast<int>(classCode)});
    if (it == s_.configs.end()) return std::optional<ClassConfig>();
    return std::optional<ClassConfig>(it->second);
}

Result<std::vector<ClassConfig>> InMemoryConfigRepository::listClassConfigs(const std::string& trainNumber) {
    std::vector<ClassConfig> out;
    for (const auto& kv : s_.configs) {
        if (kv.first.first == trainNumber) out.push_back(kv.second);
    }
    return out;
}

// ---------------------------------------------------------------- reservations

Status InMemoryReservationRepository::checkSeats(const Reservation& r, const std::string& ignorePnr) const {
    for (const Traveller& t : r.travellers) {
        if (t.seatNumber.empty()) continue;
        for (const auto& kv : s_.reservations) {
            if (kv.first == ignorePnr || !sameTrip(kv.second, r.trainNumber, r.journeyDate, r.classCode)) continue;
            for (const Traveller& other : kv.second.travellers) {
                if (other.seatNumber == t.seatNumber) return dbError("UNIQUE constraint failed: ux_seat");
            }
        }
    }
    return Status::success();
}

Status InMemoryReservationRepository::insert(const Reservation& r) {
    if (s_.reservations.count(r.pnr)) return dbError("UNIQUE constraint failed: reservation.pnr");
    for (const auto& kv : s_.reservations) {
        if (kv.second.requestId == r.requestId) return dbError("UNIQUE constraint failed: reservation.request_id");
    }
    if (!s_.accounts.count(r.accountId) || !s_.accounts.count(r.bookedBy)) {
        return dbError("FOREIGN KEY constraint failed");
    }
    if ((r.status == TicketStatus::Waitlisted) != (r.waitlistPosition > 0)) {
        return dbError("CHECK constraint failed: reservation");
    }
    Status s = checkSeats(r, "");
    if (!s.ok()) return s;
    s_.reservations[r.pnr] = r;
    return Status::success();
}

Status InMemoryReservationRepository::update(const Reservation& r) {
    auto it = s_.reservations.find(r.pnr);
    if (it == s_.reservations.end()) return Status::failure(ErrorCode::PnrNotFound, r.pnr);
    if ((r.status == TicketStatus::Waitlisted) != (r.waitlistPosition > 0)) {
        return dbError("CHECK constraint failed: reservation");
    }
    Reservation candidate = it->second;
    candidate.status = r.status;
    candidate.waitlistPosition = r.waitlistPosition;
    candidate.refund = r.refund;
    candidate.cancelledAt = r.cancelledAt;
    for (std::size_t i = 0; i < candidate.travellers.size() && i < r.travellers.size(); ++i) {
        candidate.travellers[i] = r.travellers[i];
    }
    Status s = checkSeats(candidate, r.pnr);
    if (!s.ok()) return s;
    it->second = candidate;
    return Status::success();
}

Result<std::optional<Reservation>> InMemoryReservationRepository::findByPnr(const std::string& pnr) {
    auto it = s_.reservations.find(pnr);
    if (it == s_.reservations.end()) return std::optional<Reservation>();
    return std::optional<Reservation>(it->second);
}

Result<std::optional<Reservation>> InMemoryReservationRepository::findByRequestId(const std::string& requestId) {
    for (const auto& kv : s_.reservations) {
        if (kv.second.requestId == requestId) return std::optional<Reservation>(kv.second);
    }
    return std::optional<Reservation>();
}

Result<bool> InMemoryReservationRepository::pnrExists(const std::string& pnr) {
    return s_.reservations.count(pnr) > 0;
}

Result<TripOccupancy> InMemoryReservationRepository::occupancy(const std::string& trainNumber, const Date& date,
                                                               ClassCode classCode) {
    TripOccupancy occ;
    for (const auto& kv : s_.reservations) {
        const Reservation& r = kv.second;
        if (!sameTrip(r, trainNumber, date, classCode)) continue;
        switch (r.status) {
            case TicketStatus::Confirmed: occ.confirmedTravellers += r.travellerCount(); break;
            case TicketStatus::Rac: occ.racTravellers += r.travellerCount(); break;
            case TicketStatus::Waitlisted: occ.waitlistTickets += 1; break;
            case TicketStatus::Cancelled: break;
        }
        for (const Traveller& t : r.travellers) {
            if (!t.seatNumber.empty()) occ.occupiedSeats.push_back(t.seatNumber);
        }
    }
    std::sort(occ.occupiedSeats.begin(), occ.occupiedSeats.end());
    return occ;
}

Result<std::vector<Reservation>> InMemoryReservationRepository::listActiveForTrip(const std::string& trainNumber,
                                                                                  const Date& date,
                                                                                  ClassCode classCode) {
    std::vector<Reservation> out;
    for (const auto& kv : s_.reservations) {
        if (sameTrip(kv.second, trainNumber, date, classCode) && isActive(kv.second.status)) {
            out.push_back(kv.second);
        }
    }
    std::sort(out.begin(), out.end(),
              [](const Reservation& a, const Reservation& b) { return a.queueSeq < b.queueSeq; });
    return out;
}

Result<std::vector<Reservation>> InMemoryReservationRepository::listForAccount(const std::string& accountId) {
    std::vector<Reservation> out;
    for (const auto& kv : s_.reservations) {
        if (kv.second.accountId == accountId) out.push_back(kv.second);
    }
    std::sort(out.begin(), out.end(), [](const Reservation& a, const Reservation& b) {
        if (a.journeyDate != b.journeyDate) return a.journeyDate < b.journeyDate;
        return a.pnr < b.pnr;
    });
    return out;
}

Result<std::vector<Reservation>> InMemoryReservationRepository::listByJourneyDate(const Date& from, const Date& to) {
    std::vector<Reservation> out;
    for (const auto& kv : s_.reservations) {
        if (kv.second.journeyDate >= from && kv.second.journeyDate <= to) out.push_back(kv.second);
    }
    std::sort(out.begin(), out.end(), [](const Reservation& a, const Reservation& b) {
        if (a.journeyDate != b.journeyDate) return a.journeyDate < b.journeyDate;
        return a.queueSeq < b.queueSeq;
    });
    return out;
}

Result<int> InMemoryReservationRepository::countActiveForTrain(const std::string& trainNumber,
                                                               const Date& fromDate) {
    int n = 0;
    for (const auto& kv : s_.reservations) {
        const Reservation& r = kv.second;
        if (r.trainNumber == trainNumber && r.journeyDate >= fromDate && isActive(r.status)) ++n;
    }
    return n;
}

Result<std::int64_t> InMemoryReservationRepository::nextQueueSeq() {
    std::int64_t max = 0;
    for (const auto& kv : s_.reservations) max = std::max(max, kv.second.queueSeq);
    return max + 1;
}

// ---------------------------------------------------------------- users

Status InMemoryUserRepository::insert(const Account& a) {
    for (const auto& kv : s_.accounts) {
        const Account& o = kv.second;
        if (o.loginId == a.loginId || (!a.contactNumber.empty() && o.contactNumber == a.contactNumber) ||
            (!a.email.empty() && o.email == a.email)) {
            return Status::failure(ErrorCode::DuplicateUser);
        }
    }
    if (s_.accounts.count(a.accountId)) return dbError("UNIQUE constraint failed: account.account_id");
    s_.accounts[a.accountId] = a;
    return Status::success();
}

Result<std::optional<Account>> InMemoryUserRepository::findByLoginId(const std::string& loginId) {
    for (const auto& kv : s_.accounts) {
        if (kv.second.loginId == loginId || (!loginId.empty() && (kv.second.contactNumber == loginId || kv.second.email == loginId))) return std::optional<Account>(kv.second);
    }
    return std::optional<Account>();
}

Result<std::optional<Account>> InMemoryUserRepository::findById(const std::string& accountId) {
    auto it = s_.accounts.find(accountId);
    if (it == s_.accounts.end()) return std::optional<Account>();
    return std::optional<Account>(it->second);
}

Status InMemoryUserRepository::updateAttempts(const std::string& accountId, int failedAttempts,
                                              EpochSeconds lockedUntil) {
    auto it = s_.accounts.find(accountId);
    if (it != s_.accounts.end()) {
        it->second.failedAttempts = failedAttempts;
        it->second.lockedUntil = lockedUntil;
    }
    return Status::success();
}

Status InMemoryUserRepository::updateProfile(const Account& account) {
    auto it = s_.accounts.find(account.accountId);
    if (it == s_.accounts.end()) return Status::success();
    Account& a = it->second;
    a.name = account.name;
    a.age = account.age;
    a.gender = account.gender;
    a.contactNumber = account.contactNumber;
    a.email = account.email;
    a.idProofType = account.idProofType;
    a.idProofNumber = account.idProofNumber;
    return Status::success();
}

// ---------------------------------------------------------------- unit of work

Status InMemoryUnitOfWork::begin() {
    if (snapshot_) return Status::failure(ErrorCode::Internal, "nested transaction");
    snapshot_ = std::make_unique<StoreState>(s_);
    return Status::success();
}

Status InMemoryUnitOfWork::commit() {
    if (!snapshot_) return Status::failure(ErrorCode::Internal, "no transaction");
    snapshot_.reset();
    return Status::success();
}

Status InMemoryUnitOfWork::rollback() {
    if (snapshot_) {
        s_ = *snapshot_;
        snapshot_.reset();
    }
    return Status::success();
}

}  // namespace rrs::fakes
