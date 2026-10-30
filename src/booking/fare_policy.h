// fare_policy.h
// Fare arithmetic used when the booking is confirmed (FR-011, FR-012).
//   fare per traveller = distance (km) x rate per km x class multiplier
//   total fare         = fare per traveller x number of travellers
// Rate and multipliers are configuration (SRS Table 2.7-B); money is integer
// paise, rounded half-up to the paisa.
//
// NOTE: the passenger-facing FareCalculator and fare quotes are issues #37/#38.
// The booking service needs the same formula to re-check the accepted fare,
// so it lives here as a pure helper; once #37 lands, both should share one
// implementation.

#pragma once

#include <cstdint>

#include "common/money.h"

namespace rrs {

struct FareRules {
    std::int64_t paisePerKm = 100;   // 1.00 per km (baseline)
};

// multiplierPercent: 100 = Sleeper (1.0), 250 = AC 3-Tier (2.5).
Money farePerTraveller(int distanceKm, int multiplierPercent, const FareRules& rules = FareRules{});
Money totalFare(int distanceKm, int multiplierPercent, int travellers,
                const FareRules& rules = FareRules{});

}  // namespace rrs
