#include "auth/auth_service.h"
#include "security/input_validator.h"
namespace rrs {
Result<std::string> AuthService::registerPassenger(const Registration& r) {
    using R = Result<std::string>;
    if (r.mobile.empty() && r.email.empty()) return R::failure(ErrorCode::Validation, "Mobile or email required.");
    for (const auto& s : {InputValidator::name(r.name), InputValidator::password(r.password)}) if (!s.ok()) return s.error();
    if (!r.mobile.empty()) { auto s = InputValidator::mobile(r.mobile); if (!s.ok()) return s.error(); }
    if (!r.email.empty()) { auto s = InputValidator::email(r.email); if (!s.ok()) return s.error(); }
    Account a;
    a.contactNumber = r.mobile;
    a.email = InputValidator::normalizeEmail(r.email);
    a.loginId = r.mobile.empty() ? a.email : r.mobile;
    a.name = r.name;
    auto id = PasswordHasher::randomToken();
    if (!id.ok()) return id.error();
    auto digest = PasswordHasher::hash(r.password);
    if (!digest.ok()) return digest.error();
    a.accountId = id.value(); a.passwordHash = digest->hash; a.salt = digest->salt;
    TransactionGuard tx(data_.unitOfWork);
    if (!tx.started().ok()) return tx.started().error();
    auto inserted = data_.users.insert(a);
    if (!inserted.ok()) return inserted.error();
    auto committed = tx.commit();
    if (!committed.ok()) return committed.error();
    audit_.log("ACCOUNT_REGISTERED", a.accountId, "passenger");
    return a.accountId;
}
Result<Session> AuthService::login(const std::string& id, const std::string& password) {
    using R = Result<Session>;
    if (id.empty() || id.size() > 100 || password.size() > 128) return R::failure(ErrorCode::AuthInvalid);
    TransactionGuard tx(data_.unitOfWork); // serializes lockout counter updates
    if (!tx.started().ok()) return tx.started().error();
    auto found = data_.users.findByLoginId(InputValidator::normalizeEmail(id));
    if (!found.ok()) return found.error();
    Account a;
    if (found.value()) a = *found.value();
    const bool verified = PasswordHasher::verify(password, a.passwordHash, a.salt);
    if (!found.value()) {
        audit_.log("LOGIN_FAILED", "anonymous", "invalid credentials");
        return R::failure(ErrorCode::AuthInvalid);
    }
    if (a.lockedUntil > clock_.now()) {
        audit_.log("LOGIN_FAILED", a.accountId, "account locked");
        return R::failure(ErrorCode::AccountLocked);
    }
    if (!verified) {
        const int attempts = (a.lockedUntil != 0 ? 0 : a.failedAttempts) + 1;
        const EpochSeconds until = attempts >= 5 ? clock_.now() + 300 : 0;
        auto saved = data_.users.updateAttempts(a.accountId, attempts, until);
        if (!saved.ok()) return saved.error();
        auto committed = tx.commit(); if (!committed.ok()) return committed.error();
        audit_.log("LOGIN_FAILED", a.accountId, "invalid credentials");
        if (until) audit_.log("ACCOUNT_LOCKED", a.accountId, "five failed attempts");
        return R::failure(until ? ErrorCode::AccountLocked : ErrorCode::AuthInvalid);
    }
    auto session = Session::issue(a.accountId, a.role, clock_);
    if (!session.ok()) return session.error();
    auto saved = data_.users.updateAttempts(a.accountId, 0, 0);
    if (!saved.ok()) return saved.error();
    auto committed = tx.commit(); if (!committed.ok()) return committed.error();
    audit_.log("LOGIN_SUCCESS", a.accountId, "role=" + roleString(a.role));
    return session;
}
Status AuthService::logout(const Session& s) {
    s.revoke();
    audit_.log("LOGOUT", s.accountId, "session revoked");
    return Status::success();
}
}
