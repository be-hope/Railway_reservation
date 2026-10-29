// schedule_coach_service.cpp

#include "admin/schedule_coach_service.h"

#include "booking/seat_allocator.h"

namespace rrs {

namespace {

constexpr long kMaxScheduleSpanDays = 366;

Status capacityError(const Date& date, const std::string& reason) {
    return Status::failure(ErrorCode::CapacityBelowBooked, date.display() + ": " + reason);
}

}  // namespace

std::string describeConfig(const ClassConfig& c) {
    const int whole = c.fareMultiplierPercent / 100, frac = c.fareMultiplierPercent % 100;
    return classCodeString(c.classCode) + " " + std::to_string(c.coaches) + "x" + std::to_string(c.seatsPerCoach) +
           " RAC " + std::to_string(c.racQuota) + " WL " + std::to_string(c.waitlistCapacity) + " x" +
           std::to_string(whole) + "." + (frac < 10 ? "0" : "") + std::to_string(frac);
}

ScheduleCoachService::ScheduleCoachService(DataAccess data, const IClock& clock, IAuditLogger& audit)
    : data_(data), clock_(clock), audit_(audit), waitlist_(data.reservations, data.config) {}

Status ScheduleCoachService::requireAdmin(const Session& session, const std::string& operation) {
    Status s = requireRole(session, {Role::Admin}, clock_);
    if (s.code() == ErrorCode::Forbidden) audit_.log("UNAUTHORIZED_ATTEMPT", session.accountId, operation);
    return s;
}

Status ScheduleCoachService::validateConfig(const ClassConfig& c) {
    if (c.coaches < 1 || c.coaches > kMaxCoaches) {
        return Status::failure(ErrorCode::Validation,
                               "Invalid coaches: 1 to " + std::to_string(kMaxCoaches) + ".");
    }
    if (c.seatsPerCoach < 1 || c.seatsPerCoach > kMaxSeatsPerCoach) {
        return Status::failure(ErrorCode::Validation,
                               "Invalid seats per coach: 1 to " + std::to_string(kMaxSeatsPerCoach) + ".");
    }
    if (c.racQuota < 0 || c.racQuota > c.capacity()) {
        return Status::failure(ErrorCode::Validation, "Invalid RAC quota: 0 to the class capacity.");
    }
    if (c.waitlistCapacity < 0 || c.waitlistCapacity > kMaxWaitlistCapacity) {
        return Status::failure(ErrorCode::Validation,
                               "Invalid waitlist capacity: 0 to " + std::to_string(kMaxWaitlistCapacity) + ".");
    }
    if (c.fareMultiplierPercent <= 0 || c.fareMultiplierPercent > 1000) {
        return Status::failure(ErrorCode::Validation, "Invalid fare multiplier: above 0 and at most 10.00.");
    }
    return Status::success();
}

Status ScheduleCoachService::checkFits(const ClassConfig& c, const Date& date) {
    auto occ = data_.reservations.occupancy(c.trainNumber, date, c.classCode);
    if (!occ.ok()) return occ.error();
    const TripOccupancy& o = occ.value();
    if (o.confirmedTravellers > c.capacity()) {
        return capacityError(date, std::to_string(o.confirmedTravellers) + " seats booked, capacity " +
                                       std::to_string(c.capacity()));
    }
    for (const std::string& seat : o.occupiedSeats) {
        if (!seatExists(c, seat)) return capacityError(date, "seat " + seat + " is booked");
    }
    if (o.racTravellers > c.racQuota) {
        return capacityError(date, std::to_string(o.racTravellers) + " RAC travellers booked");
    }
    if (o.waitlistTickets > c.waitlistCapacity) {
        return capacityError(date, std::to_string(o.waitlistTickets) + " tickets waitlisted");
    }
    return Status::success();
}

Status ScheduleCoachService::addSchedules(const Session& session, const std::string& trainNumber,
                                          const Date& from, const Date& to) {
    Status access = requireAdmin(session, "addSchedules");
    if (!access.ok()) return access;
    if (to < from || to.dayNumber() - from.dayNumber() >= kMaxScheduleSpanDays) {
        return Status::failure(ErrorCode::Validation, "Invalid date range: from <= to, at most 366 days.");
    }
    if (from < clock_.today()) {
        return Status::failure(ErrorCode::Validation, "Invalid date range: schedules cannot start in the past.");
    }
    auto train = data_.trains.findTrain(trainNumber);
    if (!train.ok()) return train.error();
    if (!train.value() || !train.value()->active) return Status::failure(ErrorCode::TrainNotFound);

    TransactionGuard tx(data_.unitOfWork);
    if (!tx.started().ok()) return tx.started();
    for (Date d = from; d <= to; d = d.plusDays(1)) {
        Status s = data_.trains.addSchedule(trainNumber, d);
        if (!s.ok()) return s;
    }
    Status committed = tx.commit();
    if (!committed.ok()) return committed;
    audit_.log("TRAIN_CHANGED", session.accountId,
               "train=" + trainNumber + " schedules added " + from.iso() + ".." + to.iso());
    return Status::success();
}

Status ScheduleCoachService::removeSchedule(const Session& session, const std::string& trainNumber,
                                            const Date& date) {
    Status access = requireAdmin(session, "removeSchedule");
    if (!access.ok()) return access;
    auto runs = data_.trains.hasSchedule(trainNumber, date);
    if (!runs.ok()) return runs.error();
    if (!runs.value()) return Status::failure(ErrorCode::TrainNotFound, "no service on " + date.display());

    TransactionGuard tx(data_.unitOfWork);
    if (!tx.started().ok()) return tx.started();
    for (ClassCode cls : {ClassCode::SL, ClassCode::AC3}) {
        auto active = data_.reservations.listActiveForTrip(trainNumber, date, cls);
        if (!active.ok()) return active.error();
        if (!active.value().empty()) {
            audit_.log("TRAIN_CHANGED", session.accountId,
                       "rejected removal of " + trainNumber + " on " + date.iso() + ": active reservations");
            return Status::failure(ErrorCode::TrainInUse, date.display());
        }
    }
    Status s = data_.trains.removeSchedule(trainNumber, date);
    if (!s.ok()) return s;
    Status committed = tx.commit();
    if (!committed.ok()) return committed;
    audit_.log("TRAIN_CHANGED", session.accountId, "train=" + trainNumber + " schedule removed " + date.iso());
    return Status::success();
}

Result<std::vector<Date>> ScheduleCoachService::listSchedules(const Session& session,
                                                              const std::string& trainNumber) {
    Status access = requireAdmin(session, "listSchedules");
    if (!access.ok()) return access.error();
    return data_.trains.listSchedules(trainNumber);
}

Result<std::vector<std::string>> ScheduleCoachService::configureClass(const Session& session,
                                                                      const ClassConfig& config) {
    using R = Result<std::vector<std::string>>;
    Status access = requireAdmin(session, "configureClass");
    if (!access.ok()) return access.error();
    Status valid = validateConfig(config);
    if (!valid.ok()) return valid.error();
    auto train = data_.trains.findTrain(config.trainNumber);
    if (!train.ok()) return train.error();
    if (!train.value()) return R::failure(ErrorCode::TrainNotFound);

    TransactionGuard tx(data_.unitOfWork);
    if (!tx.started().ok()) return tx.started().error();

    auto old = data_.config.findClassConfig(config.trainNumber, config.classCode);
    if (!old.ok()) return old.error();
    auto dates = data_.trains.listSchedules(config.trainNumber);
    if (!dates.ok()) return dates.error();

    const Date today = clock_.today();
    for (const Date& d : dates.value()) {
        if (d < today) continue;
        Status fits = checkFits(config, d);
        if (!fits.ok()) {
            audit_.log("TRAIN_CHANGED", session.accountId,
                       "rejected " + config.trainNumber + " " + describeConfig(config) + ": " + fits.error().detail);
            return fits.error();
        }
    }

    Status saved = data_.config.saveClassConfig(config);
    if (!saved.ok()) return saved.error();

    // More seats or RAC slots go to the waiting queue straight away.
    std::vector<std::string> promoted;
    for (const Date& d : dates.value()) {
        if (d < today) continue;
        auto p = waitlist_.promote(config.trainNumber, d, config.classCode);
        if (!p.ok()) return p.error();
        promoted.insert(promoted.end(), p.value().begin(), p.value().end());
    }
    Status committed = tx.commit();
    if (!committed.ok()) return committed.error();

    audit_.log("TRAIN_CHANGED", session.accountId,
               "train=" + config.trainNumber + " old=" + (old.value() ? describeConfig(*old.value()) : "none") +
                   " new=" + describeConfig(config) + " promoted=" + std::to_string(promoted.size()));
    return promoted;
}

Result<std::vector<ClassConfig>> ScheduleCoachService::listClassConfigs(const Session& session,
                                                                        const std::string& trainNumber) {
    Status access = requireAdmin(session, "listClassConfigs");
    if (!access.ok()) return access.error();
    return data_.config.listClassConfigs(trainNumber);
}

}  // namespace rrs
