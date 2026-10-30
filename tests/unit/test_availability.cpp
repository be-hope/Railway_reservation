// test_availability.cpp - UT-BKG-05 availability computation (FR-009, issue
// #26) and the status decision table (FR-012, FR-018, BR-01).

#include <gtest/gtest.h>

#include "booking/availability.h"

using namespace rrs;

namespace {

ClassConfig config(int coaches, int seats, int rac, int wl) {
    ClassConfig c;
    c.trainNumber = "20003";
    c.classCode = ClassCode::SL;
    c.coaches = coaches;
    c.seatsPerCoach = seats;
    c.racQuota = rac;
    c.waitlistCapacity = wl;
    return c;
}

const Date kDate = *Date::fromYmd(2026, 11, 9);

}  // namespace

// UT-BKG-05 case 1: capacity 2, one confirmed -> 1 seat free.
TEST(Availability, UT_BKG_05_OneConfirmedLeavesOneFree) {
    TripOccupancy occ;
    occ.confirmedTravellers = 1;
    occ.occupiedSeats = {"S1-01"};
    Availability a = computeAvailability(config(1, 2, 0, 2), kDate, occ);
    EXPECT_EQ(a.capacity, 2);
    EXPECT_EQ(a.confirmedFree, 1);
    EXPECT_EQ(a.racFree, 0);
    EXPECT_EQ(a.waitlistCount, 0);
    EXPECT_EQ(a.waitlistCapacity, 2);
}

// UT-BKG-05 case 2: a cancelled ticket does not count (repositories only
// report active tickets), so both seats are free again.
TEST(Availability, UT_BKG_05_CancelledSeatsAreReleased) {
    TripOccupancy occ;   // 1 confirmed + 1 cancelled -> the cancelled one released its seat
    occ.confirmedTravellers = 0;
    Availability a = computeAvailability(config(1, 2, 0, 2), kDate, occ);
    EXPECT_EQ(a.confirmedFree, 2);
}

TEST(Availability, ReportsRacAndWaitlist) {
    TripOccupancy occ;
    occ.confirmedTravellers = 2;
    occ.racTravellers = 1;
    occ.waitlistTickets = 1;
    Availability a = computeAvailability(config(1, 2, 1, 2), kDate, occ);
    EXPECT_EQ(a.confirmedFree, 0);
    EXPECT_EQ(a.racFree, 0);
    EXPECT_EQ(a.waitlistCount, 1);
}

TEST(Availability, NeverNegativeAfterACapacityChange) {
    TripOccupancy occ;
    occ.confirmedTravellers = 5;
    EXPECT_EQ(computeAvailability(config(1, 2, 0, 2), kDate, occ).confirmedFree, 0);
}

// ---------------------------------------------------------------- decision table

TEST(StatusDecision, ConfirmedWhenSeatsFreeAndNobodyQueued) {
    Availability a = computeAvailability(config(1, 2, 0, 2), kDate, {});
    auto d = decideStatus(a, 0, 2);
    ASSERT_TRUE(d.ok());
    EXPECT_EQ(d.value().status, TicketStatus::Confirmed);
}

// UT-CAN-03: 2 seats, RAC 0, WL 2 -> #1,#2 confirmed; #3 WL1; #4 WL2; #5 E3001.
TEST(StatusDecision, UT_CAN_03_SequenceUntilCapacityIsExhausted) {
    const ClassConfig c = config(1, 2, 0, 2);
    TripOccupancy occ;
    std::vector<TicketStatus> got;
    std::vector<int> positions;
    for (int i = 0; i < 4; ++i) {
        auto d = decideStatus(computeAvailability(c, kDate, occ), occ.racTravellers, 1);
        ASSERT_TRUE(d.ok());
        got.push_back(d.value().status);
        positions.push_back(d.value().waitlistPosition);
        if (d.value().status == TicketStatus::Confirmed) ++occ.confirmedTravellers;
        else ++occ.waitlistTickets;
    }
    EXPECT_EQ(got, (std::vector<TicketStatus>{TicketStatus::Confirmed, TicketStatus::Confirmed,
                                              TicketStatus::Waitlisted, TicketStatus::Waitlisted}));
    EXPECT_EQ(positions, (std::vector<int>{0, 0, 1, 2}));
    EXPECT_EQ(decideStatus(computeAvailability(c, kDate, occ), 0, 1).code(), ErrorCode::NoCapacity);
}

TEST(StatusDecision, RacBeforeWaitlist) {
    TripOccupancy occ;
    occ.confirmedTravellers = 2;
    auto d = decideStatus(computeAvailability(config(1, 2, 1, 2), kDate, occ), 0, 1);
    ASSERT_TRUE(d.ok());
    EXPECT_EQ(d.value().status, TicketStatus::Rac);
}

TEST(StatusDecision, PartyLargerThanFreeSeatsIsWaitlistedWhole) {
    TripOccupancy occ;
    occ.confirmedTravellers = 1;
    auto d = decideStatus(computeAvailability(config(1, 2, 0, 2), kDate, occ), 0, 2);
    ASSERT_TRUE(d.ok());
    EXPECT_EQ(d.value().status, TicketStatus::Waitlisted);
    EXPECT_EQ(d.value().waitlistPosition, 1);
}

// BR-07: a free seat held back for a waiting party is not given to a newcomer.
TEST(StatusDecision, NewBookingDoesNotOvertakeTheWaitlist) {
    TripOccupancy occ;
    occ.confirmedTravellers = 1;   // one seat free...
    occ.waitlistTickets = 1;       // ...but WL1 (2 travellers) is waiting for it
    auto d = decideStatus(computeAvailability(config(1, 2, 0, 3), kDate, occ), 0, 1);
    ASSERT_TRUE(d.ok());
    EXPECT_EQ(d.value().status, TicketStatus::Waitlisted);
    EXPECT_EQ(d.value().waitlistPosition, 2);
}

TEST(StatusDecision, NewBookingDoesNotOvertakeRac) {
    TripOccupancy occ;
    occ.confirmedTravellers = 1;
    occ.racTravellers = 2;
    auto d = decideStatus(computeAvailability(config(1, 2, 3, 2), kDate, occ), 2, 1);
    ASSERT_TRUE(d.ok());
    EXPECT_EQ(d.value().status, TicketStatus::Rac);
}

TEST(StatusDecision, RejectsZeroTravellers) {
    EXPECT_EQ(decideStatus(computeAvailability(config(1, 2, 0, 2), kDate, {}), 0, 0).code(),
              ErrorCode::Validation);
}
