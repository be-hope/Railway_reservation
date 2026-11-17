#include "common/session.h"
#include "auth/password_hasher.h"
namespace rrs {
struct Session::State {
    std::string id, account;
    Role role;
    EpochSeconds issuedExpiry, idleExpiry;
    bool revoked = false;
};
Result<Session> Session::issue(const std::string& account, Role role, const IClock& clock) {
    auto token = PasswordHasher::randomToken();
    if (!token.ok()) return token.error();
    Session s;
    s.sessionId = token.value(); s.accountId = account; s.role = role;
    s.expiresAt = clock.now() + 900;
    s.state_ = std::make_shared<State>(State{s.sessionId, account, role, s.expiresAt, s.expiresAt, false});
    return s;
}
void Session::revoke() const { if (state_) state_->revoked = true; }
Status requireRole(const Session& s, std::initializer_list<Role> allowed, const IClock& clock) {
    if (!s.state_ || s.state_->revoked || s.accountId.empty() || s.sessionId != s.state_->id ||
        s.accountId != s.state_->account || s.role != s.state_->role ||
        s.expiresAt != s.state_->issuedExpiry || s.state_->idleExpiry <= clock.now()) {
        return Status::failure(ErrorCode::SessionExpired);
    }
    for (Role role : allowed) if (role == s.role) {
        s.state_->idleExpiry = clock.now() + 900;
        return Status::success();
    }
    return Status::failure(ErrorCode::Forbidden);
}
}
