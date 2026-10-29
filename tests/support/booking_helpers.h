// booking_helpers.h
// Builders for booking-service tests.

#pragma once

#include <string>
#include <vector>

#include "booking/booking_service.h"

namespace rrs::test {

// A valid, fare-accepted request; the fare is filled in by quoting `service`.
inline BookingRequest bookingRequest(IBookingService& service, const std::string& requestId,
                                     const std::string& train, const Date& date, const std::string& from,
                                     const std::string& to, ClassCode cls, std::vector<Traveller> travellers) {
    BookingRequest r;
    r.requestId = requestId;
    r.trainNumber = train;
    r.journeyDate = date;
    r.sourceCode = from;
    r.destinationCode = to;
    r.classCode = cls;
    r.travellers = std::move(travellers);
    r.fareAccepted = true;
    auto quote = service.quoteFare(train, from, to, cls, static_cast<int>(r.travellers.size()));
    if (quote.ok()) r.acceptedFare = quote.value().total;
    return r;
}

}  // namespace rrs::test
