// test_waitlist_plan.cpp - strict-order promotion planning (FR-019, BR-07,
// issue #53): UT-CAN-04 and the promotion rules, without any database.

#include <gtest/gtest.h>

#include "cancel/waitlist_manager.h"

using namespace rrs;

namespace {

ClassConfig config(int seats, int rac, int wl) {
    ClassConfig c;
    c.trainNumber = "20003";
    c.classCode = ClassCode::SL;
    c.coaches = 1;
    c.seatsPerCoach = seats;
    c.racQuota = rac;
    c.waitlistCapacity = wl;
    return c;
}

Reservation ticket(const std::string& pnr, std::int64_t seq, TicketStatus status, int travellers,
                   int position = 0, std::vector<std::string> seats = {}) {
    Reservation r;
    r.pnr = pnr;
    r.queueSeq = seq;
    r.status = status;
    r.waitlistPosition = position;
    for (int i = 0; i < travellers; ++i) {
        Traveller t;
        t.name = "T";
        if (i < static_cast<int>(seats.size())) t.seatNumber = seats[static_cast<std::size_t>(i)];
        r.travellers.push_back(t);
    }
    return r;
}

const Reservation* find(const PromotionPlan& plan, const std::string& pnr) {
    for (const Reservation& r : plan.changed) {
        if (r.pnr == pnr) return &r;
    }
    return nullptr;
}

}  // namespace

// UT-CAN-04: WL1 holds 2 travellers, WL2 holds 1, one seat is freed ->
// nobody is promoted: WL1 does not fit and WL2 may not skip the head.
TEST(WaitlistPlan, UT_CAN_04_HeadThatDoesNotFitBlocksTheQueue) {
    std::vector<Reservation> active = {
        ticket("1000000001", 1, TicketStatus::Confirmed, 1, 0, {"S1-02"}),
        ticket("1000000003", 3, TicketStatus::Waitlisted, 2, 1),
        ticket("1000000004", 4, TicketStatus::Waitlisted, 1, 2),
    };
    PromotionPlan plan = planPromotion(config(2, 0, 2), active);
    EXPECT_TRUE(plan.promotedPnrs.empty());
    EXPECT_TRUE(plan.changed.empty());
}

// IT-CAN-03 logic: the freed seat goes to WL1; WL2 becomes WL1.
TEST(WaitlistPlan, HeadOfWaitlistGetsTheFreedSeat) {
    std::vector<Reservation> active = {
        ticket("1000000002", 2, TicketStatus::Confirmed, 1, 0, {"S1-02"}),
        ticket("1000000003", 3, TicketStatus::Waitlisted, 1, 1),
        ticket("1000000004", 4, TicketStatus::Waitlisted, 1, 2),
    };
    PromotionPlan plan = planPromotion(config(2, 0, 2), active);
    ASSERT_EQ(plan.promotedPnrs, std::vector<std::string>{"1000000003"});
    const Reservation* promoted = find(plan, "1000000003");
    ASSERT_NE(promoted, nullptr);
    EXPECT_EQ(promoted->status, TicketStatus::Confirmed);
    EXPECT_EQ(promoted->waitlistPosition, 0);
    EXPECT_EQ(promoted->travellers[0].seatNumber, "S1-01");
    const Reservation* moved = find(plan, "1000000004");
    ASSERT_NE(moved, nullptr);
    EXPECT_EQ(moved->status, TicketStatus::Waitlisted);
    EXPECT_EQ(moved->waitlistPosition, 1);
}

// IT-CAN-05 logic: RAC is confirmed first, then WL1 moves into RAC.
TEST(WaitlistPlan, RacConfirmedThenWaitlistMovesIntoRac) {
    std::vector<Reservation> active = {
        ticket("1000000002", 2, TicketStatus::Confirmed, 1, 0, {"B1-02"}),
        ticket("1000000003", 3, TicketStatus::Rac, 1),
        ticket("1000000004", 4, TicketStatus::Waitlisted, 1, 1),
    };
    ClassConfig c = config(2, 1, 2);
    c.classCode = ClassCode::AC3;
    PromotionPlan plan = planPromotion(c, active);
    EXPECT_EQ(plan.promotedPnrs, (std::vector<std::string>{"1000000003", "1000000004"}));
    EXPECT_EQ(find(plan, "1000000003")->status, TicketStatus::Confirmed);
    EXPECT_EQ(find(plan, "1000000003")->travellers[0].seatNumber, "B1-01");
    EXPECT_EQ(find(plan, "1000000004")->status, TicketStatus::Rac);
    EXPECT_EQ(find(plan, "1000000004")->waitlistPosition, 0);
}

TEST(WaitlistPlan, SeveralTicketsArePromotedInQueueOrder) {
    std::vector<Reservation> active = {
        ticket("1000000005", 5, TicketStatus::Waitlisted, 1, 2),
        ticket("1000000004", 4, TicketStatus::Waitlisted, 2, 1),
        ticket("1000000006", 6, TicketStatus::Waitlisted, 1, 3),
    };
    PromotionPlan plan = planPromotion(config(3, 0, 5), active);   // all 3 seats free
    EXPECT_EQ(plan.promotedPnrs, (std::vector<std::string>{"1000000004", "1000000005"}));
    EXPECT_EQ(find(plan, "1000000004")->travellers[1].seatNumber, "S1-02");
    EXPECT_EQ(find(plan, "1000000005")->travellers[0].seatNumber, "S1-03");
    EXPECT_EQ(find(plan, "1000000006")->waitlistPosition, 1);
}

TEST(WaitlistPlan, GapsInWaitlistPositionsAreClosed) {
    // WL2 was cancelled: WL1 and WL3 remain, no seat free.
    std::vector<Reservation> active = {
        ticket("1000000001", 1, TicketStatus::Confirmed, 2, 0, {"S1-01", "S1-02"}),
        ticket("1000000003", 3, TicketStatus::Waitlisted, 1, 1),
        ticket("1000000005", 5, TicketStatus::Waitlisted, 1, 3),
    };
    PromotionPlan plan = planPromotion(config(2, 0, 5), active);
    EXPECT_TRUE(plan.promotedPnrs.empty());
    ASSERT_EQ(plan.changed.size(), 1u);
    EXPECT_EQ(plan.changed[0].pnr, "1000000005");
    EXPECT_EQ(plan.changed[0].waitlistPosition, 2);
}

TEST(WaitlistPlan, BlockedRacHeadAlsoBlocksTheWaitlist) {
    std::vector<Reservation> active = {
        ticket("1000000001", 1, TicketStatus::Confirmed, 1, 0, {"S1-02"}),
        ticket("1000000002", 2, TicketStatus::Rac, 2),
        ticket("1000000003", 3, TicketStatus::Waitlisted, 1, 1),
    };
    PromotionPlan plan = planPromotion(config(2, 2, 2), active);   // 1 seat free, RAC full
    EXPECT_TRUE(plan.promotedPnrs.empty());
}

TEST(WaitlistPlan, NothingToDoWhenNobodyWaits) {
    std::vector<Reservation> active = {ticket("1000000001", 1, TicketStatus::Confirmed, 1, 0, {"S1-01"})};
    PromotionPlan plan = planPromotion(config(2, 0, 2), active);
    EXPECT_TRUE(plan.changed.empty());
    EXPECT_TRUE(plan.promotedPnrs.empty());
}
