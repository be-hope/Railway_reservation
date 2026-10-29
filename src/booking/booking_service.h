// booking_service.h
// IBookingService - component C4, Reservation & Booking (owner M3).
// Interface version: 1. Sequence SD-01 (fare quote, then create reservation
// in one transaction), activity diagram AD-01.
//
// | Operation          | Role                      | Errors                        |
// |--------------------|---------------------------|-------------------------------|
// | checkAvailability  | anyone                    | E1001, E2001                  |
// | quoteFare          | anyone                    | E1001, E2001                  |
// | createBooking      | PASSENGER, STAFF (behalf) | E1001, E1005, E1006, E2001,   |
// |                    |                           | E3001, E9001                  |
// | getTicket          | owner, STAFF, ADMIN       | E1001, E1005, E1006, E4001    |

#pragma once

#include <optional>
#include <string>

#include "booking/booking_request_validator.h"
#include "booking/fare_policy.h"
#include "booking/pnr_generator.h"
#include "common/audit.h"
#include "common/clock.h"
#include "common/domain.h"
#include "common/result.h"
#include "common/session.h"
#include "dal/repositories.h"

namespace rrs {

struct FareQuote {
    std::string trainNumber;
    std::string sourceCode;
    std::string destinationCode;
    ClassCode classCode = ClassCode::SL;
    int distanceKm = 0;
    int travellers = 0;
    Money perTraveller;
    Money total;
};

class IBookingService {
public:
    virtual ~IBookingService() = default;

    // FR-009: confirmed seats free, RAC slots free and waitlist length,
    // computed from the stored reservations at query time.
    virtual Result<Availability> checkAvailability(const std::string& trainNumber,
                                                   const Date& journeyDate,
                                                   ClassCode classCode) = 0;

    // FR-011: total fare shown before confirmation.
    virtual Result<FareQuote> quoteFare(const std::string& trainNumber,
                                        const std::string& sourceCode,
                                        const std::string& destinationCode,
                                        ClassCode classCode, int travellers) = 0;

    // FR-012/013/014/018, NFR-004: creates a CONFIRMED, RAC or WAITLISTED
    // reservation in one transaction. A retry with the same requestId returns
    // the original reservation instead of creating a second one.
    virtual Result<Reservation> createBooking(const Session& session,
                                              const BookingRequest& request) = 0;

    // FR-014 ticket display: the reservation behind a PNR, for its owner or
    // for staff/administrators.
    virtual Result<Reservation> getTicket(const Session& session, const std::string& pnr) = 0;
};

class BookingService : public IBookingService {
public:
    BookingService(DataAccess data, const IClock& clock, IAuditLogger& audit,
                   PnrGenerator& pnrGenerator, FareRules fareRules = FareRules{});

    Result<Availability> checkAvailability(const std::string& trainNumber, const Date& journeyDate,
                                           ClassCode classCode) override;
    Result<FareQuote> quoteFare(const std::string& trainNumber, const std::string& sourceCode,
                                const std::string& destinationCode, ClassCode classCode,
                                int travellers) override;
    Result<Reservation> createBooking(const Session& session, const BookingRequest& request) override;
    Result<Reservation> getTicket(const Session& session, const std::string& pnr) override;

    // Distance between two stops of a train, if the train calls at source
    // before destination; E2001 otherwise.
    static Result<int> distanceBetween(const Train& train, const std::string& sourceCode,
                                       const std::string& destinationCode);

private:
    Result<Reservation> createInTransaction(const Session& session, const BookingRequest& request,
                                            const std::string& ownerId, const ClassConfig& config);
    // The reservation already created for this token, if any (E1001 if the
    // token belongs to another passenger's booking).
    Result<std::optional<Reservation>> existingForRequest(const std::string& requestId,
                                                          const std::string& ownerId);

    DataAccess data_;
    const IClock& clock_;
    IAuditLogger& audit_;
    PnrGenerator& pnrGenerator_;
    FareRules fareRules_;
};

}  // namespace rrs
