// booking_request_validator.cpp

#include "booking/booking_request_validator.h"

#include <cctype>
#include "security/input_validator.h"

namespace rrs {

namespace {

Status invalid(const std::string& message) {
    return Status::failure(ErrorCode::Validation, message);
}

// Letters, digits and '-' / '/' only (ID-proof and request tokens).
bool isToken(const std::string& s, bool allowSlash) {
    for (char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!std::isalnum(u) && c != '-' && !(allowSlash && c == '/')) return false;
    }
    return true;
}

bool isTrainNumber(const std::string& s) {
    if (s.empty() || s.size() > 10) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

}  // namespace

Status validateTraveller(const Traveller& t, int position) {
    const std::string who = "Traveller " + std::to_string(position) + ": ";
    for (const auto& s : {InputValidator::name(t.name), InputValidator::age(t.age),
                           InputValidator::gender(t.gender), InputValidator::idProof(t.idProofNumber)}) {
        if (!s.ok()) return invalid(who + s.error().detail);
    }
    return Status::success();
}

Status validateBookingRequest(const BookingRequest& r, const Date& today) {
    if (r.requestId.empty() || r.requestId.size() > 36 || !isToken(r.requestId, false)) {
        return invalid("Invalid request id.");
    }
    if (!isTrainNumber(r.trainNumber)) {
        return invalid("Invalid train number: up to 10 digits.");
    }
    if (!InputValidator::station(r.sourceCode).ok() || !InputValidator::station(r.destinationCode).ok()) {
        return invalid("Invalid station code: 2 to 5 capital letters.");
    }
    if (r.sourceCode == r.destinationCode) {
        return invalid("Invalid journey: source and destination are the same.");
    }
    if (!InputValidator::journeyDate(r.journeyDate, today).ok()) {
        return invalid("Invalid journey date: choose a date from today to today + 120 days.");
    }
    const int n = static_cast<int>(r.travellers.size());
    if (n < kMinTravellers || n > kMaxTravellers) {
        return invalid("Invalid number of travellers: 1 to 6 per booking.");
    }
    for (int i = 0; i < n; ++i) {
        Status s = validateTraveller(r.travellers[i], i + 1);
        if (!s.ok()) return s;
    }
    return Status::success();
}

}  // namespace rrs
