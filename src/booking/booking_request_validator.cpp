// booking_request_validator.cpp

#include "booking/booking_request_validator.h"

#include <cctype>

namespace rrs {

namespace {

Status invalid(const std::string& message) {
    return Status::failure(ErrorCode::Validation, message);
}

bool isPrintableText(const std::string& s) {
    for (char c : s) {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) return false;
    }
    return true;
}

bool isBlank(const std::string& s) {
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// Letters, digits and '-' / '/' only (ID-proof and request tokens).
bool isToken(const std::string& s, bool allowSlash) {
    for (char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!std::isalnum(u) && c != '-' && !(allowSlash && c == '/')) return false;
    }
    return true;
}

bool isStationCode(const std::string& s) {
    if (s.size() < 2 || s.size() > 5) return false;
    for (char c : s) {
        if (!std::isupper(static_cast<unsigned char>(c))) return false;
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
    if (t.name.empty() || isBlank(t.name) || t.name.size() > 100 || !isPrintableText(t.name)) {
        return invalid(who + "Invalid name: 1 to 100 printable characters.");
    }
    if (t.age < 1 || t.age > 120) {
        return invalid(who + "Invalid age: must be between 1 and 120.");
    }
    if (t.gender != 'M' && t.gender != 'F' && t.gender != 'O') {
        return invalid(who + "Invalid gender: enter M, F or O.");
    }
    if (t.idProofNumber.empty() || t.idProofNumber.size() > 30 || !isToken(t.idProofNumber, true)) {
        return invalid(who + "Invalid ID proof: 1 to 30 letters, digits, '-' or '/'.");
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
    if (!isStationCode(r.sourceCode) || !isStationCode(r.destinationCode)) {
        return invalid("Invalid station code: 2 to 5 capital letters.");
    }
    if (r.sourceCode == r.destinationCode) {
        return invalid("Invalid journey: source and destination are the same.");
    }
    if (r.journeyDate < today || r.journeyDate > today.plusDays(kBookingWindowDays)) {
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
