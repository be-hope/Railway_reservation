#pragma once
#include "admin/schedule_coach_service.h"
#include "security/access_control.h"
namespace rrs {
class TrainAdminService {
public:
    TrainAdminService(DataAccess data, const IClock& clock, IAuditLogger& audit)
        : data_(data), clock_(clock), audit_(audit), access_(clock, audit), schedules_(data, clock, audit) {}
    Status addTrain(const Session&, const Train&);
    Status updateTrainName(const Session&, const std::string&, const std::string&);
    Status deactivateTrain(const Session&, const std::string&);
    Status activateTrain(const Session&, const std::string&);
    Result<std::vector<Train>> listTrains(const Session& s) {
        auto allowed = access_.require(s, Permission::ManageTrains);
        if (!allowed.ok()) return allowed.error();
        return data_.trains.listTrains();
    }
    Result<Train> getTrain(const Session&, const std::string&);
    Status setRoute(const Session&, const std::string&, const std::vector<RouteStop>&);
    Result<std::vector<RouteStop>> getRoute(const Session&, const std::string&);
    Status addSchedule(const Session& s, const std::string& n, const Date& d) { return schedules_.addSchedules(s, n, d, d); }
    Status removeSchedule(const Session& s, const std::string& n, const Date& d) { return schedules_.removeSchedule(s, n, d); }
    Result<std::vector<Date>> listSchedules(const Session& s, const std::string& n) { return schedules_.listSchedules(s, n); }
private:
    Status save(const Session&, const Train&);
    Status validateRoute(const std::vector<RouteStop>&);
    DataAccess data_;
    const IClock& clock_;
    IAuditLogger& audit_;
    AccessControl access_;
    ScheduleCoachService schedules_;
};
}
