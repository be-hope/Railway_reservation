#include "cancel/refund_calculator.h"

namespace rrs {

Money RefundCalculator::calculate(Money fare, TicketStatus status,
                                  std::int64_t secondsToDeparture) {
    if (status == TicketStatus::Waitlisted) {
        return fare;
    }

    int percentage = 50;

    if (secondsToDeparture >= 48LL * 60 * 60) {
        percentage = 90;
    } else if (secondsToDeparture >= 12LL * 60 * 60) {
        percentage = 75;
    }

    // Integer arithmetic in paise, rounded half-up.
    const std::int64_t paise = fare.paise();
    const std::int64_t refund =
        (paise / 100) * percentage +
        ((paise % 100) * percentage + 50) / 100;

    return Money::fromPaise(refund);
}

}  // namespace rrs