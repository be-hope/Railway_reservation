#include <gtest/gtest.h>

#include "cancel/refund_calculator.h"

using namespace rrs;

TEST(RefundCalculatorTest, RefundSlabBoundaries) {
    const Money fare = Money::fromRupees(1000);
    const std::int64_t hour = 60 * 60;

    EXPECT_EQ(RefundCalculator::calculate(fare, TicketStatus::Confirmed,
                                            72 * hour).toString(), "900.00");

    EXPECT_EQ(RefundCalculator::calculate(fare, TicketStatus::Confirmed,
                                            48 * hour).toString(), "900.00");

    EXPECT_EQ(RefundCalculator::calculate(fare, TicketStatus::Confirmed,
                                            47 * hour + 59 * 60).toString(), "750.00");

    EXPECT_EQ(RefundCalculator::calculate(fare, TicketStatus::Confirmed,
                                            12 * hour).toString(), "750.00");

    EXPECT_EQ(RefundCalculator::calculate(fare, TicketStatus::Confirmed,
                                            11 * hour + 59 * 60).toString(), "500.00");
}

TEST(RefundCalculatorTest, WaitlistedTicketGetsFullRefund) {
    const Money fare = Money::fromRupees(1000);

    EXPECT_EQ(RefundCalculator::calculate(fare, TicketStatus::Waitlisted,
                                            5 * 60 * 60).toString(), "1,000.00");
}

TEST(RefundCalculatorTest, RacTicketUsesNormalRefundSlabs) {
    const Money fare = Money::fromRupees(1000);

    EXPECT_EQ(RefundCalculator::calculate(fare, TicketStatus::Rac,
                                            60 * 60).toString(), "500.00");
}

TEST(RefundCalculatorTest, HandlesPaiseWithoutFloatingPoint) {
    const Money fare = Money::fromPaise(101);

    EXPECT_EQ(RefundCalculator::calculate(fare, TicketStatus::Confirmed,
                                            48LL * 60 * 60).paise(), 91);
}