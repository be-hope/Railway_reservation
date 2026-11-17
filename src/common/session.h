#pragma once
#include <initializer_list>
#include <memory>
#include <string>
#include "common/clock.h"
#include "common/domain.h"
#include "common/result.h"

namespace rrs {
class AuthService;
struct Session;
namespace test { Session sessionFor(const std::string&, Role, const IClock&); }
struct Session {
    std::string sessionId;
    std::string accountId;
    Role role = Role::Passenger;
    EpochSeconds expiresAt = 0;
    void revoke() const;
private:
    struct State;
    std::shared_ptr<State> state_;
    static Result<Session> issue(const std::string&, Role, const IClock&);
    friend class AuthService;
    friend Session test::sessionFor(const std::string&, Role, const IClock&);
    friend Status requireRole(const Session&, std::initializer_list<Role>, const IClock&);
};
Status requireRole(const Session&, std::initializer_list<Role>, const IClock&);
}
