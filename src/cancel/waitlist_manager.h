// waitlist_manager.h
// Strict-order promotion of queued tickets when seats or RAC slots become
// free (FR-019, BR-07, ADR-005, state machine SM-01).
//
// The queue of one train/date/class is: RAC tickets in booking order, then
// WAITLISTED tickets in position order.
//   Phase 1 - confirm: walk the queue from the head; a ticket is CONFIRMED
//             (and gets seats) while its whole party fits the free seats.
//             The first ticket that does not fit stops the walk, so nobody
//             behind it can skip it.
//   Phase 2 - RAC:     walk the waitlist from the head; a ticket moves to RAC
//             while its party fits the free RAC slots; stop at the first
//             that does not fit.
//   Finally the remaining waitlist is renumbered 1, 2, 3 ... in order.
// A large party at the head can therefore block smaller tickets behind it;
// this is accepted and documented in ADR-005.

#pragma once

#include <string>
#include <vector>

#include "common/domain.h"
#include "common/result.h"
#include "dal/repositories.h"

namespace rrs {

struct PromotionPlan {
    std::vector<Reservation> changed;        // reservations whose status, position or seats changed
    std::vector<std::string> promotedPnrs;   // tickets that moved up a status (WL->RAC/CONFIRMED, RAC->CONFIRMED)
};

// Pure planning step: `active` holds every non-cancelled reservation of one
// trip. No I/O, so it is unit-testable (NFR-006).
PromotionPlan planPromotion(const ClassConfig& config, const std::vector<Reservation>& active);

class WaitlistManager {
public:
    WaitlistManager(IReservationRepository& reservations, IConfigRepository& config)
        : reservations_(reservations), config_(config) {}

    // Loads the trip, plans the promotion and writes the changes. Must be
    // called inside the caller's transaction so release and promotion are
    // atomic (AC-019). Returns the promoted PNRs.
    Result<std::vector<std::string>> promote(const std::string& trainNumber, const Date& journeyDate,
                                             ClassCode classCode);

private:
    IReservationRepository& reservations_;
    IConfigRepository& config_;
};

}  // namespace rrs
