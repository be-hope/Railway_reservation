#pragma once
#include "auth/password_hasher.h"
#include "common/audit.h"
#include "common/session.h"
#include "dal/repositories.h"
namespace rrs {
struct Registration { std::string mobile, email, password, name; };
class AuthService {
public:
    AuthService(DataAccess data, const IClock& clock, IAuditLogger& audit) : data_(data), clock_(clock), audit_(audit) {}
    Result<std::string> registerPassenger(const Registration& request);
    Result<Session> login(const std::string& loginId, const std::string& password);
    Status logout(const Session& session);
private:
    DataAccess data_;
    const IClock& clock_;
    IAuditLogger& audit_;
};
}
