#pragma once

#include <string>

#include "booking/booking_service.h"
#include "cancel/seat_release_service.h"
#include "common/clock.h"
#include "dal/repositories.h"

namespace rrs {

class CancellationService {
public:
    CancellationService(IBookingService& booking, ITrainRepository& trains,
                        SeatReleaseService& release, const IClock& clock);

    Result<Reservation> cancel(const Session& session, const std::string& pnr);

    // Pure eligibility rule, independently testable without database I/O.
    static Status validateEligibility(TicketStatus status,
                                      EpochSeconds departureAt,
                                      EpochSeconds now);

private:
    Result<EpochSeconds> departureTime(const Reservation& reservation) const;

    IBookingService& booking_;
    ITrainRepository& trains_;
    SeatReleaseService& release_;
    const IClock& clock_;
};

}  // namespace rrs
