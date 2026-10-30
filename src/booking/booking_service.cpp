// booking_service.cpp

#include "booking/booking_service.h"

#include "booking/availability.h"
#include "booking/seat_allocator.h"

namespace rrs {

BookingService::BookingService(DataAccess data, const IClock& clock, IAuditLogger& audit,
                               PnrGenerator& pnrGenerator, FareRules fareRules)
    : data_(data), clock_(clock), audit_(audit), pnrGenerator_(pnrGenerator), fareRules_(fareRules) {}

Result<int> BookingService::distanceBetween(const Train& train, const std::string& sourceCode,
                                            const std::string& destinationCode) {
    const RouteStop* from = nullptr;
    for (const RouteStop& stop : train.stops) {
        if (stop.stationCode == sourceCode) from = &stop;
        if (stop.stationCode == destinationCode) {
            if (from == nullptr) break;   // destination comes before the source
            return stop.distanceKm - from->distanceKm;
        }
    }
    return Result<int>::failure(ErrorCode::TrainNotFound,
                                "train " + train.number + " does not run " + sourceCode + " to " +
                                    destinationCode);
}

Result<Availability> BookingService::checkAvailability(const std::string& trainNumber,
                                                       const Date& journeyDate, ClassCode classCode) {
    auto config = data_.config.findClassConfig(trainNumber, classCode);
    if (!config.ok()) return config.error();
    if (!config.value()) {
        return Result<Availability>::failure(ErrorCode::TrainNotFound,
                                             "class " + classCodeString(classCode) + " not configured");
    }
    auto runs = data_.trains.hasSchedule(trainNumber, journeyDate);
    if (!runs.ok()) return runs.error();
    if (!runs.value()) {
        return Result<Availability>::failure(ErrorCode::TrainNotFound,
                                             "no service on " + journeyDate.display());
    }
    auto occupancy = data_.reservations.occupancy(trainNumber, journeyDate, classCode);
    if (!occupancy.ok()) return occupancy.error();
    return computeAvailability(*config.value(), journeyDate, occupancy.value());
}

Result<FareQuote> BookingService::quoteFare(const std::string& trainNumber,
                                            const std::string& sourceCode,
                                            const std::string& destinationCode, ClassCode classCode,
                                            int travellers) {
    using R = Result<FareQuote>;
    if (travellers < kMinTravellers || travellers > kMaxTravellers) {
        return R::failure(ErrorCode::Validation, "Invalid number of travellers: 1 to 6 per booking.");
    }
    auto train = data_.trains.findTrain(trainNumber);
    if (!train.ok()) return train.error();
    if (!train.value() || !train.value()->active) return R::failure(ErrorCode::TrainNotFound);

    auto config = data_.config.findClassConfig(trainNumber, classCode);
    if (!config.ok()) return config.error();
    if (!config.value()) {
        return R::failure(ErrorCode::TrainNotFound, "class " + classCodeString(classCode) + " not configured");
    }
    auto km = distanceBetween(*train.value(), sourceCode, destinationCode);
    if (!km.ok()) return km.error();

    FareQuote q;
    q.trainNumber = trainNumber;
    q.sourceCode = sourceCode;
    q.destinationCode = destinationCode;
    q.classCode = classCode;
    q.distanceKm = km.value();
    q.travellers = travellers;
    q.perTraveller = farePerTraveller(km.value(), config.value()->fareMultiplierPercent, fareRules_);
    q.total = q.perTraveller * travellers;
    return q;
}

Result<std::optional<Reservation>> BookingService::existingForRequest(const std::string& requestId,
                                                                      const std::string& ownerId) {
    using R = Result<std::optional<Reservation>>;
    auto existing = data_.reservations.findByRequestId(requestId);
    if (!existing.ok()) return existing.error();
    if (existing.value() && existing.value()->accountId != ownerId) {
        return R::failure(ErrorCode::Validation, "Invalid request id: already used by another booking.");
    }
    return existing.value();
}

Result<Reservation> BookingService::createBooking(const Session& session, const BookingRequest& request) {
    using R = Result<Reservation>;

    // 1. Access control (SEC-002, SEC-004).
    Status access = requireRole(session, {Role::Passenger, Role::Staff}, clock_);
    if (!access.ok()) {
        if (access.code() == ErrorCode::Forbidden) {
            audit_.log("UNAUTHORIZED_ATTEMPT", session.accountId, "createBooking");
        }
        return access.error();
    }
    std::string ownerId = session.accountId;
    if (session.role == Role::Staff) {
        if (request.onBehalfOf.empty()) {
            return R::failure(ErrorCode::Validation, "Passenger account is required for a staff booking.");
        }
        auto passenger = data_.users.findById(request.onBehalfOf);
        if (!passenger.ok()) return passenger.error();
        if (!passenger.value() || passenger.value()->role != Role::Passenger) {
            return R::failure(ErrorCode::Validation, "Invalid passenger account.");
        }
        ownerId = request.onBehalfOf;
    } else if (!request.onBehalfOf.empty() && request.onBehalfOf != session.accountId) {
        audit_.log("UNAUTHORIZED_ATTEMPT", session.accountId, "book on behalf");
        return R::failure(ErrorCode::Forbidden);
    }

    // 2. Idempotency (NFR-004, ADR-004): a retry returns the original PNR.
    //    Checked before validation so a retry after the journey date has
    //    rolled past "today" still finds its ticket.
    if (!request.requestId.empty() && request.requestId.size() <= 36) {
        auto existing = existingForRequest(request.requestId, ownerId);
        if (!existing.ok()) return existing.error();
        if (existing.value()) return *existing.value();
    }

    // 3. Field validation (FR-010) - nothing reaches the database before this.
    Status valid = validateBookingRequest(request, clock_.today());
    if (!valid.ok()) return valid.error();

    // 4. The passenger must have accepted the displayed fare (FR-012).
    if (!request.fareAccepted) {
        return R::failure(ErrorCode::Validation, "Fare not accepted: no booking was made.");
    }
    auto quote = quoteFare(request.trainNumber, request.sourceCode, request.destinationCode,
                           request.classCode, static_cast<int>(request.travellers.size()));
    if (!quote.ok()) return quote.error();
    if (quote.value().total != request.acceptedFare) {
        return R::failure(ErrorCode::Validation,
                          "The fare is now " + quote.value().total.toString() +
                              ". Please review and accept it again.");
    }

    // 5. The train must run on that date in that class.
    auto runs = data_.trains.hasSchedule(request.trainNumber, request.journeyDate);
    if (!runs.ok()) return runs.error();
    if (!runs.value()) return R::failure(ErrorCode::TrainNotFound, "no service on " + request.journeyDate.display());
    auto config = data_.config.findClassConfig(request.trainNumber, request.classCode);
    if (!config.ok()) return config.error();
    if (!config.value()) return R::failure(ErrorCode::TrainNotFound);

    // 6. Status decision, seats, PNR and insert in one transaction.
    R created = createInTransaction(session, request, ownerId, *config.value());
    if (created.ok()) {
        const Reservation& r = created.value();
        audit_.log("BOOKING_CREATED", session.accountId,
                   "pnr=" + r.pnr + " status=" + ticketStatusString(r.status) +
                       " train=" + r.trainNumber + " date=" + r.journeyDate.iso());
    }
    return created;
}

Result<Reservation> BookingService::createInTransaction(const Session& session,
                                                        const BookingRequest& request,
                                                        const std::string& ownerId,
                                                        const ClassConfig& config) {
    TransactionGuard tx(data_.unitOfWork);
    if (!tx.started().ok()) return tx.started().error();

    // Re-check the token inside the transaction (a concurrent retry may have won).
    auto existing = existingForRequest(request.requestId, ownerId);
    if (!existing.ok()) return existing.error();
    if (existing.value()) return *existing.value();

    auto occupancy = data_.reservations.occupancy(request.trainNumber, request.journeyDate,
                                                  request.classCode);
    if (!occupancy.ok()) return occupancy.error();
    const Availability availability =
        computeAvailability(config, request.journeyDate, occupancy.value());
    const int n = static_cast<int>(request.travellers.size());

    auto decision = decideStatus(availability, occupancy.value().racTravellers, n);
    if (!decision.ok()) return decision.error();

    Reservation r;
    r.requestId = request.requestId;
    r.accountId = ownerId;
    r.bookedBy = session.accountId;
    r.trainNumber = request.trainNumber;
    r.journeyDate = request.journeyDate;
    r.classCode = request.classCode;
    r.sourceCode = request.sourceCode;
    r.destinationCode = request.destinationCode;
    r.status = decision.value().status;
    r.waitlistPosition = decision.value().waitlistPosition;
    r.fare = request.acceptedFare;
    r.bookedAt = clock_.now();
    r.travellers = request.travellers;
    for (Traveller& t : r.travellers) t.seatNumber.clear();

    if (r.status == TicketStatus::Confirmed) {
        auto seats = SeatAllocator::allocate(config, occupancy.value().occupiedSeats, n);
        if (!seats.ok()) return seats.error();
        for (int i = 0; i < n; ++i) r.travellers[i].seatNumber = seats.value()[i];
    }

    auto seq = data_.reservations.nextQueueSeq();
    if (!seq.ok()) return seq.error();
    r.queueSeq = seq.value();

    auto pnr = pnrGenerator_.generate(
        [this](const std::string& candidate) { return data_.reservations.pnrExists(candidate); });
    if (!pnr.ok()) return pnr.error();
    r.pnr = pnr.value();

    Status inserted = data_.reservations.insert(r);
    if (!inserted.ok()) return inserted.error();   // guard rolls back
    Status committed = tx.commit();
    if (!committed.ok()) return committed.error();
    return r;
}

Result<Reservation> BookingService::getTicket(const Session& session, const std::string& pnr) {
    using R = Result<Reservation>;
    Status access = requireRole(session, {Role::Passenger, Role::Staff, Role::Admin}, clock_);
    if (!access.ok()) return access.error();
    if (!PnrGenerator::isValidPnr(pnr)) {
        return R::failure(ErrorCode::Validation, "Invalid PNR: enter the 10-digit number on your ticket.");
    }
    auto found = data_.reservations.findByPnr(pnr);
    if (!found.ok()) return found.error();
    if (!found.value()) return R::failure(ErrorCode::PnrNotFound);
    if (session.role == Role::Passenger && found.value()->accountId != session.accountId) {
        audit_.log("UNAUTHORIZED_ATTEMPT", session.accountId, "view pnr=" + pnr);
        return R::failure(ErrorCode::Forbidden);
    }
    return *found.value();
}

}  // namespace rrs
