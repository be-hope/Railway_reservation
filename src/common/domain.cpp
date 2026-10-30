// domain.cpp

#include "common/domain.h"

namespace rrs {

std::string classCodeString(ClassCode c) {
    return c == ClassCode::SL ? "SL" : "3A";
}

std::string className(ClassCode c) {
    return c == ClassCode::SL ? "Sleeper" : "AC 3-Tier";
}

std::optional<ClassCode> parseClassCode(const std::string& s) {
    if (s == "SL" || s == "sl") return ClassCode::SL;
    if (s == "3A" || s == "3a") return ClassCode::AC3;
    return std::nullopt;
}

std::string coachPrefix(ClassCode c) {
    return c == ClassCode::SL ? "S" : "B";
}

std::string ticketStatusString(TicketStatus s) {
    switch (s) {
        case TicketStatus::Confirmed:  return "CONFIRMED";
        case TicketStatus::Rac:        return "RAC";
        case TicketStatus::Waitlisted: return "WAITLISTED";
        case TicketStatus::Cancelled:  return "CANCELLED";
    }
    return "CANCELLED";
}

std::optional<TicketStatus> parseTicketStatus(const std::string& s) {
    if (s == "CONFIRMED") return TicketStatus::Confirmed;
    if (s == "RAC") return TicketStatus::Rac;
    if (s == "WAITLISTED") return TicketStatus::Waitlisted;
    if (s == "CANCELLED") return TicketStatus::Cancelled;
    return std::nullopt;
}

bool isActive(TicketStatus s) { return s != TicketStatus::Cancelled; }

std::string roleString(Role r) {
    switch (r) {
        case Role::Passenger: return "PASSENGER";
        case Role::Staff:     return "STAFF";
        case Role::Admin:     return "ADMIN";
        case Role::Ops:       return "OPS";
    }
    return "PASSENGER";
}

std::optional<Role> parseRole(const std::string& s) {
    if (s == "PASSENGER") return Role::Passenger;
    if (s == "STAFF") return Role::Staff;
    if (s == "ADMIN") return Role::Admin;
    if (s == "OPS") return Role::Ops;
    return std::nullopt;
}

std::string maskIdProof(const std::string& idProofNumber) {
    if (idProofNumber.size() <= 4) return std::string(idProofNumber.size(), '*');
    return std::string(idProofNumber.size() - 4, '*') +
           idProofNumber.substr(idProofNumber.size() - 4);
}

}  // namespace rrs
