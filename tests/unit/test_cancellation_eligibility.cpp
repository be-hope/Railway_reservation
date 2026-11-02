#include <gtest/gtest.h>

#include "cancel/cancellation_service.h"

using namespace rrs;

namespace {

constexpr EpochSeconds kNow = 100000;
constexpr EpochSeconds kFutureDeparture = kNow + 3600;

TEST(CancellationEligibilityTest, UT_CAN_02_FutureActiveTicketsCanBeCancelled) {
    EXPECT_TRUE(CancellationService::validateEligibility(
        TicketStatus::Confirmed, kFutureDeparture, kNow).ok());

    EXPECT_TRUE(CancellationService::validateEligibility(
        TicketStatus::Rac, kFutureDeparture, kNow).ok());

    EXPECT_TRUE(CancellationService::validateEligibility(
        TicketStatus::Waitlisted, kFutureDeparture, kNow).ok());
}

TEST(CancellationEligibilityTest, UT_CAN_02_AlreadyCancelledTicketIsRejected) {
    auto result = CancellationService::validateEligibility(
        TicketStatus::Cancelled, kFutureDeparture, kNow);

    EXPECT_EQ(result.code(), ErrorCode::AlreadyCancelled);
}

TEST(CancellationEligibilityTest, UT_CAN_02_DepartedTicketIsRejected) {
    auto result = CancellationService::validateEligibility(
        TicketStatus::Confirmed, kNow - 1, kNow);

    EXPECT_EQ(result.code(), ErrorCode::CancelDeparted);
}

TEST(CancellationEligibilityTest, UT_CAN_02_ExactDepartureTimeIsRejected) {
    auto result = CancellationService::validateEligibility(
        TicketStatus::Confirmed, kNow, kNow);

    EXPECT_EQ(result.code(), ErrorCode::CancelDeparted);
}

}  // namespace
