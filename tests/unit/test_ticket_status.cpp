#include <gtest/gtest.h>

#include "common/domain.h"

using namespace rrs;

TEST(TicketStatusTest, ValidForwardTransitionsAreAccepted) {
    EXPECT_TRUE(canTransitionTicketStatus(TicketStatus::Confirmed,
                                          TicketStatus::Cancelled));

    EXPECT_TRUE(canTransitionTicketStatus(TicketStatus::Rac,
                                          TicketStatus::Confirmed));
    EXPECT_TRUE(canTransitionTicketStatus(TicketStatus::Rac,
                                          TicketStatus::Cancelled));

    EXPECT_TRUE(canTransitionTicketStatus(TicketStatus::Waitlisted,
                                          TicketStatus::Rac));
    EXPECT_TRUE(canTransitionTicketStatus(TicketStatus::Waitlisted,
                                          TicketStatus::Confirmed));
    EXPECT_TRUE(canTransitionTicketStatus(TicketStatus::Waitlisted,
                                          TicketStatus::Cancelled));
}

TEST(TicketStatusTest, InvalidBackwardTransitionsAreRejected) {
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Confirmed,
                                           TicketStatus::Rac));
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Confirmed,
                                           TicketStatus::Waitlisted));
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Rac,
                                           TicketStatus::Waitlisted));
}

TEST(TicketStatusTest, CancelledIsTerminal) {
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Cancelled,
                                           TicketStatus::Confirmed));
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Cancelled,
                                           TicketStatus::Rac));
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Cancelled,
                                           TicketStatus::Waitlisted));
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Cancelled,
                                           TicketStatus::Cancelled));
}

TEST(TicketStatusTest, RepeatedStatusesAreNotTransitions) {
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Confirmed,
                                           TicketStatus::Confirmed));
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Rac,
                                           TicketStatus::Rac));
    EXPECT_FALSE(canTransitionTicketStatus(TicketStatus::Waitlisted,
                                           TicketStatus::Waitlisted));
}
