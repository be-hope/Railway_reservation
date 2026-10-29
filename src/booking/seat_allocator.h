// seat_allocator.h
// Seat/berth allocation (FR-013, UT-BKG-04). Pure logic - no I/O (NFR-006).
//
// Seats are numbered coach by coach: S1-01, S1-02, ..., S1-72, S2-01, ...
// (prefix S for Sleeper, B for AC 3-Tier). allocate() always hands out the
// lowest free seat numbers in that order, and never a seat that is already
// occupied, so allocation is ordered and collision-free. The database index
// ux_seat backs this up if two allocations ever race.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "common/domain.h"
#include "common/result.h"

namespace rrs {

struct SeatPosition {
    int coach = 0;   // 1-based
    int seat = 0;    // 1-based within the coach
};

// "S1-07" for coach 1 seat 7 of a Sleeper with up to 99 seats per coach.
std::string seatLabel(ClassCode classCode, int coach, int seat, int seatsPerCoach);
// Parses a label of this class; nullopt for a malformed label or wrong class.
std::optional<SeatPosition> parseSeatLabel(ClassCode classCode, const std::string& label);
// True when the label names a seat that exists in this configuration.
bool seatExists(const ClassConfig& config, const std::string& label);

class SeatAllocator {
public:
    // Returns `count` free seats (lowest first) or E3001 when fewer are free.
    static Result<std::vector<std::string>> allocate(const ClassConfig& config,
                                                     const std::vector<std::string>& occupied,
                                                     int count);
};

}  // namespace rrs
