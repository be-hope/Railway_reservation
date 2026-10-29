// test_schedule_coach_service.cpp - schedule, coach and seat management
// (issue #56, FR-022, IT-ADM-04): capacity changes affect availability.
// Runs on the in-memory fakes and on SQLite.

#include <gtest/gtest.h>

#include "admin/schedule_coach_service.h"
#include "booking/booking_service.h"
#include "cancel/seat_release_service.h"
#include "seed_data.h"
#include "support/booking_helpers.h"
#include "support/test_support.h"
#include "support/trip_invariants.h"

using namespace rrs;
using test::traveller;

namespace {

class ScheduleCoachTest : public ::testing::TestWithParam<std::string> {
protected:
    ScheduleCoachTest() : clock_(test::testToday()) {}

    void SetUp() override {
        backend_ = test::makeBackend(GetParam());
        data_ = std::make_unique<DataAccess>(backend_->access());
        ASSERT_TRUE(seed::loadSeedData(*data_, test::testToday(), 10).ok());

        // Test train 20099 SBC -> MYS, SL 1 coach x 2 seats, RAC 0, WL 2.
        Train t;
        t.number = "20099";
        t.name = "Test Intercity";
        RouteStop a, b;
        a.seq = 1; a.stationCode = "SBC"; a.departureMinutes = 8 * 60; a.distanceKm = 0;
        b.seq = 2; b.stationCode = "MYS"; b.arrivalMinutes = 11 * 60; b.distanceKm = 140;
        t.stops = {a, b};
        ASSERT_TRUE(data_->trains.saveTrain(t).ok());
        ClassConfig sl;
        sl.trainNumber = "20099";
        sl.classCode = ClassCode::SL;
        sl.coaches = 1;
        sl.seatsPerCoach = 2;
        sl.racQuota = 0;
        sl.waitlistCapacity = 2;
        ASSERT_TRUE(data_->config.saveClassConfig(sl).ok());
        ASSERT_TRUE(data_->trains.addSchedule("20099", date()).ok());

        booking_ = std::make_unique<BookingService>(*data_, clock_, audit_, pnr_);
        release_ = std::make_unique<SeatReleaseService>(*data_, clock_, audit_);
        admin_ = std::make_unique<ScheduleCoachService>(*data_, clock_, audit_);
        a1_ = test::sessionFor(seed::kA1, Role::Admin, clock_);
    }

    Date date() const { return test::testToday().plusDays(5); }

    ClassConfig config(int coaches, int seats, int rac = 0, int wl = 2) const {
        ClassConfig c;
        c.trainNumber = "20099";
        c.classCode = ClassCode::SL;
        c.coaches = coaches;
        c.seatsPerCoach = seats;
        c.racQuota = rac;
        c.waitlistCapacity = wl;
        return c;
    }

    Reservation book(int travellers = 1) {
        std::vector<Traveller> list(static_cast<std::size_t>(travellers), traveller("Asha Rao"));
        auto r = booking_->createBooking(
            test::sessionFor(seed::kP1, Role::Passenger, clock_),
            test::bookingRequest(*booking_, "req-" + std::to_string(++n_), "20099", date(), "SBC", "MYS",
                                 ClassCode::SL, list));
        EXPECT_TRUE(r.ok()) << formatError(r.error());
        return r.ok() ? r.value() : Reservation{};
    }

    Availability availability() { return booking_->checkAvailability("20099", date(), ClassCode::SL).value(); }

    FixedClock clock_;
    test::RecordingAuditLogger audit_;
    PnrGenerator pnr_;
    std::unique_ptr<test::Backend> backend_;
    std::unique_ptr<DataAccess> data_;
    std::unique_ptr<BookingService> booking_;
    std::unique_ptr<SeatReleaseService> release_;
    std::unique_ptr<ScheduleCoachService> admin_;
    Session a1_;
    int n_ = 0;
};

}  // namespace

// IT-ADM-04 steps 1-2: set SL to 2 coaches x 3 seats -> capacity 6 is shown.
TEST_P(ScheduleCoachTest, IT_ADM_04_IncreasingCapacityIsVisibleInAvailability) {
    EXPECT_EQ(availability().capacity, 2);
    auto r = admin_->configureClass(a1_, config(2, 3));
    ASSERT_TRUE(r.ok()) << formatError(r.error());
    Availability a = availability();
    EXPECT_EQ(a.capacity, 6);
    EXPECT_EQ(a.confirmedFree, 6);
    EXPECT_EQ(audit_.count("TRAIN_CHANGED"), 1);
    EXPECT_NE(audit_.entries.back().detail.find("old=SL 1x2"), std::string::npos);
    EXPECT_NE(audit_.entries.back().detail.find("new=SL 2x3"), std::string::npos);
}

// IT-ADM-04 step 3: with 4 seats booked, 1 x 3 is rejected with E5002.
TEST_P(ScheduleCoachTest, IT_ADM_04_ReducingBelowBookedSeatsIsRejected) {
    ASSERT_TRUE(admin_->configureClass(a1_, config(2, 3)).ok());
    for (int i = 0; i < 4; ++i) book();
    auto r = admin_->configureClass(a1_, config(1, 3));
    EXPECT_EQ(r.code(), ErrorCode::CapacityBelowBooked);
    EXPECT_EQ(availability().capacity, 6);   // unchanged
    EXPECT_EQ(availability().confirmedFree, 2);
}

TEST_P(ScheduleCoachTest, RemovingACoachWithAnOccupiedSeatIsRejected) {
    ASSERT_TRUE(admin_->configureClass(a1_, config(2, 3)).ok());
    std::vector<Reservation> t;
    for (int i = 0; i < 4; ++i) t.push_back(book());   // S1-01..S1-03, S2-01
    ASSERT_EQ(t[3].travellers[0].seatNumber, "S2-01");
    ASSERT_TRUE(release_->cancelAndRelease(t[0].pnr, std::nullopt, seed::kP1).ok());
    // 3 seats booked fits 1 x 3 by count, but S2-01 would no longer exist.
    auto r = admin_->configureClass(a1_, config(1, 3));
    EXPECT_EQ(r.code(), ErrorCode::CapacityBelowBooked);
    EXPECT_NE(r.error().detail.find("S2-01"), std::string::npos);
}

TEST_P(ScheduleCoachTest, ReducingUnusedCapacityIsAllowed) {
    ASSERT_TRUE(admin_->configureClass(a1_, config(2, 3)).ok());
    book();
    ASSERT_TRUE(admin_->configureClass(a1_, config(1, 2)).ok());
    EXPECT_EQ(availability().confirmedFree, 1);
}

// Capacity changes affect availability: added seats go to the waitlist head.
TEST_P(ScheduleCoachTest, AddedSeatsPromoteTheWaitlistInOrder) {
    book();
    book();
    Reservation wl1 = book();
    Reservation wl2 = book();
    ASSERT_EQ(wl1.waitlistPosition, 1);

    auto r = admin_->configureClass(a1_, config(1, 3));
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value(), std::vector<std::string>{wl1.pnr});
    auto promoted = data_->reservations.findByPnr(wl1.pnr).value();
    EXPECT_EQ(promoted->status, TicketStatus::Confirmed);
    EXPECT_EQ(promoted->travellers[0].seatNumber, "S1-03");
    EXPECT_EQ(data_->reservations.findByPnr(wl2.pnr).value()->waitlistPosition, 1);
    EXPECT_EQ(test::checkTripInvariants(data_->reservations, config(1, 3), date()), "");
}

TEST_P(ScheduleCoachTest, AddedRacSlotsMoveTheWaitlistIntoRac) {
    book();
    book();
    Reservation wl1 = book();
    auto r = admin_->configureClass(a1_, config(1, 2, 1, 2));
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(data_->reservations.findByPnr(wl1.pnr).value()->status, TicketStatus::Rac);
    Availability a = availability();
    EXPECT_EQ(a.racFree, 0);
    EXPECT_EQ(a.waitlistCount, 0);
}

TEST_P(ScheduleCoachTest, QuotasCannotDropBelowWhatIsHeld) {
    ASSERT_TRUE(admin_->configureClass(a1_, config(1, 2, 1, 2)).ok());
    for (int i = 0; i < 5; ++i) book();   // 2 confirmed, 1 RAC, 2 WL
    EXPECT_EQ(admin_->configureClass(a1_, config(1, 2, 0, 2)).code(), ErrorCode::CapacityBelowBooked);
    EXPECT_EQ(admin_->configureClass(a1_, config(1, 2, 1, 1)).code(), ErrorCode::CapacityBelowBooked);
    EXPECT_TRUE(admin_->configureClass(a1_, config(1, 2, 1, 5)).ok());
    EXPECT_EQ(availability().waitlistCapacity, 5);
}

TEST_P(ScheduleCoachTest, FareMultiplierChangeAffectsQuotes) {
    ClassConfig c = config(1, 2);
    c.fareMultiplierPercent = 150;
    ASSERT_TRUE(admin_->configureClass(a1_, c).ok());
    EXPECT_EQ(booking_->quoteFare("20099", "SBC", "MYS", ClassCode::SL, 1).value().total.toString(), "210.00");
}

TEST_P(ScheduleCoachTest, NewClassCanBeAdded) {
    ClassConfig ac = config(1, 4, 1, 3);
    ac.classCode = ClassCode::AC3;
    ac.fareMultiplierPercent = 250;
    ASSERT_TRUE(admin_->configureClass(a1_, ac).ok());
    auto list = admin_->listClassConfigs(a1_, "20099");
    ASSERT_TRUE(list.ok());
    EXPECT_EQ(list.value().size(), 2u);
    EXPECT_EQ(booking_->checkAvailability("20099", date(), ClassCode::AC3).value().capacity, 4);
}

TEST_P(ScheduleCoachTest, InvalidConfigurationsAreRejected) {
    ClassConfig c = config(0, 2);
    EXPECT_EQ(admin_->configureClass(a1_, c).code(), ErrorCode::Validation);
    c = config(1, 100);
    EXPECT_EQ(admin_->configureClass(a1_, c).code(), ErrorCode::Validation);
    c = config(1, 2, 3);   // RAC above capacity
    EXPECT_EQ(admin_->configureClass(a1_, c).code(), ErrorCode::Validation);
    c = config(1, 2, 0, -1);
    EXPECT_EQ(admin_->configureClass(a1_, c).code(), ErrorCode::Validation);
    c = config(1, 2);
    c.fareMultiplierPercent = 0;
    EXPECT_EQ(admin_->configureClass(a1_, c).code(), ErrorCode::Validation);
    c = config(1, 2);
    c.trainNumber = "88888";
    EXPECT_EQ(admin_->configureClass(a1_, c).code(), ErrorCode::TrainNotFound);
    EXPECT_EQ(availability().capacity, 2);
}

// BR-04 / SEC-002: only administrators manage coaches and schedules.
TEST_P(ScheduleCoachTest, OnlyAdministratorsMayChangeConfiguration) {
    for (Role role : {Role::Passenger, Role::Staff, Role::Ops}) {
        Session s = test::sessionFor(seed::kP1, role, clock_);
        EXPECT_EQ(admin_->configureClass(s, config(2, 3)).code(), ErrorCode::Forbidden);
        EXPECT_EQ(admin_->addSchedules(s, "20099", date(), date()).code(), ErrorCode::Forbidden);
        EXPECT_EQ(admin_->removeSchedule(s, "20099", date()).code(), ErrorCode::Forbidden);
    }
    EXPECT_EQ(audit_.count("UNAUTHORIZED_ATTEMPT"), 9);
    Session expired = a1_;
    clock_.advanceSeconds(20 * 60);
    EXPECT_EQ(admin_->configureClass(expired, config(2, 3)).code(), ErrorCode::SessionExpired);
    EXPECT_EQ(availability().capacity, 2);
}

TEST_P(ScheduleCoachTest, SchedulesCanBeAddedForADateRange) {
    const Date from = test::testToday().plusDays(20);
    ASSERT_TRUE(admin_->addSchedules(a1_, "20099", from, from.plusDays(6)).ok());
    auto dates = admin_->listSchedules(a1_, "20099");
    ASSERT_TRUE(dates.ok());
    EXPECT_EQ(dates.value().size(), 8u);   // date() + 7 new days
    auto found = data_->trains.findTrains("SBC", "MYS", from.plusDays(3)).value();   // seed runs 10 days only
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].number, "20099");

    EXPECT_EQ(admin_->addSchedules(a1_, "20099", from, from.plusDays(-1)).code(), ErrorCode::Validation);
    EXPECT_EQ(admin_->addSchedules(a1_, "20099", test::testToday().plusDays(-1), from).code(),
              ErrorCode::Validation);
    EXPECT_EQ(admin_->addSchedules(a1_, "20099", from, from.plusDays(400)).code(), ErrorCode::Validation);
    EXPECT_EQ(admin_->addSchedules(a1_, "77777", from, from).code(), ErrorCode::TrainNotFound);
}

TEST_P(ScheduleCoachTest, ScheduleWithActiveReservationsCannotBeRemoved) {
    Reservation r = book();
    EXPECT_EQ(admin_->removeSchedule(a1_, "20099", date()).code(), ErrorCode::TrainInUse);
    EXPECT_TRUE(data_->trains.hasSchedule("20099", date()).value());

    ASSERT_TRUE(release_->cancelAndRelease(r.pnr, std::nullopt, seed::kP1).ok());
    ASSERT_TRUE(admin_->removeSchedule(a1_, "20099", date()).ok());
    EXPECT_FALSE(data_->trains.hasSchedule("20099", date()).value());
    EXPECT_TRUE(data_->trains.findTrains("SBC", "MYS", date()).value().size() == 1u);   // only 20003
    EXPECT_EQ(booking_->checkAvailability("20099", date(), ClassCode::SL).code(), ErrorCode::TrainNotFound);
    EXPECT_EQ(admin_->removeSchedule(a1_, "20099", date()).code(), ErrorCode::TrainNotFound);
}

INSTANTIATE_TEST_SUITE_P(Backends, ScheduleCoachTest, ::testing::ValuesIn(test::backendKinds()),
                         [](const auto& info) { return info.param; });
