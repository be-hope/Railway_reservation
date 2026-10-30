// schedule_coach_service.h
// Administrator management of schedules, coaches, seats, RAC quota and
// waitlist capacity (FR-022, BR-04, IT-ADM-04). ADMIN only (MANAGE_TRAINS in
// the SAD §3.9.3 permission matrix); every change is audited (TRAIN_CHANGED).
//
// Capacity rules (AC-022):
//   * capacity = coaches x seats per coach
//   * a change is rejected with ERR-CAPACITY-BELOW-BOOKED (E5002) if, on any
//     journey date from today on, it would leave fewer seats than are
//     confirmed, drop a seat that is occupied, or fall below the RAC
//     travellers or waitlisted tickets already held
//   * after a change the waitlist of every affected date is re-planned, so
//     added seats or RAC slots go to the head of the queue immediately
//
// Train and route maintenance (add/update/remove train) is issue #55.

#pragma once

#include <string>
#include <vector>

#include "cancel/waitlist_manager.h"
#include "common/audit.h"
#include "common/clock.h"
#include "common/session.h"
#include "dal/repositories.h"

namespace rrs {

constexpr int kMaxCoaches = 30;
constexpr int kMaxSeatsPerCoach = 99;
constexpr int kMaxWaitlistCapacity = 999;

class ScheduleCoachService {
public:
    ScheduleCoachService(DataAccess data, const IClock& clock, IAuditLogger& audit);

    // Adds a schedule (journey date) for every day in [from, to].
    Status addSchedules(const Session& session, const std::string& trainNumber, const Date& from,
                        const Date& to);
    // Removes one journey date; E5001 if it still has active reservations.
    Status removeSchedule(const Session& session, const std::string& trainNumber, const Date& date);
    Result<std::vector<Date>> listSchedules(const Session& session, const std::string& trainNumber);

    // Creates or changes the configuration of one class on one train.
    // Returns the PNRs promoted because capacity grew.
    Result<std::vector<std::string>> configureClass(const Session& session, const ClassConfig& config);
    Result<std::vector<ClassConfig>> listClassConfigs(const Session& session, const std::string& trainNumber);

private:
    Status requireAdmin(const Session& session, const std::string& operation);
    Status validateConfig(const ClassConfig& config);
    // E5002 if `config` cannot hold what is already booked on `date`.
    Status checkFits(const ClassConfig& config, const Date& date);

    DataAccess data_;
    const IClock& clock_;
    IAuditLogger& audit_;
    WaitlistManager waitlist_;
};

std::string describeConfig(const ClassConfig& c);   // "SL 4x72 RAC 0 WL 20 x1.00"

}  // namespace rrs
