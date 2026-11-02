#include "cancel/cancellation_service.h"

#include <ctime>

#include "booking/pnr_generator.h"
#include "cancel/refund_calculator.h"

namespace rrs {

CancellationService::CancellationService(IBookingService& booking,
                                         ITrainRepository& trains,
                                         SeatReleaseService& release,
                                         const IClock& clock)
    : booking_(booking), trains_(trains), release_(release), clock_(clock) {}

Status CancellationService::validateEligibility(
    TicketStatus status, EpochSeconds departureAt, EpochSeconds now) {
    if (status == TicketStatus::Cancelled) {
        return Status::failure(ErrorCode::AlreadyCancelled);
    }
    if (departureAt <= now) {
        return Status::failure(ErrorCode::CancelDeparted);
    }
    return Status::success();
}

Result<EpochSeconds> CancellationService::departureTime(
    const Reservation& reservation) const {

    auto found = trains_.findTrain(reservation.trainNumber);
    if (!found.ok()) return found.error();
    if (!found.value()) {
        return Result<EpochSeconds>::failure(ErrorCode::TrainNotFound);
    }

    for (const RouteStop& stop : found.value()->stops) {
        if (stop.stationCode != reservation.sourceCode) continue;

        if (stop.departureMinutes < 0 || stop.departureMinutes >= 24 * 60) {
            return Result<EpochSeconds>::failure(
                ErrorCode::Validation, "Invalid scheduled departure time.");
        }

        const Date date = reservation.journeyDate.plusDays(stop.dayOffset);

        std::tm local{};
        local.tm_year = date.year() - 1900;
        local.tm_mon = date.month() - 1;
        local.tm_mday = date.day();
        local.tm_hour = stop.departureMinutes / 60;
        local.tm_min = stop.departureMinutes % 60;
        local.tm_sec = 0;
        local.tm_isdst = -1;

        const std::time_t timestamp = std::mktime(&local);
        if (timestamp == static_cast<std::time_t>(-1)) {
            return Result<EpochSeconds>::failure(
                ErrorCode::Internal, "Could not determine scheduled departure time.");
        }

        return static_cast<EpochSeconds>(timestamp);
    }

    return Result<EpochSeconds>::failure(
        ErrorCode::TrainNotFound,
        "Boarding station not found on the train route.");
}

Result<Reservation> CancellationService::cancel(
    const Session& session, const std::string& pnr) {

    using R = Result<Reservation>;

    Status access = requireRole(session, {Role::Passenger}, clock_);
    if (!access.ok()) return access.error();

    if (!PnrGenerator::isValidPnr(pnr)) {
        return R::failure(
            ErrorCode::Validation,
            "Invalid PNR: enter the 10-digit number on your ticket.");
    }

    // Checks session, PNR existence and passenger ownership.
    auto ticket = booking_.getTicket(session, pnr);
    if (!ticket.ok()) return ticket.error();

    const Reservation& reservation = ticket.value();

    auto departure = departureTime(reservation);
    if (!departure.ok()) return departure.error();

    Status eligibility = validateEligibility(
        reservation.status, departure.value(), clock_.now());
    if (!eligibility.ok()) return eligibility.error();

    const EpochSeconds remaining = departure.value() - clock_.now();

    const Money refund = RefundCalculator::calculate(
        reservation.fare, reservation.status, remaining);

    // SeatReleaseService owns the transaction, reservation update,
    // seat release, waitlist promotion and cancellation audit event.
    auto result = release_.cancelAndRelease(pnr, refund, session.accountId);
    if (!result.ok()) return result.error();

    return result.value().cancelled;
}

}  // namespace rrs
