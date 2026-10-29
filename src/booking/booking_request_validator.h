// booking_request_validator.h
// Field checks for a booking request (FR-010, BR-05, SEC-003). Every field is
// length-, format- and range-checked before any data access; failures return
// ERR-VALIDATION (E1001) with a field-level message.
//
// NOTE: issues #18 (InputValidator) and #29 (validate passenger and journey
// details) own the shared validators. This file only covers what the booking
// service must reject on its own; delegate to InputValidator when it exists.

#pragma once

#include <string>
#include <vector>

#include "common/date.h"
#include "common/domain.h"
#include "common/money.h"
#include "common/result.h"

namespace rrs {

constexpr int kMinTravellers = 1;
constexpr int kMaxTravellers = 6;          // BR-05
constexpr int kBookingWindowDays = 120;    // SRS Table 2.7-B

struct BookingRequest {
    std::string requestId;        // idempotency token, 1-36 chars, reused on retry
    std::string onBehalfOf;       // passenger account id; staff bookings only (FR-023)
    std::string trainNumber;
    Date journeyDate;
    std::string sourceCode;
    std::string destinationCode;
    ClassCode classCode = ClassCode::SL;
    std::vector<Traveller> travellers;
    bool fareAccepted = false;    // the passenger answered Y on the fare screen
    Money acceptedFare;           // the total that was shown and accepted
};

Status validateTraveller(const Traveller& traveller, int position);
Status validateBookingRequest(const BookingRequest& request, const Date& today);

}  // namespace rrs
