#include "security/access_control.h"
namespace rrs {
Status AccessControl::require(const Session& s, Permission p, const std::string& owner) {
    Status status;
    switch (p) {
    case Permission::BookTicket: status = requireRole(s, {Role::Passenger, Role::Staff}, clock_); break;
    case Permission::Profile:
    case Permission::History: status = requireRole(s, {Role::Passenger}, clock_); break;
    case Permission::ViewPnr:
    case Permission::CancelTicket: status = requireRole(s, {Role::Passenger, Role::Staff, Role::Admin}, clock_); break;
    case Permission::EditReservation: status = requireRole(s, {Role::Staff, Role::Admin}, clock_); break;
    case Permission::ManageTrains:
    case Permission::ViewAudit: status = requireRole(s, {Role::Admin}, clock_); break;
    case Permission::ViewReports: status = requireRole(s, {Role::Admin, Role::Ops}, clock_); break;
    default: status = Status::failure(ErrorCode::Forbidden); break;
    }
    if (status.ok() && s.role == Role::Passenger && !owner.empty() && owner != s.accountId)
        status = Status::failure(ErrorCode::Forbidden);
    if (!status.ok()) audit_.log("UNAUTHORIZED_ATTEMPT", s.accountId, "permission=" + std::to_string(static_cast<int>(p)));
    return status;
}
}
