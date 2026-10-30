// session.cpp

#include "common/session.h"

namespace rrs {

Status requireRole(const Session& session, std::initializer_list<Role> allowed,
                   const IClock& clock) {
    if (session.accountId.empty() || session.expiresAt <= clock.now()) {
        return Status::failure(ErrorCode::SessionExpired);
    }
    for (Role r : allowed) {
        if (r == session.role) return Status::success();
    }
    return Status::failure(ErrorCode::Forbidden);
}

}  // namespace rrs
