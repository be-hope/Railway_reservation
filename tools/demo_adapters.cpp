// demo_adapters.cpp

#include "demo_adapters.h"

#include <cstdio>

namespace rrs::demo {

namespace {

std::string hhmm(int minutes) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", (minutes / 60) % 100, minutes % 60);
    return buf;
}

}  // namespace

TrainSummary summarize(const Train& train, const std::string& source, const std::string& destination) {
    TrainSummary s;
    s.trainNumber = train.number;
    s.trainName = train.name;
    const RouteStop* from = nullptr;
    const RouteStop* to = nullptr;
    for (const RouteStop& stop : train.stops) {
        if (stop.stationCode == source) from = &stop;
        if (stop.stationCode == destination) to = &stop;
    }
    if (from && to) {
        const int dep = from->dayOffset * 1440 + from->departureMinutes;
        const int arr = to->dayOffset * 1440 + to->arrivalMinutes;
        s.departure = hhmm(from->departureMinutes);
        s.arrival = hhmm(to->arrivalMinutes);
        s.duration = hhmm(arr - dep);
    }
    return s;
}

Result<std::vector<TrainSummary>> RepositoryTrainSearch::search(const std::string& source,
                                                                const std::string& destination,
                                                                const Date& date) {
    auto trains = trains_.findTrains(source, destination, date);
    if (!trains.ok()) return trains.error();
    std::vector<TrainSummary> rows;
    for (const Train& t : trains.value()) rows.push_back(summarize(t, source, destination));
    return rows;
}

Result<Train> RepositoryTrainSearch::route(const std::string& trainNumber) {
    auto train = trains_.findTrain(trainNumber);
    if (!train.ok()) return train.error();
    if (!train.value() || !train.value()->active) return Result<Train>::failure(ErrorCode::TrainNotFound);
    return *train.value();
}

Result<std::vector<Reservation>> RepositoryBookingHistory::history(const Session& session) {
    Status access = requireRole(session, {Role::Passenger}, clock_);
    if (!access.ok()) return access.error();
    return reservations_.listForAccount(session.accountId);
}

Result<Account> RepositoryProfile::view(const Session& session) {
    Status access = requireRole(session, {Role::Passenger}, clock_);
    if (!access.ok()) return access.error();
    auto account = users_.findById(session.accountId);
    if (!account.ok()) return account.error();
    if (!account.value()) return Result<Account>::failure(ErrorCode::Forbidden);
    return *account.value();
}

Status RepositoryProfile::update(const Session& session, const Account& changes) {
    Status access = requireRole(session, {Role::Passenger}, clock_);
    if (!access.ok()) return access;
    if (changes.accountId != session.accountId) return Status::failure(ErrorCode::Forbidden);
    if (changes.name.empty() || changes.name.size() > 100) {
        return Status::failure(ErrorCode::Validation, "Invalid name: 1 to 100 characters.");
    }
    if (changes.age < 1 || changes.age > 120) {
        return Status::failure(ErrorCode::Validation, "Invalid age: must be between 1 and 120.");
    }
    if (changes.gender != 'M' && changes.gender != 'F' && changes.gender != 'O') {
        return Status::failure(ErrorCode::Validation, "Invalid gender: enter M, F or O.");
    }
    if (changes.idProofType.size() > 20 || changes.idProofNumber.size() > 30) {
        return Status::failure(ErrorCode::Validation, "Invalid ID proof: type <= 20, number <= 30 characters.");
    }
    return users_.updateProfile(changes);
}

Result<Reservation> InterimCancellation::cancel(const Session& session, const std::string& pnr) {
    auto ticket = booking_.getTicket(session, pnr);   // session, format and ownership checks
    if (!ticket.ok()) return ticket.error();
    auto released = release_.cancelAndRelease(pnr, std::nullopt, session.accountId);
    if (!released.ok()) return released.error();
    return released.value().cancelled;
}

}  // namespace rrs::demo
