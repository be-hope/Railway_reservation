// fare_policy.cpp

#include "booking/fare_policy.h"

namespace rrs {

Money farePerTraveller(int distanceKm, int multiplierPercent, const FareRules& rules) {
    if (distanceKm <= 0 || multiplierPercent <= 0) return Money();
    const std::int64_t base = static_cast<std::int64_t>(distanceKm) * rules.paisePerKm;
    return Money::fromPaise((base * multiplierPercent + 50) / 100);
}

Money totalFare(int distanceKm, int multiplierPercent, int travellers, const FareRules& rules) {
    if (travellers <= 0) return Money();
    return farePerTraveller(distanceKm, multiplierPercent, rules) * travellers;
}

}  // namespace rrs
