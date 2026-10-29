// availability.cpp

#include "booking/availability.h"

#include <algorithm>

namespace rrs {

Availability computeAvailability(const ClassConfig& config, const Date& journeyDate,
                                 const TripOccupancy& occupancy) {
    Availability a;
    a.trainNumber = config.trainNumber;
    a.journeyDate = journeyDate;
    a.classCode = config.classCode;
    a.capacity = config.capacity();
    a.confirmedFree = std::max(0, config.capacity() - occupancy.confirmedTravellers);
    a.racQuota = config.racQuota;
    a.racFree = std::max(0, config.racQuota - occupancy.racTravellers);
    a.waitlistCount = occupancy.waitlistTickets;
    a.waitlistCapacity = config.waitlistCapacity;
    return a;
}

Result<StatusDecision> decideStatus(const Availability& a, int racTravellersQueued, int travellers) {
    using R = Result<StatusDecision>;
    if (travellers <= 0) return R::failure(ErrorCode::Validation, "At least one traveller is required.");

    const bool nobodyWaitlisted = a.waitlistCount == 0;
    const bool nobodyQueued = nobodyWaitlisted && racTravellersQueued == 0;

    if (nobodyQueued && travellers <= a.confirmedFree) {
        return StatusDecision{TicketStatus::Confirmed, 0};
    }
    if (nobodyWaitlisted && travellers <= a.racFree) {
        return StatusDecision{TicketStatus::Rac, 0};
    }
    if (a.waitlistCount < a.waitlistCapacity) {
        return StatusDecision{TicketStatus::Waitlisted, a.waitlistCount + 1};
    }
    return R::failure(ErrorCode::NoCapacity);
}

}  // namespace rrs
