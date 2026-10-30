#pragma once
#include "common/audit.h"
#include "common/session.h"
namespace rrs {
enum class Permission { BookTicket, Profile, History, ViewPnr, CancelTicket, EditReservation, ManageTrains, ViewReports, ViewAudit };
class AccessControl {
public:
    AccessControl(const IClock& clock, IAuditLogger& audit) : clock_(clock), audit_(audit) {}
    Status require(const Session& session, Permission permission, const std::string& owner = {});
private:
    const IClock& clock_;
    IAuditLogger& audit_;
};
}
