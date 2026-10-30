#include "admin/reservation_admin_service.h"
#include "security/input_validator.h"
namespace rrs {
Result<Reservation> ReservationAdminService::bookOnBehalf(const Session& s, const BookingRequest& request) {
    auto allowed = requireRole(s, {Role::Staff}, clock_);
    if (!allowed.ok()) { audit_.log("UNAUTHORIZED_ATTEMPT", s.accountId, "staff booking"); return allowed.error(); }
    return booking_.createBooking(s, request);
}
Result<Reservation> ReservationAdminService::correctReservation(const Session& s, const std::string& pnr, const std::vector<Traveller>& corrections) {
    using R = Result<Reservation>;
    auto allowed = access_.require(s, Permission::EditReservation); if (!allowed.ok()) return allowed.error();
    auto valid = InputValidator::pnr(pnr); if (!valid.ok()) return valid.error();
    TransactionGuard tx(data_.unitOfWork); if (!tx.started().ok()) return tx.started().error();
    auto found = data_.reservations.findByPnr(pnr); if (!found.ok()) return found.error();
    if (!found.value()) return R::failure(ErrorCode::PnrNotFound);
    Reservation r = *found.value();
    if (r.status == TicketStatus::Cancelled) return R::failure(ErrorCode::AlreadyCancelled);
    if (r.journeyDate < clock_.today()) return R::failure(ErrorCode::CancelDeparted);
    auto train = data_.trains.findTrain(r.trainNumber); if (!train.ok()) return train.error();
    if (!train.value()) return R::failure(ErrorCode::TrainNotFound);
    for (const auto& stop : train.value()->stops) if (stop.stationCode == r.sourceCode) {
        const auto departureDate = r.journeyDate.plusDays(stop.dayOffset);
        if (departureDate < clock_.today() || (departureDate == clock_.today() &&
            clock_.localSecondsSinceMidnight() >= stop.departureMinutes * 60)) return R::failure(ErrorCode::CancelDeparted);
    }
    if (corrections.size() != r.travellers.size()) return R::failure(ErrorCode::Validation, "Traveller count cannot be changed.");
    for (std::size_t i = 0; i < corrections.size(); ++i) {
        auto check = validateTraveller(corrections[i], static_cast<int>(i + 1)); if (!check.ok()) return check.error();
        auto seat = r.travellers[i].seatNumber;
        r.travellers[i] = corrections[i]; r.travellers[i].seatNumber = seat;
    }
    auto saved = data_.reservations.update(r); if (!saved.ok()) return saved.error();
    auto committed = tx.commit(); if (!committed.ok()) return committed.error();
    audit_.log("RESERVATION_CORRECTED", s.accountId, "pnr=" + pnr);
    return r;
}
}
