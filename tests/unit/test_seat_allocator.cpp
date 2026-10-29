// test_seat_allocator.cpp - UT-BKG-04 seat allocation order (FR-013, issue #27).

#include <gtest/gtest.h>

#include <set>

#include "booking/seat_allocator.h"

using namespace rrs;

namespace {

ClassConfig config(ClassCode cls, int coaches, int seats) {
    ClassConfig c;
    c.trainNumber = "20003";
    c.classCode = cls;
    c.coaches = coaches;
    c.seatsPerCoach = seats;
    return c;
}

}  // namespace

// UT-BKG-04: capacity 2 -> S1-01; then S1-02; then capacity exhausted.
TEST(SeatAllocator, UT_BKG_04_AllocatesLowestFreeSeatThenExhausts) {
    const ClassConfig c = config(ClassCode::SL, 1, 2);

    auto first = SeatAllocator::allocate(c, {}, 1);
    ASSERT_TRUE(first.ok());
    EXPECT_EQ(first.value(), std::vector<std::string>{"S1-01"});

    auto second = SeatAllocator::allocate(c, {"S1-01"}, 1);
    ASSERT_TRUE(second.ok());
    EXPECT_EQ(second.value(), std::vector<std::string>{"S1-02"});

    auto third = SeatAllocator::allocate(c, {"S1-01", "S1-02"}, 1);
    ASSERT_FALSE(third.ok());
    EXPECT_EQ(third.code(), ErrorCode::NoCapacity);
}

TEST(SeatAllocator, FillsGapsLeftByReleasedSeatsFirst) {
    const ClassConfig c = config(ClassCode::SL, 1, 5);
    auto seats = SeatAllocator::allocate(c, {"S1-01", "S1-03", "S1-04"}, 2);
    ASSERT_TRUE(seats.ok());
    EXPECT_EQ(seats.value(), (std::vector<std::string>{"S1-02", "S1-05"}));
}

TEST(SeatAllocator, ContinuesIntoTheNextCoach) {
    const ClassConfig c = config(ClassCode::AC3, 2, 2);
    auto seats = SeatAllocator::allocate(c, {"B1-01"}, 3);
    ASSERT_TRUE(seats.ok());
    EXPECT_EQ(seats.value(), (std::vector<std::string>{"B1-02", "B2-01", "B2-02"}));
}

TEST(SeatAllocator, AllOrNothingWhenTooFewSeatsAreFree) {
    const ClassConfig c = config(ClassCode::SL, 1, 3);
    auto seats = SeatAllocator::allocate(c, {"S1-02"}, 3);
    EXPECT_EQ(seats.code(), ErrorCode::NoCapacity);
}

TEST(SeatAllocator, NeverReturnsAnOccupiedOrDuplicateSeat) {
    const ClassConfig c = config(ClassCode::SL, 4, 72);
    std::vector<std::string> occupied;
    std::set<std::string> seen;
    for (int i = 0; i < 4 * 72; ++i) {
        auto seat = SeatAllocator::allocate(c, occupied, 1);
        ASSERT_TRUE(seat.ok()) << "allocation " << i;
        EXPECT_TRUE(seen.insert(seat.value()[0]).second) << seat.value()[0] << " allocated twice";
        occupied.push_back(seat.value()[0]);
    }
    EXPECT_EQ(SeatAllocator::allocate(c, occupied, 1).code(), ErrorCode::NoCapacity);
}

TEST(SeatAllocator, RejectsNonPositiveCount) {
    EXPECT_EQ(SeatAllocator::allocate(config(ClassCode::SL, 1, 2), {}, 0).code(), ErrorCode::Validation);
}

TEST(SeatLabel, FormatsAndParsesLabels) {
    EXPECT_EQ(seatLabel(ClassCode::SL, 1, 7, 72), "S1-07");
    EXPECT_EQ(seatLabel(ClassCode::AC3, 2, 64, 64), "B2-64");
    EXPECT_EQ(seatLabel(ClassCode::SL, 1, 7, 120), "S1-007");

    auto p = parseSeatLabel(ClassCode::SL, "S3-15");
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->coach, 3);
    EXPECT_EQ(p->seat, 15);
    EXPECT_FALSE(parseSeatLabel(ClassCode::SL, "B1-01"));   // wrong class
    EXPECT_FALSE(parseSeatLabel(ClassCode::SL, "S1-"));
    EXPECT_FALSE(parseSeatLabel(ClassCode::SL, "S0-01"));
    EXPECT_FALSE(parseSeatLabel(ClassCode::SL, "Sx-01"));
}

TEST(SeatLabel, SeatExistsChecksTheLayout) {
    const ClassConfig c = config(ClassCode::SL, 2, 3);
    EXPECT_TRUE(seatExists(c, "S2-03"));
    EXPECT_FALSE(seatExists(c, "S2-04"));
    EXPECT_FALSE(seatExists(c, "S3-01"));
}
