#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "booking/booking_service.h"
#include "cancel/cancellation_service.h"
#include "cancel/seat_release_service.h"
#include "seed_data.h"
#include "support/booking_helpers.h"
#include "support/test_support.h"

using namespace rrs;
using test::traveller;

namespace {

class CancellationServiceTest : public ::testing::TestWithParam<std::string> {
protected:
    CancellationServiceTest() : clock_(test::testToday()) {}

    void SetUp() override {
        backend_ = test::makeBackend(GetParam());
        DataAccess raw = backend_->access();

        ASSERT_TRUE(seed::loadSeedData(raw, test::testToday()).ok());

        data_ = std::make_unique<DataAccess>(raw);
        booking_ = std::make_unique<BookingService>(*data_, clock_, audit_, pnr_);
        release_ = std::make_unique<SeatReleaseService>(*data_, clock_, audit_);

        cancellation_ = std::make_unique<CancellationService>(
            *booking_, data_->trains, *release_, clock_);

        p1_ = test::sessionFor(seed::kP1, Role::Passenger, clock_);
        p2_ = test::sessionFor(seed::kP2, Role::Passenger, clock_);
    }

    Date journeyDate() const {
        return test::testToday().plusDays(3);
    }

    Reservation book(const std::string& account = seed::kP1) {
        BookingRequest request = test::bookingRequest(
            *booking_,
            "cancel-test-" + std::to_string(++requestNumber_),
            "20003", journeyDate(), "SBC", "MYS", ClassCode::SL,
            {traveller("Test Passenger", 25, 'O', "ID123")});

        auto result = booking_->createBooking(
            test::sessionFor(account, Role::Passenger, clock_), request);

        EXPECT_TRUE(result.ok()) << (result.ok() ? "" : formatError(result.error()));
        return result.ok() ? result.value() : Reservation{};
    }

    Reservation reload(const std::string& pnr) {
        auto result = data_->reservations.findByPnr(pnr);
        EXPECT_TRUE(result.ok());
        EXPECT_TRUE(result.value().has_value());
        return result.value().value();
    }

    FixedClock clock_;
    test::RecordingAuditLogger audit_;
    PnrGenerator pnr_;

    std::unique_ptr<test::Backend> backend_;
    std::unique_ptr<DataAccess> data_;
    std::unique_ptr<BookingService> booking_;
    std::unique_ptr<SeatReleaseService> release_;
    std::unique_ptr<CancellationService> cancellation_;

    Session p1_;
    Session p2_;
    int requestNumber_ = 0;
};

TEST_P(CancellationServiceTest, CancelsOwnedConfirmedTicketAndStoresRefund) {
    Reservation ticket = book();

    ASSERT_EQ(ticket.status, TicketStatus::Confirmed);
    ASSERT_EQ(ticket.fare.toString(), "140.00");

    auto result = cancellation_->cancel(p1_, ticket.pnr);

    ASSERT_TRUE(result.ok()) << (result.ok() ? "" : formatError(result.error()));
    EXPECT_EQ(result.value().status, TicketStatus::Cancelled);

    ASSERT_TRUE(result.value().refund.has_value());
    EXPECT_EQ(result.value().refund->toString(), "126.00");

    EXPECT_TRUE(result.value().travellers[0].seatNumber.empty());
    EXPECT_EQ(reload(ticket.pnr).status, TicketStatus::Cancelled);

    auto availability = booking_->checkAvailability(
        "20003", journeyDate(), ClassCode::SL);

    ASSERT_TRUE(availability.ok());
    EXPECT_EQ(availability.value().confirmedFree, 2);
    EXPECT_EQ(audit_.count("TICKET_CANCELLED"), 1);
}

TEST_P(CancellationServiceTest, RejectsForeignAndUnknownPnrWithoutChangingTicket) {
    Reservation ticket = book();

    auto foreign = cancellation_->cancel(p2_, ticket.pnr);
    EXPECT_EQ(foreign.code(), ErrorCode::Forbidden);

    auto unknown = cancellation_->cancel(p1_, "9999999999");
    EXPECT_EQ(unknown.code(), ErrorCode::PnrNotFound);

    Reservation stored = reload(ticket.pnr);
    EXPECT_EQ(stored.status, TicketStatus::Confirmed);
    EXPECT_EQ(stored.travellers[0].seatNumber, "S1-01");
    EXPECT_FALSE(stored.refund.has_value());

    EXPECT_EQ(audit_.count("TICKET_CANCELLED"), 0);
}

TEST_P(CancellationServiceTest, RejectsAlreadyCancelledTicket) {
    Reservation ticket = book();

    auto first = cancellation_->cancel(p1_, ticket.pnr);
    ASSERT_TRUE(first.ok());

    auto second = cancellation_->cancel(p1_, ticket.pnr);
    EXPECT_EQ(second.code(), ErrorCode::AlreadyCancelled);

    EXPECT_EQ(reload(ticket.pnr).status, TicketStatus::Cancelled);
    EXPECT_EQ(audit_.count("TICKET_CANCELLED"), 1);
}

TEST_P(CancellationServiceTest, RejectsCancellationAfterDepartureWithoutChangingTicket) {
    Reservation ticket = book();

    // Advance beyond the train's 07:00 departure on the journey date.
    clock_.advanceSeconds(72LL * 60 * 60);

    // Refresh the test session so the test reaches the departure-time check.
    Session currentSession = test::sessionFor(seed::kP1, Role::Passenger, clock_);

    auto result = cancellation_->cancel(currentSession, ticket.pnr);
    EXPECT_EQ(result.code(), ErrorCode::CancelDeparted);

    Reservation stored = reload(ticket.pnr);
    EXPECT_EQ(stored.status, TicketStatus::Confirmed);
    EXPECT_EQ(stored.travellers[0].seatNumber, "S1-01");
    EXPECT_FALSE(stored.refund.has_value());
    EXPECT_EQ(audit_.count("TICKET_CANCELLED"), 0);
}

TEST_P(CancellationServiceTest, WaitlistedTicketReceivesFullRefund) {
    book();
    book();

    Reservation waitlisted = book();
    ASSERT_EQ(waitlisted.status, TicketStatus::Waitlisted);

    auto result = cancellation_->cancel(p1_, waitlisted.pnr);

    ASSERT_TRUE(result.ok()) << (result.ok() ? "" : formatError(result.error()));
    ASSERT_TRUE(result.value().refund.has_value());

    EXPECT_EQ(result.value().refund->paise(), waitlisted.fare.paise());
    EXPECT_EQ(result.value().status, TicketStatus::Cancelled);
    EXPECT_EQ(result.value().waitlistPosition, 0);
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    Backends, CancellationServiceTest,
    ::testing::ValuesIn(test::backendKinds()),
    [](const auto& info) { return info.param; });
