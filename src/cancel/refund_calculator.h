#pragma once

#include <cstdint>

#include "common/domain.h"
#include "common/money.h"

namespace rrs {

class RefundCalculator {
public:
    // secondsToDeparture must be positive for an eligible cancellation.
    static Money calculate(Money fare, TicketStatus status,
                           std::int64_t secondsToDeparture);
};

}  // namespace rrs