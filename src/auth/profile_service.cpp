#include "auth/profile_service.h"
#include <algorithm>
namespace rrs {
Result<Account> ProfileService::view(const Session& s) {
    auto allowed = access_.require(s, Permission::Profile);
    if (!allowed.ok()) return allowed.error();
    auto found = users_.findById(s.accountId);
    if (!found.ok()) return found.error();
    if (!found.value()) return Result<Account>::failure(ErrorCode::Forbidden);
    Account a = *found.value();
    a.passwordHash.clear(); a.salt.clear();
    a.idProofNumber = maskIdProof(a.idProofNumber);
    return a;
}
Status ProfileService::update(const Session& s, const Account& changes) {
    auto allowed = access_.require(s, Permission::Profile, changes.accountId);
    if (!allowed.ok()) return allowed;
    if (changes.accountId != s.accountId) return Status::failure(ErrorCode::Forbidden);
    auto found = users_.findById(s.accountId);
    if (!found.ok()) return found.error();
    if (!found.value()) return Status::failure(ErrorCode::Forbidden);
    Account a = *found.value(); // copy only editable fields, never role/credentials/attempts
    a.name = changes.name; a.age = changes.age; a.gender = changes.gender;
    a.contactNumber = changes.contactNumber; a.email = InputValidator::normalizeEmail(changes.email);
    a.idProofType = changes.idProofType;
    if (changes.idProofNumber != maskIdProof(a.idProofNumber)) a.idProofNumber = changes.idProofNumber;
    auto valid = InputValidator::profile(a); if (!valid.ok()) return valid;
    auto saved = users_.updateProfile(a);
    if (saved.ok()) audit_.log("PROFILE_UPDATED", s.accountId, "profile fields updated");
    return saved;
}
Result<std::vector<Reservation>> BookingHistoryService::history(const Session& s) {
    auto allowed = access_.require(s, Permission::History);
    if (!allowed.ok()) return allowed.error();
    auto rows = reservations_.listForAccount(s.accountId);
    if (!rows.ok()) return rows.error();
    // Enforce ownership and ordering even if a repository adapter is faulty.
    auto& values = rows.value();
    values.erase(std::remove_if(values.begin(), values.end(), [&](const Reservation& r) { return r.accountId != s.accountId; }), values.end());
    std::sort(values.begin(), values.end(), [](const Reservation& a, const Reservation& b) {
        return a.journeyDate == b.journeyDate ? a.pnr < b.pnr : a.journeyDate < b.journeyDate;
    });
    for (auto& r : values) for (auto& t : r.travellers) t.idProofNumber = maskIdProof(t.idProofNumber);
    return rows;
}
}
