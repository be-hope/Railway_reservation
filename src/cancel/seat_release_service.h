// seat_release_service.h
// Releases the seats of a cancelled reservation and promotes the waiting
// queue in the same transaction (FR-016, FR-019, AC-019, SD-02b).
//
// Division of work with CancellationService (issues #46-#48): that service
// checks ownership, the departure time and computes the refund; then it calls
// cancelAndRelease() with the refund. This class owns the state change:
//   status -> CANCELLED, seat numbers -> NULL (seat free again), waitlist
//   renumbered, head-of-queue tickets promoted, TICKET_CANCELLED audited.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cancel/waitlist_manager.h"
#include "common/audit.h"
#include "common/clock.h"
#include "dal/repositories.h"

namespace rrs {

struct ReleaseResult {
    Reservation cancelled;                   // as stored after the change
    int seatsReleased = 0;
    std::vector<std::string> promotedPnrs;   // promoted in the same transaction
};

class SeatReleaseService {
public:
    SeatReleaseService(DataAccess data, const IClock& clock, IAuditLogger& audit);

    // Runs in its own transaction. E4001 unknown PNR, E4002 already cancelled.
    Result<ReleaseResult> cancelAndRelease(const std::string& pnr, std::optional<Money> refund,
                                           const std::string& actorId);

    // Same work for a caller that already holds a transaction.
    Result<ReleaseResult> releaseInTransaction(const std::string& pnr, std::optional<Money> refund);

private:
    DataAccess data_;
    const IClock& clock_;
    IAuditLogger& audit_;
    WaitlistManager waitlist_;
};

}  // namespace rrs
