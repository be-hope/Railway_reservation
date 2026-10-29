// ticket_view.h
// Text screens for availability, fare and the ticket (FR-009, FR-011, FR-014).
// Lines fit in 80 columns; ID-proof numbers are masked (SEC-005).

#pragma once

#include <string>

#include "booking/booking_service.h"
#include "common/domain.h"

namespace rrs {

std::string formatAvailability(const Availability& a);
std::string formatFareQuote(const FareQuote& q);
// "Confirmed", "RAC", "Waitlisted - position 2", "Cancelled" (IT-CAN-06 wording).
std::string statusText(const Reservation& r);
// Full ticket: PNR, train, journey, class, status, travellers with seats, fare.
std::string formatTicket(const Reservation& r, const std::string& trainName);

}  // namespace rrs
