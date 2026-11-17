#include <gtest/gtest.h>
#include "admin/train_admin_service.h"
#include "booking/booking_service.h"
#include "cancel/cancellation_service.h"
#include "demo_adapters.h"
#include "seed_data.h"
#include "support/test_support.h"
#include "support/booking_helpers.h"
using namespace rrs;
namespace {
class FinalIntegration : public ::testing::TestWithParam<std::string> {
protected:
    FixedClock clock{test::testToday()}; test::RecordingAuditLogger audit;
    std::unique_ptr<test::Backend> backend; std::unique_ptr<DataAccess> data;
    std::unique_ptr<TrainAdminService> admin; std::unique_ptr<BookingService> booking;
    Session a, p, staff; PnrGenerator pnr;
    void SetUp() override {
        backend = test::makeBackend(GetParam()); data = std::make_unique<DataAccess>(backend->access());
        ASSERT_TRUE(seed::loadSeedData(*data, clock.today()).ok());
        admin = std::make_unique<TrainAdminService>(*data, clock, audit);
        booking = std::make_unique<BookingService>(*data, clock, audit, pnr);
        a = test::sessionFor(seed::kA1, Role::Admin, clock); p = test::sessionFor(seed::kP1, Role::Passenger, clock);
        staff = test::sessionFor(seed::kS1, Role::Staff, clock);
    }
    BookingRequest request() { return test::bookingRequest(*booking, "integrated", "20001", clock.today().plusDays(3), "SBC", "MAS", ClassCode::SL, {test::traveller("Asha")}); }
};
TEST_P(FinalIntegration, AdminAddsRenamesAndTogglesTrain) {
    Train t; t.number = "77777"; t.name = "New Train";
    EXPECT_EQ(admin->addTrain(p, t).code(), ErrorCode::Forbidden);
    ASSERT_TRUE(admin->addTrain(a, t).ok()); EXPECT_EQ(admin->addTrain(a, t).code(), ErrorCode::Validation);
    ASSERT_TRUE(admin->updateTrainName(a, t.number, "Renamed Train").ok());
    EXPECT_EQ(admin->getTrain(a, t.number)->name, "Renamed Train");
    ASSERT_TRUE(admin->deactivateTrain(a, t.number).ok()); EXPECT_FALSE(admin->getTrain(a, t.number)->active);
    ASSERT_TRUE(admin->activateTrain(a, t.number).ok()); EXPECT_TRUE(admin->getTrain(a, t.number)->active);
    EXPECT_EQ(admin->getTrain(a, "99999").code(), ErrorCode::TrainNotFound);
}
TEST_P(FinalIntegration, RouteAndScheduleValidation) {
    auto train = admin->getTrain(a, "20001"); ASSERT_TRUE(train.ok()); auto route = train->stops;
    ASSERT_TRUE(admin->setRoute(a, "20001", route).ok());
    auto bad = route; bad[1].distanceKm = -1; EXPECT_EQ(admin->setRoute(a, "20001", bad).code(), ErrorCode::Validation);
    bad = route; bad[1].stationCode = "ZZZ"; EXPECT_EQ(admin->setRoute(a, "20001", bad).code(), ErrorCode::Validation);
    bad = route; bad[1].arrivalMinutes = 10; EXPECT_EQ(admin->setRoute(a, "20001", bad).code(), ErrorCode::Validation);
    bad = route; bad[1].seq = 5; EXPECT_EQ(admin->setRoute(a, "20001", bad).code(), ErrorCode::Validation);
    EXPECT_EQ(admin->setRoute(a, "20001", {}).code(), ErrorCode::Validation);
    EXPECT_EQ(admin->addSchedule(a, "20001", clock.today().plusDays(-1)).code(), ErrorCode::Validation);
    ASSERT_TRUE(admin->addSchedule(a, "20001", clock.today().plusDays(130)).ok());
    ASSERT_TRUE(admin->removeSchedule(a, "20001", clock.today().plusDays(130)).ok());
}
TEST_P(FinalIntegration, ExistingBookingsProtectTrainRouteAndSchedule) {
    auto ticket = booking->createBooking(p, request()); ASSERT_TRUE(ticket.ok());
    auto route = admin->getRoute(a, "20001"); ASSERT_TRUE(route.ok());
    EXPECT_EQ(admin->deactivateTrain(a, "20001").code(), ErrorCode::TrainInUse);
    EXPECT_EQ(admin->setRoute(a, "20001", route.value()).code(), ErrorCode::TrainInUse);
    EXPECT_EQ(admin->removeSchedule(a, "20001", ticket->journeyDate).code(), ErrorCode::TrainInUse);
    EXPECT_EQ(data->reservations.findByPnr(ticket->pnr).value()->status, TicketStatus::Confirmed);
}
TEST_P(FinalIntegration, InactiveTrainHasNoAvailabilityAndSameStationHasNoFare) {
    ASSERT_TRUE(admin->deactivateTrain(a, "20001").ok());
    EXPECT_EQ(booking->checkAvailability("20001", clock.today().plusDays(3), ClassCode::SL).code(), ErrorCode::TrainNotFound);
    EXPECT_EQ(booking->quoteFare("20003", "SBC", "SBC", ClassCode::SL, 1).code(), ErrorCode::Validation);
}
TEST_P(FinalIntegration, SearchRejectsUnknownSameAndOutOfWindowInputs) {
    demo::RepositoryTrainSearch search(data->trains, &clock);
    EXPECT_EQ(search.search("SBC", "SBC", clock.today()).code(), ErrorCode::Validation);
    EXPECT_EQ(search.search("SBC", "ZZZ", clock.today()).code(), ErrorCode::Validation);
    EXPECT_EQ(search.search("SBC", "MAS", clock.today().plusDays(121)).code(), ErrorCode::Validation);
    auto rows = search.search("SBC", "MAS", clock.today()); ASSERT_TRUE(rows.ok()); EXPECT_EQ(rows->size(), 1u);
    auto reverse = search.search("MYS", "SBC", clock.today()); ASSERT_TRUE(reverse.ok()); EXPECT_TRUE(reverse->empty());
}
TEST_P(FinalIntegration, StaffCancelsWithRefundAndOpsIsDenied) {
    auto ticket = booking->createBooking(p, request()); ASSERT_TRUE(ticket.ok());
    SeatReleaseService release(*data, clock, audit); CancellationService cancel(*booking, data->trains, release, clock);
    auto ops = test::sessionFor(seed::kO1, Role::Ops, clock);
    EXPECT_EQ(cancel.cancel(ops, ticket->pnr).code(), ErrorCode::Forbidden);
    auto cancelled = cancel.cancel(staff, ticket->pnr); ASSERT_TRUE(cancelled.ok());
    EXPECT_EQ(cancelled->status, TicketStatus::Cancelled); EXPECT_EQ(cancelled->refund->paise(), 31500);
}
INSTANTIATE_TEST_SUITE_P(Backends, FinalIntegration, ::testing::ValuesIn(test::backendKinds()));
}
