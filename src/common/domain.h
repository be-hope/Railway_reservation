// domain.h
// Persisted domain records shared by the business and data-access layers
// (SAD CD-01, SRS Appendix A field layouts).
//
// NOTE: issues #9 (train/station), #25 (coach/seat/class-config) and #51
// (reservation/waitlist statuses) own the richer versions of some of these
// records; the fields here are the ones the booking module and the schema
// need. Change them only by pull request (SAD §4.3 contract rules).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/clock.h"
#include "common/date.h"
#include "common/money.h"

namespace rrs {

// ---------------------------------------------------------------- enums

enum class ClassCode { SL, AC3 };   // Sleeper, AC 3-Tier ("3A")

std::string classCodeString(ClassCode c);              // "SL", "3A"
std::string className(ClassCode c);                    // "Sleeper", "AC 3-Tier"
std::optional<ClassCode> parseClassCode(const std::string& s);
// Coach prefix used in seat numbers: SL -> "S" (S1-07), 3A -> "B" (B1-07).
std::string coachPrefix(ClassCode c);

// Ticket state machine SM-01: starts CONFIRMED, RAC or WAITLISTED; ends CANCELLED.
enum class TicketStatus { Confirmed, Rac, Waitlisted, Cancelled };

std::string ticketStatusString(TicketStatus s);        // "CONFIRMED" ... (stored form)
std::optional<TicketStatus> parseTicketStatus(const std::string& s);
bool isActive(TicketStatus s);                         // anything but CANCELLED

enum class Role { Passenger, Staff, Admin, Ops };

std::string roleString(Role r);
std::optional<Role> parseRole(const std::string& s);

// ---------------------------------------------------------------- trains

struct Station {
    std::string code;   // <= 5 chars, e.g. "SBC"
    std::string name;   // <= 50 chars
};

struct RouteStop {
    int seq = 0;                    // 1-based order on the route
    std::string stationCode;
    int arrivalMinutes = -1;        // minutes after midnight; -1 at the origin
    int departureMinutes = -1;      // -1 at the terminus
    int dayOffset = 0;              // 1 when the stop is reached the next day
    int distanceKm = 0;             // cumulative from the origin
};

struct Train {
    std::string number;             // <= 10 chars, e.g. "20001"
    std::string name;               // <= 60 chars
    bool active = true;
    std::vector<RouteStop> stops;   // ordered
};

// Coach / seat configuration of one class on one train (FR-022).
struct ClassConfig {
    std::string trainNumber;
    ClassCode classCode = ClassCode::SL;
    int coaches = 1;
    int seatsPerCoach = 1;
    int racQuota = 0;               // RAC berths (travellers)
    int waitlistCapacity = 20;      // waitlisted tickets
    int fareMultiplierPercent = 100;   // 100 = 1.0 (SL), 250 = 2.5 (3A)

    int capacity() const { return coaches * seatsPerCoach; }
};

// Seat availability for one train, date and class (FR-009).
struct Availability {
    std::string trainNumber;
    Date journeyDate;
    ClassCode classCode = ClassCode::SL;
    int capacity = 0;
    int confirmedFree = 0;
    int racQuota = 0;
    int racFree = 0;
    int waitlistCount = 0;          // tickets currently waitlisted
    int waitlistCapacity = 0;
};

// ---------------------------------------------------------------- accounts

struct Account {
    std::string accountId;          // UUID-like string
    Role role = Role::Passenger;
    std::string loginId;            // mobile number, e-mail or staff id
    std::string name;
    int age = 0;
    char gender = 'O';
    std::string contactNumber;
    std::string email;
    std::string passwordHash;       // PBKDF2 hash (hex) - never plain text
    std::string salt;
    std::string idProofType;
    std::string idProofNumber;
    int failedAttempts = 0;
    EpochSeconds lockedUntil = 0;
};

// ---------------------------------------------------------------- bookings

struct Traveller {
    std::string name;               // 1-100 chars
    int age = 0;                    // 1-120
    char gender = 'O';              // M, F or O
    std::string idProofNumber;      // <= 30 chars; masked on screen (SEC-005)
    std::string seatNumber;         // "S1-07"; empty unless CONFIRMED
};

struct Reservation {
    std::string pnr;                // 10 digits, first digit 1-9
    std::string requestId;          // idempotency token (ADR-004)
    std::string accountId;          // passenger who owns the ticket
    std::string bookedBy;           // account that created it (staff on behalf)
    std::string trainNumber;
    Date journeyDate;
    ClassCode classCode = ClassCode::SL;
    std::string sourceCode;
    std::string destinationCode;
    TicketStatus status = TicketStatus::Confirmed;
    int waitlistPosition = 0;       // > 0 only when WAITLISTED
    Money fare;
    std::optional<Money> refund;
    std::int64_t queueSeq = 0;      // global booking order; ranks the RAC queue
    EpochSeconds bookedAt = 0;
    EpochSeconds cancelledAt = 0;
    std::vector<Traveller> travellers;

    int travellerCount() const { return static_cast<int>(travellers.size()); }
};

// Shows only the last 4 characters of an ID-proof number (SEC-005).
std::string maskIdProof(const std::string& idProofNumber);

}  // namespace rrs
