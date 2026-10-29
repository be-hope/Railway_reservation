// seat_release_service.cpp

#include "cancel/seat_release_service.h"

#include "booking/pnr_generator.h"

namespace rrs {

SeatReleaseService::SeatReleaseService(DataAccess data, const IClock& clock, IAuditLogger& audit)
    : data_(data), clock_(clock), audit_(audit), waitlist_(data.reservations, data.config) {}

Result<ReleaseResult> SeatReleaseService::releaseInTransaction(const std::string& pnr,
                                                               std::optional<Money> refund) {
    using R = Result<ReleaseResult>;
    if (!PnrGenerator::isValidPnr(pnr)) {
        return R::failure(ErrorCode::Validation, "Invalid PNR: enter the 10-digit number on your ticket.");
    }
    auto found = data_.reservations.findByPnr(pnr);
    if (!found.ok()) return found.error();
    if (!found.value()) return R::failure(ErrorCode::PnrNotFound);
    Reservation r = *found.value();
    if (r.status == TicketStatus::Cancelled) return R::failure(ErrorCode::AlreadyCancelled);

    ReleaseResult result;
    for (Traveller& t : r.travellers) {
        if (!t.seatNumber.empty()) ++result.seatsReleased;
        t.seatNumber.clear();
    }
    r.status = TicketStatus::Cancelled;
    r.waitlistPosition = 0;
    r.refund = refund;
    r.cancelledAt = clock_.now();
    Status s = data_.reservations.update(r);
    if (!s.ok()) return s.error();

    // Freed seats, RAC slots or a waitlist position: re-plan the whole queue.
    auto promoted = waitlist_.promote(r.trainNumber, r.journeyDate, r.classCode);
    if (!promoted.ok()) return promoted.error();
    result.promotedPnrs = promoted.value();
    result.cancelled = r;
    return result;
}

Result<ReleaseResult> SeatReleaseService::cancelAndRelease(const std::string& pnr, std::optional<Money> refund,
                                                           const std::string& actorId) {
    TransactionGuard tx(data_.unitOfWork);
    if (!tx.started().ok()) return tx.started().error();
    auto result = releaseInTransaction(pnr, refund);
    if (!result.ok()) return result;   // guard rolls back
    Status committed = tx.commit();
    if (!committed.ok()) return committed.error();

    std::string detail = "pnr=" + pnr + " seats_released=" + std::to_string(result.value().seatsReleased);
    for (const std::string& p : result.value().promotedPnrs) detail += " promoted=" + p;
    audit_.log("TICKET_CANCELLED", actorId, detail);
    return result;
}

}  // namespace rrs
