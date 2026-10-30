// availability.h
// Seat availability (FR-009) and the booking status decision (FR-012,
// FR-018, BR-01, SM-01). Pure functions - unit-testable without a database.
//
// Status decision table for a new booking of n travellers:
//   1. CONFIRMED  if n <= confirmed seats free AND nobody is queued (RAC or WL)
//   2. RAC        if n <= RAC slots free       AND nobody is waitlisted
//   3. WAITLISTED if waitlisted tickets < waitlist capacity (position = count + 1)
//   4. otherwise  ERR-NO-CAPACITY (E3001), nothing stored
// The "nobody queued" conditions stop a new booking from overtaking tickets
// that are already waiting (BR-07): a waiting party that does not fit a freed
// seat keeps the head of the queue until enough seats are free.

#pragma once

#include "common/domain.h"
#include "common/result.h"
#include "dal/repositories.h"

namespace rrs {

Availability computeAvailability(const ClassConfig& config, const Date& journeyDate,
                                 const TripOccupancy& occupancy);

struct StatusDecision {
    TicketStatus status = TicketStatus::Confirmed;
    int waitlistPosition = 0;   // set only for WAITLISTED
};

Result<StatusDecision> decideStatus(const Availability& availability, int racTravellersQueued,
                                    int travellers);

}  // namespace rrs
