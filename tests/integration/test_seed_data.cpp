#include <gtest/gtest.h>

#include "seed_data.h"
#include "support/test_support.h"

using namespace rrs;

namespace {

class DemoSeedDataTest : public ::testing::TestWithParam<std::string> {
protected:
    void SetUp() override {
        backend_ = test::makeBackend(GetParam());
        data_ = std::make_unique<DataAccess>(backend_->access());
    }

    std::unique_ptr<test::Backend> backend_;
    std::unique_ptr<DataAccess> data_;
};

TEST_P(DemoSeedDataTest, CreatesExpectedStationsTrainsAndAccounts) {
    ASSERT_TRUE(seed::loadDemoSeedData(*data_, test::testToday(), 10).ok());

    auto stations = data_->trains.listStations();
    ASSERT_TRUE(stations.ok());
    EXPECT_EQ(stations.value().size(), 5u);

    auto trains = data_->trains.listTrains();
    ASSERT_TRUE(trains.ok());
    EXPECT_EQ(trains.value().size(), 4u);

    auto p1 = data_->users.findById(seed::kP1);
    auto p2 = data_->users.findById(seed::kP2);
    auto staff = data_->users.findById(seed::kS1);
    auto admin = data_->users.findById(seed::kA1);
    auto ops = data_->users.findById(seed::kO1);

    ASSERT_TRUE(p1.ok());
    ASSERT_TRUE(p2.ok());
    ASSERT_TRUE(staff.ok());
    ASSERT_TRUE(admin.ok());
    ASSERT_TRUE(ops.ok());

    EXPECT_TRUE(p1.value().has_value());
    EXPECT_TRUE(p2.value().has_value());
    EXPECT_TRUE(staff.value().has_value());
    EXPECT_TRUE(admin.value().has_value());
    EXPECT_TRUE(ops.value().has_value());

    EXPECT_EQ(p1.value()->role, Role::Passenger);
    EXPECT_EQ(staff.value()->role, Role::Staff);
    EXPECT_EQ(admin.value()->role, Role::Admin);
    EXPECT_EQ(ops.value()->role, Role::Ops);
}

TEST_P(DemoSeedDataTest, CreatesFourSampleReservations) {
    ASSERT_TRUE(seed::loadDemoSeedData(*data_, test::testToday(), 10).ok());

    auto p1 = data_->reservations.listForAccount(seed::kP1);
    auto p2 = data_->reservations.listForAccount(seed::kP2);

    ASSERT_TRUE(p1.ok());
    ASSERT_TRUE(p2.ok());

    EXPECT_EQ(p1.value().size(), 2u);
    EXPECT_EQ(p2.value().size(), 2u);

    auto confirmed = data_->reservations.findByPnr("9000000001");
    ASSERT_TRUE(confirmed.ok());
    ASSERT_TRUE(confirmed.value().has_value());

    EXPECT_EQ(confirmed.value()->status, TicketStatus::Confirmed);
    EXPECT_EQ(confirmed.value()->trainNumber, "20001");
    EXPECT_EQ(confirmed.value()->travellers.size(), 1u);
    EXPECT_EQ(confirmed.value()->travellers[0].seatNumber, "S1-01");

    auto waitlisted = data_->reservations.findByPnr("9000000003");
    ASSERT_TRUE(waitlisted.ok());
    ASSERT_TRUE(waitlisted.value().has_value());

    EXPECT_EQ(waitlisted.value()->status, TicketStatus::Waitlisted);
    EXPECT_EQ(waitlisted.value()->waitlistPosition, 1);
    EXPECT_TRUE(waitlisted.value()->travellers[0].seatNumber.empty());

    // This reservation was created by staff on behalf of a passenger.
    auto staffBooking = data_->reservations.findByPnr("9000000004");
    ASSERT_TRUE(staffBooking.ok());
    ASSERT_TRUE(staffBooking.value().has_value());

    EXPECT_EQ(staffBooking.value()->accountId, seed::kP2);
    EXPECT_EQ(staffBooking.value()->bookedBy, seed::kS1);
}

TEST_P(DemoSeedDataTest, RunningCompleteSeedTwiceDoesNotDuplicateData) {
    const Date start = test::testToday();

    ASSERT_TRUE(seed::loadDemoSeedData(*data_, start, 10).ok());

    auto firstP1 = data_->reservations.listForAccount(seed::kP1);
    auto firstP2 = data_->reservations.listForAccount(seed::kP2);

    ASSERT_TRUE(firstP1.ok());
    ASSERT_TRUE(firstP2.ok());

    ASSERT_TRUE(seed::loadDemoSeedData(*data_, start, 10).ok());

    auto secondP1 = data_->reservations.listForAccount(seed::kP1);
    auto secondP2 = data_->reservations.listForAccount(seed::kP2);

    ASSERT_TRUE(secondP1.ok());
    ASSERT_TRUE(secondP2.ok());

    EXPECT_EQ(secondP1.value().size(), firstP1.value().size());
    EXPECT_EQ(secondP2.value().size(), firstP2.value().size());

    EXPECT_EQ(secondP1.value().size(), 2u);
    EXPECT_EQ(secondP2.value().size(), 2u);

    EXPECT_EQ(secondP1.value()[0].pnr, firstP1.value()[0].pnr);
    EXPECT_EQ(secondP1.value()[1].pnr, firstP1.value()[1].pnr);
    EXPECT_EQ(secondP2.value()[0].pnr, firstP2.value()[0].pnr);
    EXPECT_EQ(secondP2.value()[1].pnr, firstP2.value()[1].pnr);
}

TEST_P(DemoSeedDataTest, BaseSeedDoesNotCreateSampleReservations) {
    ASSERT_TRUE(seed::loadSeedData(*data_, test::testToday(), 10).ok());

    auto p1 = data_->reservations.listForAccount(seed::kP1);
    auto p2 = data_->reservations.listForAccount(seed::kP2);

    ASSERT_TRUE(p1.ok());
    ASSERT_TRUE(p2.ok());

    EXPECT_TRUE(p1.value().empty());
    EXPECT_TRUE(p2.value().empty());
}

TEST_P(DemoSeedDataTest, CompleteSeedRejectsTooFewScheduleDays) {
    Status result = seed::loadDemoSeedData(*data_, test::testToday(), 5);

    EXPECT_FALSE(result.ok());

    auto stations = data_->trains.listStations();
    ASSERT_TRUE(stations.ok());
    EXPECT_TRUE(stations.value().empty());
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    AllBackends,
    DemoSeedDataTest,
    ::testing::ValuesIn(test::backendKinds()));
