// test_booking_service.cpp - IT-BKG-01..06 and ST-BKG-02 (issues #26, #27,
// #28, #30, #31). Runs on the in-memory fakes and on SQLite (issue #21
// "services work with SQLite and test fakes").

#include "cancel/seat_release_service.h"
#include "ui/ticket_view.h"
#include <gtest/gtest.h>

#include <set>

#include "booking/booking_service.h"
#include "seed_data.h"
#include "support/booking_helpers.h"
#include "support/test_support.h"

using namespace rrs;
using test::bookingRequest;
using test::traveller;

namespace {

class BookingServiceTest : public ::testing::TestWithParam<std::string> {
protected:
    BookingServiceTest() : clock_(test::testToday()) {}

    void SetUp() override {
        backend_ = test::makeBackend(GetParam());
        DataAccess raw = backend_->access();
        ASSERT_TRUE(seed::loadSeedData(raw, test::testToday()).ok());
        faulty_ = std::make_unique<test::FaultInjectingReservationRepository>(raw.reservations);
        data_ = std::make_unique<DataAccess>(
            DataAccess{raw.trains, raw.config, *faulty_, raw.users, raw.unitOfWork});
        service_ = std::make_unique<BookingService>(*data_, clock_, audit_, pnr_);
        p1_ = test::sessionFor(seed::kP1, Role::Passenger, clock_);
        p2_ = test::sessionFor(seed::kP2, Role::Passenger, clock_);
    }

    BookingRequest request(const std::string& id, const std::string& train, const std::string& from,
                           const std::string& to, ClassCode cls, std::vector<Traveller> travellers,
                           int daysAhead = 7) {
        return bookingRequest(*service_, id, train, test::testToday().plusDays(daysAhead), from, to, cls,
                              std::move(travellers));
    }
    BookingRequest single(const std::string& id, ClassCode cls = ClassCode::SL) {
        return request(id, "20003", "SBC", "MYS", cls, {traveller("Asha Rao", 28, 'F')});
    }
    Availability availability(ClassCode cls = ClassCode::SL, int daysAhead = 7) {
        auto a = service_->checkAvailability("20003", test::testToday().plusDays(daysAhead), cls);
        EXPECT_TRUE(a.ok());
        return a.value();
    }
    int reservationCount() { return static_cast<int>(data_->reservations.listByJourneyDate(
                                 test::testToday(), test::testToday().plusDays(120)).value().size()); }

    FixedClock clock_;
    test::RecordingAuditLogger audit_;
    PnrGenerator pnr_;
    std::unique_ptr<test::Backend> backend_;
    std::unique_ptr<test::FaultInjectingReservationRepository> faulty_;
    std::unique_ptr<DataAccess> data_;
    std::unique_ptr<BookingService> service_;
    Session p1_, p2_;
};

}  // namespace

// IT-BKG-01: displayed availability equals the database before and after a booking.
TEST_P(BookingServiceTest, IT_BKG_01_AvailabilityReflectsReservations) {
    Availability before = availability();
    EXPECT_EQ(before.confirmedFree, 2);
    EXPECT_EQ(before.racFree, 0);
    EXPECT_EQ(before.waitlistCount, 0);
    EXPECT_EQ(before.waitlistCapacity, 2);

    ASSERT_TRUE(service_->createBooking(p1_, single("r1")).ok());
    EXPECT_EQ(availability().confirmedFree, 1);
}

TEST_P(BookingServiceTest, AvailabilityForUnknownTrainDateOrClassIsE2001) {
    EXPECT_EQ(service_->checkAvailability("99999", test::testToday().plusDays(7), ClassCode::SL).code(),
              ErrorCode::TrainNotFound);
    EXPECT_EQ(service_->checkAvailability("20003", test::testToday().plusDays(500), ClassCode::SL).code(),
              ErrorCode::TrainNotFound);
}

// IT-BKG-02: successful single booking on 20003 SL.
TEST_P(BookingServiceTest, IT_BKG_02_SuccessfulSingleBooking) {
    auto booked = service_->createBooking(p1_, single("r1"));
    ASSERT_TRUE(booked.ok()) << formatError(booked.error());
    const Reservation& r = booked.value();
    EXPECT_EQ(r.status, TicketStatus::Confirmed);
    ASSERT_EQ(r.travellers.size(), 1u);
    EXPECT_EQ(r.travellers[0].seatNumber, "S1-01");
    EXPECT_TRUE(PnrGenerator::isValidPnr(r.pnr));
    EXPECT_EQ(r.fare.toString(), "140.00");
    EXPECT_EQ(r.bookedBy, seed::kP1);

    auto stored = data_->reservations.findByPnr(r.pnr).value();
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->travellers.size(), 1u);
    EXPECT_EQ(stored->fare.toString(), "140.00");
    EXPECT_EQ(reservationCount(), 1);
    EXPECT_EQ(audit_.count("BOOKING_CREATED"), 1);
}

// IT-BKG-03: an invalid traveller field blocks the booking.
TEST_P(BookingServiceTest, IT_BKG_03_InvalidTravellerBlocksBooking) {
    BookingRequest r = single("r1");
    r.travellers[0].age = 0;
    auto booked = service_->createBooking(p1_, r);
    EXPECT_EQ(booked.code(), ErrorCode::Validation);
    EXPECT_EQ(reservationCount(), 0);
    EXPECT_EQ(availability().confirmedFree, 2);
}

// IT-BKG-04: fare not accepted -> no booking.
TEST_P(BookingServiceTest, IT_BKG_04_RejectedFareCreatesNothing) {
    BookingRequest r = single("r1");
    r.fareAccepted = false;
    EXPECT_EQ(service_->createBooking(p1_, r).code(), ErrorCode::Validation);
    EXPECT_EQ(reservationCount(), 0);
    EXPECT_EQ(availability().confirmedFree, 2);
}

TEST_P(BookingServiceTest, FareDifferentFromQuoteMustBeAcceptedAgain) {
    BookingRequest r = single("r1");
    r.acceptedFare = Money::fromRupees(1);
    auto booked = service_->createBooking(p1_, r);
    EXPECT_EQ(booked.code(), ErrorCode::Validation);
    EXPECT_NE(booked.error().detail.find("140.00"), std::string::npos);
    EXPECT_EQ(reservationCount(), 0);
}

// IT-BKG-05: two bookings for the last seat -> one CONFIRMED, one WL1.
TEST_P(BookingServiceTest, IT_BKG_05_NoSeatIsAllocatedTwice) {
    ASSERT_TRUE(service_->createBooking(p1_, single("r0")).ok());   // leaves exactly one free seat
    auto first = service_->createBooking(p1_, single("r1"));
    auto second = service_->createBooking(p2_, single("r2"));
    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(second.ok());
    EXPECT_EQ(first.value().status, TicketStatus::Confirmed);
    EXPECT_EQ(first.value().travellers[0].seatNumber, "S1-02");
    EXPECT_EQ(second.value().status, TicketStatus::Waitlisted);
    EXPECT_EQ(second.value().waitlistPosition, 1);
    EXPECT_TRUE(second.value().travellers[0].seatNumber.empty());

    auto occ = data_->reservations.occupancy("20003", test::testToday().plusDays(7), ClassCode::SL).value();
    std::set<std::string> unique(occ.occupiedSeats.begin(), occ.occupiedSeats.end());
    EXPECT_EQ(unique.size(), occ.occupiedSeats.size());
}

// IT-BKG-06 / NFR-004: a retry with the same request id returns the same PNR.
TEST_P(BookingServiceTest, IT_BKG_06_DuplicateSubmissionIsIdempotent) {
    BookingRequest r = single("same-token");
    auto first = service_->createBooking(p1_, r);
    auto retry = service_->createBooking(p1_, r);
    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(retry.ok());
    EXPECT_EQ(first.value().pnr, retry.value().pnr);
    EXPECT_EQ(reservationCount(), 1);
    EXPECT_EQ(availability().confirmedFree, 1);
}

TEST_P(BookingServiceTest, RequestTokenOfAnotherPassengerIsRejected) {
    ASSERT_TRUE(service_->createBooking(p1_, single("tok")).ok());
    EXPECT_EQ(service_->createBooking(p2_, single("tok")).code(), ErrorCode::Validation);
    EXPECT_EQ(reservationCount(), 1);
}

// ST-BKG-02: 3 travellers, AC 3-Tier, SBC->MAS -> one PNR, three seats, 2,625.00.
TEST_P(BookingServiceTest, ST_BKG_02_MultiTravellerBooking) {
    auto booked = service_->createBooking(
        p1_, request("r1", "20001", "SBC", "MAS", ClassCode::AC3,
                     {traveller("Asha Rao", 28, 'F'), traveller("Ravi Rao", 31, 'M'), traveller("Mini Rao", 6, 'F')}));
    ASSERT_TRUE(booked.ok()) << formatError(booked.error());
    EXPECT_EQ(booked.value().status, TicketStatus::Confirmed);
    EXPECT_EQ(booked.value().fare.toString(), "2,625.00");
    std::vector<std::string> seats;
    for (const Traveller& t : booked.value().travellers) seats.push_back(t.seatNumber);
    EXPECT_EQ(seats, (std::vector<std::string>{"B1-01", "B1-02", "B1-03"}));
}

// RAC path (FR-018): 20003 3A has 2 seats, RAC 1, WL 2.
TEST_P(BookingServiceTest, ConfirmedThenRacThenWaitlistThenNoCapacity) {
    std::vector<TicketStatus> statuses;
    for (int i = 0; i < 5; ++i) {
        auto r = service_->createBooking(p1_, single("r" + std::to_string(i), ClassCode::AC3));
        ASSERT_TRUE(r.ok()) << i;
        statuses.push_back(r.value().status);
        if (r.value().status == TicketStatus::Rac) {
            EXPECT_TRUE(r.value().travellers[0].seatNumber.empty());
        }
    }
    EXPECT_EQ(statuses, (std::vector<TicketStatus>{TicketStatus::Confirmed, TicketStatus::Confirmed,
                                                   TicketStatus::Rac, TicketStatus::Waitlisted,
                                                   TicketStatus::Waitlisted}));
    EXPECT_EQ(service_->createBooking(p1_, single("r5", ClassCode::AC3)).code(), ErrorCode::NoCapacity);
    Availability a = availability(ClassCode::AC3);
    EXPECT_EQ(a.confirmedFree, 0);
    EXPECT_EQ(a.racFree, 0);
    EXPECT_EQ(a.waitlistCount, 2);
}

TEST_P(BookingServiceTest, StorageFailureRollsBackAndRetryWithSameTokenSucceedsOnce) {
    faulty_->failNextInserts = 1;
    BookingRequest r = single("retry-me");
    EXPECT_EQ(service_->createBooking(p1_, r).code(), ErrorCode::Database);
    EXPECT_EQ(reservationCount(), 0);
    EXPECT_EQ(availability().confirmedFree, 2);

    auto retry = service_->createBooking(p1_, r);
    ASSERT_TRUE(retry.ok());
    auto again = service_->createBooking(p1_, r);
    ASSERT_TRUE(again.ok());
    EXPECT_EQ(retry.value().pnr, again.value().pnr);
    EXPECT_EQ(reservationCount(), 1);
}

// ---------------------------------------------------------------- access control

TEST_P(BookingServiceTest, ExpiredOrEmptySessionIsE1005) {
    Session expired = p1_;
    clock_.advanceSeconds(16 * 60);
    EXPECT_EQ(service_->createBooking(expired, single("r1")).code(), ErrorCode::SessionExpired);
    EXPECT_EQ(service_->createBooking(Session{}, single("r2")).code(), ErrorCode::SessionExpired);
    EXPECT_EQ(reservationCount(), 0);
}

TEST_P(BookingServiceTest, AdminAndOpsCannotBookE1006) {
    for (Role role : {Role::Admin, Role::Ops}) {
        Session s = test::sessionFor(seed::kA1, role, clock_);
        EXPECT_EQ(service_->createBooking(s, single("r1")).code(), ErrorCode::Forbidden);
    }
    EXPECT_EQ(audit_.count("UNAUTHORIZED_ATTEMPT"), 2);
    EXPECT_EQ(reservationCount(), 0);
}

TEST_P(BookingServiceTest, PassengerCannotBookForSomeoneElse) {
    BookingRequest r = single("r1");
    r.onBehalfOf = seed::kP2;
    EXPECT_EQ(service_->createBooking(p1_, r).code(), ErrorCode::Forbidden);
}

TEST_P(BookingServiceTest, StaffBooksOnBehalfAndIsRecordedAsBookedBy) {
    Session staff = test::sessionFor(seed::kS1, Role::Staff, clock_);
    BookingRequest r = single("r1");
    EXPECT_EQ(service_->createBooking(staff, r).code(), ErrorCode::Validation);   // passenger required
    r.onBehalfOf = seed::kP2;
    auto booked = service_->createBooking(staff, r);
    ASSERT_TRUE(booked.ok());
    EXPECT_EQ(booked.value().accountId, seed::kP2);
    EXPECT_EQ(booked.value().bookedBy, seed::kS1);
    r.requestId = "r2";
    r.onBehalfOf = seed::kA1;   // not a passenger account
    EXPECT_EQ(service_->createBooking(staff, r).code(), ErrorCode::Validation);
}

// ---------------------------------------------------------------- journey checks

TEST_P(BookingServiceTest, WrongDirectionOrUnknownTrainIsE2001) {
    EXPECT_EQ(service_->quoteFare("20003", "MYS", "SBC", ClassCode::SL, 1).code(), ErrorCode::TrainNotFound);
    EXPECT_EQ(service_->quoteFare("99999", "SBC", "MYS", ClassCode::SL, 1).code(), ErrorCode::TrainNotFound);
    BookingRequest r = single("r1");
    r.sourceCode = "MYS";
    r.destinationCode = "SBC";
    EXPECT_EQ(service_->createBooking(p1_, r).code(), ErrorCode::TrainNotFound);
}

TEST_P(BookingServiceTest, TrainNotRunningOnTheDateIsE2001) {
    BookingRequest r = single("r1");
    ASSERT_TRUE(data_->trains.removeSchedule("20003", r.journeyDate).ok());
    EXPECT_EQ(service_->createBooking(p1_, r).code(), ErrorCode::TrainNotFound);
}

TEST_P(BookingServiceTest, IntermediateStopFareUsesSegmentDistance) {
    auto q = service_->quoteFare("20001", "KPD", "MAS", ClassCode::SL, 2);
    ASSERT_TRUE(q.ok());
    EXPECT_EQ(q.value().distanceKm, 140);
    EXPECT_EQ(q.value().total.toString(), "280.00");
}

TEST_P(BookingServiceTest, DateOutsideBookingWindowIsRejected) {
    BookingRequest r = request("r1", "20003", "SBC", "MYS", ClassCode::SL, {traveller("Asha")}, 121);
    EXPECT_EQ(service_->createBooking(p1_, r).code(), ErrorCode::Validation);
}

// ---------------------------------------------------------------- ticket display (#31)

TEST_P(BookingServiceTest, OwnerAndStaffCanViewTicketOthersCannot) {
    auto booked = service_->createBooking(p1_, single("r1"));
    ASSERT_TRUE(booked.ok());
    const std::string pnr = booked.value().pnr;

    auto mine = service_->getTicket(p1_, pnr);
    ASSERT_TRUE(mine.ok());
    EXPECT_EQ(mine.value().travellers[0].seatNumber, "S1-01");

    EXPECT_EQ(service_->getTicket(p2_, pnr).code(), ErrorCode::Forbidden);
    EXPECT_EQ(audit_.count("UNAUTHORIZED_ATTEMPT"), 1);
    EXPECT_TRUE(service_->getTicket(test::sessionFor(seed::kS1, Role::Staff, clock_), pnr).ok());
    EXPECT_EQ(service_->getTicket(test::sessionFor(seed::kO1, Role::Ops, clock_), pnr).code(), ErrorCode::Forbidden);
    EXPECT_EQ(service_->getTicket(p1_, "9999999999").code(), ErrorCode::PnrNotFound);
    EXPECT_EQ(service_->getTicket(p1_, "' OR 1=1").code(), ErrorCode::Validation);
}


// IT-CAN-06 / Issue #34: PNR lookup must display the actual stored status.
TEST_P(BookingServiceTest, IT_CAN_06_PnrLookupDisplaysStoredReservationStatus) {

    // 1. Create a confirmed Sleeper reservation.
    auto confirmed = service_->createBooking(
        p1_, single("issue34-confirmed", ClassCode::SL));
    ASSERT_TRUE(confirmed.ok()) << formatError(confirmed.error());

    // 2. AC3 has capacity for 2 Confirmed, 1 RAC and 2 Waitlisted.
    auto ac3First = service_->createBooking(
        p1_, single("issue34-ac3-1", ClassCode::AC3));
    auto ac3Second = service_->createBooking(
        p1_, single("issue34-ac3-2", ClassCode::AC3));
    auto rac = service_->createBooking(
        p1_, single("issue34-rac", ClassCode::AC3));
    auto waitlist1 = service_->createBooking(
        p1_, single("issue34-wl-1", ClassCode::AC3));
    auto waitlist2 = service_->createBooking(
        p1_, single("issue34-wl-2", ClassCode::AC3));

    ASSERT_TRUE(ac3First.ok()) << formatError(ac3First.error());
    ASSERT_TRUE(ac3Second.ok()) << formatError(ac3Second.error());
    ASSERT_TRUE(rac.ok()) << formatError(rac.error());
    ASSERT_TRUE(waitlist1.ok()) << formatError(waitlist1.error());
    ASSERT_TRUE(waitlist2.ok()) << formatError(waitlist2.error());

    // 3. Create a separate booking and cancel it.
    BookingRequest cancelRequest = request(
        "issue34-cancel",
        "20003",
        "SBC",
        "MYS",
        ClassCode::SL,
        {traveller("Cancelled passenger")},
        8);

    auto cancelledBooking = service_->createBooking(p1_, cancelRequest);
    ASSERT_TRUE(cancelledBooking.ok())
        << formatError(cancelledBooking.error());

    SeatReleaseService release(*data_, clock_, audit_);
    auto cancellation = release.cancelAndRelease(
        cancelledBooking.value().pnr, std::nullopt, seed::kP1);

    ASSERT_TRUE(cancellation.ok()) << formatError(cancellation.error());

    // 4. Retrieve each ticket and compare its stored, retrieved and displayed
    // status. This verifies the complete PNR lookup path.
const auto verifyStatus =
    [this](const Reservation& booking,
           TicketStatus expectedStatus,
           const std::string& expectedText,
           int expectedWaitlistPosition = 0) {

        auto stored = data_->reservations.findByPnr(booking.pnr);
        ASSERT_TRUE(stored.ok());
        ASSERT_TRUE(stored.value().has_value());

        auto ticket = service_->getTicket(p1_, booking.pnr);
        ASSERT_TRUE(ticket.ok()) << formatError(ticket.error());

        EXPECT_EQ(stored.value()->status, expectedStatus);
        EXPECT_EQ(ticket.value().status, stored.value()->status);

        const std::string displayed =
            formatTicket(ticket.value(), "Test Train");

        EXPECT_NE(
            displayed.find("Status  : " + expectedText),
            std::string::npos
        );

        if (expectedStatus == TicketStatus::Waitlisted) {
            EXPECT_EQ(ticket.value().waitlistPosition,
                      expectedWaitlistPosition);
        }
    };

verifyStatus(
    confirmed.value(),
    TicketStatus::Confirmed,
    "Confirmed");

verifyStatus(
    rac.value(),
    TicketStatus::Rac,
    "RAC");

verifyStatus(
    waitlist1.value(),
    TicketStatus::Waitlisted,
    "Waitlisted - position 1",
    1);

verifyStatus(
    waitlist2.value(),
    TicketStatus::Waitlisted,
    "Waitlisted - position 2",
    2);

verifyStatus(
    cancelledBooking.value(),
    TicketStatus::Cancelled,
    "Cancelled");
}

INSTANTIATE_TEST_SUITE_P(Backends, BookingServiceTest, ::testing::ValuesIn(test::backendKinds()),
                         [](const auto& info) { return info.param; });
