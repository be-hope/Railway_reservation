// session.h
// Session data and the role check every service entry point performs
// (SAD §3.9.3 permission matrix, §3.9.4 sessions).
//
// NOTE: sessions are created by AuthService (issues #59 login/session) and the
// full AccessControl / RBAC matrix is issue #19. This header only carries the
// session record and a small role guard so the booking module can enforce
// SEC-002 / SEC-004 today; swap the guard for AccessControl::requirePermission
// once #19 lands.

#pragma once

#include <initializer_list>
#include <string>

#include "common/clock.h"
#include "common/domain.h"
#include "common/result.h"

namespace rrs {

struct Session {
    std::string sessionId;
    std::string accountId;
    Role role = Role::Passenger;
    EpochSeconds expiresAt = 0;   // now + 15 minutes, refreshed by AuthService
};

// E1005 for an empty or expired session, E1006 when the role is not allowed.
Status requireRole(const Session& session, std::initializer_list<Role> allowed,
                   const IClock& clock);

}  // namespace rrs
