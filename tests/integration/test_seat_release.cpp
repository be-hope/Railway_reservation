// test_seat_release.cpp - releasing cancelled seats (issue #49, IT-CAN-01)
// and strict-order waitlist promotion (issue #53, IT-CAN-03/04/05, UT-CAN-04).
// Runs on the in-memory fakes and on SQLite.

#include <gtest/gtest.h>

#include <random>

#include "booking/booking_service.h"
#include "cancel/seat_release_service.h"
#include "seed_data.h"
#include "support/booking_helpers.h"
#include "support/test_support.h"
#include "support/trip_invariants.h"

using namespace rrs;
using test::traveller;

namespace {

class SeatReleaseTest : public ::testing::TestWithParam<std::string> {
protected:
    SeatReleaseTest() : clock_(test::testToday()) {}

    void SetUp() override {
        backend_ = test::makeBackend(GetParam());
        DataAccess raw = backend_->access();
        ASSERT_TRUE(seed::loadSeedData(raw, test::testToday()).ok());
        faulty_ = std::make_unique<test::FaultInjectingReservationRepository>(raw.reservations);
        data_ = std::make_unique<DataAccess>(DataAccess{raw.trains, raw.config, *faulty_, raw.users, raw.unitOfWork});
        booking_ = std::make_unique<BookingService>(*data_, clock_, audit_, pnr_);
        release_ = std::make_unique<SeatReleaseService>(*data_, clock_, audit_);
    }

    Date date() const { return test::testToday().plusDays(3); }

    // Books 20003 SBC->MYS for the given number of travellers.
    Reservation book(ClassCode cls, int travellers = 1, const std::string& account = seed::kP1) {
        std::vector<Traveller> list;
        for (int i = 0; i < travellers; ++i) list.push_back(traveller("Traveller " + std::to_string(i + 1)));
        BookingRequest r = test::bookingRequest(*booking_, "req-" + std::to_string(++requests_), "20003", date(),
                                                "SBC", "MYS", cls, list);
        auto booked = booking_->createBooking(test::sessionFor(account, Role::Passenger, clock_), r);
        EXPECT_TRUE(booked.ok()) << formatError(booked.error());
        return booked.ok() ? booked.value() : Reservation{};
    }

    Reservation reload(const std::string& pnr) { return *data_->reservations.findByPnr(pnr).value(); }

    Availability availability(ClassCode cls = ClassCode::SL) {
        return booking_->checkAvailability("20003", date(), cls).value();
    }

    std::string invariants(ClassCode cls) {
        auto config = data_->config.findClassConfig("20003", cls).value();
        return test::checkTripInvariants(data_->reservations, *config, date());
    }

    FixedClock clock_;
    test::RecordingAuditLogger audit_;
    PnrGenerator pnr_;
    std::unique_ptr<test::Backend> backend_;
    std::unique_ptr<test::FaultInjectingReservationRepository> faulty_;
    std::unique_ptr<DataAccess> data_;
    std::unique_ptr<BookingService> booking_;
    std::unique_ptr<SeatReleaseService> release_;
    int requests_ = 0;
};

}  // namespace

// IT-CAN-01 (seat part): status CANCELLED, seat released (availability +1),
// refund stored, audit event written.
TEST_P(SeatReleaseTest, IT_CAN_01_CancelledSeatBecomesAvailableAgain) {
    Reservation r = book(ClassCode::SL);
    ASSERT_EQ(r.travellers[0].seatNumber, "S1-01");
    EXPECT_EQ(availability().confirmedFree, 1);

    auto released = release_->cancelAndRelease(r.pnr, Money::fromRupees(126), seed::kP1);
    ASSERT_TRUE(released.ok()) << formatError(released.error());
    EXPECT_EQ(released.value().seatsReleased, 1);
    EXPECT_TRUE(released.value().promotedPnrs.empty());

    Reservation stored = reload(r.pnr);
    EXPECT_EQ(stored.status, TicketStatus::Cancelled);
    EXPECT_TRUE(stored.travellers[0].seatNumber.empty());
    ASSERT_TRUE(stored.refund.has_value());
    EXPECT_EQ(stored.refund->toString(), "126.00");
    EXPECT_EQ(stored.cancelledAt, clock_.now());
    EXPECT_EQ(availability().confirmedFree, 2);
    EXPECT_EQ(audit_.count("TICKET_CANCELLED"), 1);
}

TEST_P(SeatReleaseTest, ReleasedSeatIsAllocatedToTheNextBooking) {
    Reservation first = book(ClassCode::SL);
    Reservation second = book(ClassCode::SL);
    ASSERT_TRUE(release_->cancelAndRelease(first.pnr, std::nullopt, seed::kP1).ok());
    Reservation third = book(ClassCode::SL, 1, seed::kP2);
    EXPECT_EQ(second.travellers[0].seatNumber, "S1-02");
    EXPECT_EQ(third.status, TicketStatus::Confirmed);
    EXPECT_EQ(third.travellers[0].seatNumber, "S1-01");
}

TEST_P(SeatReleaseTest, AllSeatsOfAMultiTravellerTicketAreReleased) {
    Reservation r = book(ClassCode::SL, 2);
    EXPECT_EQ(availability().confirmedFree, 0);
    auto released = release_->cancelAndRelease(r.pnr, std::nullopt, seed::kP1);
    ASSERT_TRUE(released.ok());
    EXPECT_EQ(released.value().seatsReleased, 2);
    EXPECT_EQ(availability().confirmedFree, 2);
}

TEST_P(SeatReleaseTest, UnknownAlreadyCancelledAndMalformedPnr) {
    Reservation r = book(ClassCode::SL);
    EXPECT_EQ(release_->cancelAndRelease("9999999999", std::nullopt, seed::kP1).code(), ErrorCode::PnrNotFound);
    ASSERT_TRUE(release_->cancelAndRelease(r.pnr, std::nullopt, seed::kP1).ok());
    EXPECT_EQ(release_->cancelAndRelease(r.pnr, std::nullopt, seed::kP1).code(), ErrorCode::AlreadyCancelled);
    EXPECT_EQ(release_->cancelAndRelease("12AB", std::nullopt, seed::kP1).code(), ErrorCode::Validation);
    EXPECT_EQ(audit_.count("TICKET_CANCELLED"), 1);
}

// IT-CAN-03: 20003 SL (2 seats, RAC 0, WL 2): 4 tickets; cancel #1 ->
// #3 CONFIRMED with the freed seat, #4 moves from WL2 to WL1, in one transaction.
TEST_P(SeatReleaseTest, IT_CAN_03_WaitlistPromotedOnCancellation) {
    std::vector<Reservation> t;
    for (int i = 0; i < 4; ++i) t.push_back(book(ClassCode::SL));
    ASSERT_EQ(t[2].waitlistPosition, 1);
    ASSERT_EQ(t[3].waitlistPosition, 2);

    auto released = release_->cancelAndRelease(t[0].pnr, std::nullopt, seed::kP1);
    ASSERT_TRUE(released.ok());
    EXPECT_EQ(released.value().promotedPnrs, std::vector<std::string>{t[2].pnr});

    Reservation promoted = reload(t[2].pnr);
    EXPECT_EQ(promoted.status, TicketStatus::Confirmed);
    EXPECT_EQ(promoted.travellers[0].seatNumber, "S1-01");
    EXPECT_EQ(promoted.waitlistPosition, 0);
    Reservation next = reload(t[3].pnr);
    EXPECT_EQ(next.status, TicketStatus::Waitlisted);
    EXPECT_EQ(next.waitlistPosition, 1);
    EXPECT_EQ(invariants(ClassCode::SL), "");
}

// UT-CAN-04 end to end: WL1 holds 2, WL2 holds 1, one seat freed -> nobody moves.
TEST_P(SeatReleaseTest, UT_CAN_04_LargeHeadIsNotSkipped) {
    Reservation c1 = book(ClassCode::SL);
    book(ClassCode::SL);
    Reservation wl1 = book(ClassCode::SL, 2);
    Reservation wl2 = book(ClassCode::SL);
    ASSERT_EQ(wl1.waitlistPosition, 1);
    ASSERT_EQ(wl2.waitlistPosition, 2);

    auto released = release_->cancelAndRelease(c1.pnr, std::nullopt, seed::kP1);
    ASSERT_TRUE(released.ok());
    EXPECT_TRUE(released.value().promotedPnrs.empty());
    EXPECT_EQ(reload(wl1.pnr).status, TicketStatus::Waitlisted);
    EXPECT_EQ(reload(wl2.pnr).waitlistPosition, 2);
    // The free seat is held for the queue: a newcomer is waitlisted, not confirmed.
    EXPECT_EQ(availability().confirmedFree, 1);
    EXPECT_EQ(invariants(ClassCode::SL), "");
}

// IT-CAN-04: cancelling WL1 moves WL2 up to WL1; a waitlisted ticket has no seats.
TEST_P(SeatReleaseTest, IT_CAN_04_CancelWaitlistedTicketRenumbersQueue) {
    book(ClassCode::SL);
    book(ClassCode::SL);
    Reservation wl1 = book(ClassCode::SL);
    Reservation wl2 = book(ClassCode::SL);
    auto released = release_->cancelAndRelease(wl1.pnr, Money::fromRupees(140), seed::kP1);
    ASSERT_TRUE(released.ok());
    EXPECT_EQ(released.value().seatsReleased, 0);
    EXPECT_TRUE(released.value().promotedPnrs.empty());
    EXPECT_EQ(reload(wl1.pnr).status, TicketStatus::Cancelled);
    EXPECT_EQ(reload(wl1.pnr).waitlistPosition, 0);
    EXPECT_EQ(reload(wl2.pnr).waitlistPosition, 1);
    EXPECT_EQ(availability().waitlistCount, 1);
}

// IT-CAN-05: 20003 3A (2 seats, RAC 1, WL 2): #1,#2 CONFIRMED, #3 RAC, #4 WL1;
// cancel #1 -> #3 CONFIRMED and #4 RAC.
TEST_P(SeatReleaseTest, IT_CAN_05_RacPath) {
    std::vector<Reservation> t;
    for (int i = 0; i < 4; ++i) t.push_back(book(ClassCode::AC3));
    ASSERT_EQ(t[2].status, TicketStatus::Rac);
    ASSERT_EQ(t[3].status, TicketStatus::Waitlisted);

    auto released = release_->cancelAndRelease(t[0].pnr, std::nullopt, seed::kP1);
    ASSERT_TRUE(released.ok());
    EXPECT_EQ(released.value().promotedPnrs, (std::vector<std::string>{t[2].pnr, t[3].pnr}));
    EXPECT_EQ(reload(t[2].pnr).status, TicketStatus::Confirmed);
    EXPECT_EQ(reload(t[2].pnr).travellers[0].seatNumber, "B1-01");
    EXPECT_EQ(reload(t[3].pnr).status, TicketStatus::Rac);
    EXPECT_EQ(invariants(ClassCode::AC3), "");
}

TEST_P(SeatReleaseTest, CancellingAnRacTicketPromotesTheWaitlistIntoRac) {
    for (int i = 0; i < 2; ++i) book(ClassCode::AC3);
    Reservation rac = book(ClassCode::AC3);
    Reservation wl = book(ClassCode::AC3);
    auto released = release_->cancelAndRelease(rac.pnr, std::nullopt, seed::kP1);
    ASSERT_TRUE(released.ok());
    EXPECT_EQ(reload(wl.pnr).status, TicketStatus::Rac);
    EXPECT_EQ(invariants(ClassCode::AC3), "");
}

// AC-019: release and promotion are one transaction - a failure while
// promoting leaves the cancelled ticket untouched.
TEST_P(SeatReleaseTest, FailureDuringPromotionRollsBackTheWholeCancellation) {
    Reservation c1 = book(ClassCode::SL);
    book(ClassCode::SL);
    Reservation wl1 = book(ClassCode::SL);
    // Cancel with a fault on the very first update: nothing changes.
    faulty_->failNextUpdates = 1;
    EXPECT_EQ(release_->cancelAndRelease(c1.pnr, std::nullopt, seed::kP1).code(), ErrorCode::Database);
    EXPECT_EQ(reload(c1.pnr).status, TicketStatus::Confirmed);
    EXPECT_EQ(reload(c1.pnr).travellers[0].seatNumber, "S1-01");
    EXPECT_EQ(reload(wl1.pnr).status, TicketStatus::Waitlisted);
    EXPECT_EQ(audit_.count("TICKET_CANCELLED"), 0);
    EXPECT_EQ(invariants(ClassCode::SL), "");

    // Fault on the second update (promoting WL1): still nothing changes.
    faulty_->updatesBeforeFault = 1;
    EXPECT_EQ(release_->cancelAndRelease(c1.pnr, std::nullopt, seed::kP1).code(), ErrorCode::Database);
    EXPECT_EQ(reload(c1.pnr).status, TicketStatus::Confirmed);
    EXPECT_EQ(reload(wl1.pnr).status, TicketStatus::Waitlisted);
    EXPECT_EQ(invariants(ClassCode::SL), "");

    // Retry succeeds and promotes.
    ASSERT_TRUE(release_->cancelAndRelease(c1.pnr, std::nullopt, seed::kP1).ok());
    EXPECT_EQ(reload(wl1.pnr).status, TicketStatus::Confirmed);
}

// No passenger skips the queue: random bookings (1-3 travellers) and
// cancellations on 20003 3A; the trip invariants hold after every step.
TEST_P(SeatReleaseTest, RandomisedBookAndCancelNeverBreaksQueueOrder) {
    ClassConfig bigger = *data_->config.findClassConfig("20003", ClassCode::AC3).value();
    bigger.seatsPerCoach = 6;
    bigger.racQuota = 3;
    bigger.waitlistCapacity = 6;
    ASSERT_TRUE(data_->config.saveClassConfig(bigger).ok());

    std::mt19937 rng(20261102);
    std::vector<std::string> live;
    for (int step = 0; step < 200; ++step) {
        if (live.empty() || rng() % 3 != 0) {
            const int n = 1 + static_cast<int>(rng() % 3);
            std::vector<Traveller> list(static_cast<std::size_t>(n), traveller("Rand"));
            BookingRequest r = test::bookingRequest(*booking_, "rnd-" + std::to_string(step), "20003", date(),
                                                    "SBC", "MYS", ClassCode::AC3, list);
            auto booked = booking_->createBooking(test::sessionFor(seed::kP1, Role::Passenger, clock_), r);
            if (booked.ok()) live.push_back(booked.value().pnr);
            else ASSERT_EQ(booked.code(), ErrorCode::NoCapacity);
        } else {
            const std::size_t i = rng() % live.size();
            ASSERT_TRUE(release_->cancelAndRelease(live[i], std::nullopt, seed::kP1).ok());
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(i));
        }
        ASSERT_EQ(invariants(ClassCode::AC3), "") << "after step " << step;
    }
}

INSTANTIATE_TEST_SUITE_P(Backends, SeatReleaseTest, ::testing::ValuesIn(test::backendKinds()),
                         [](const auto& info) { return info.param; });
