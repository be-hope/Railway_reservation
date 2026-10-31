// test_sqlite_schema.cpp - database constraints of rrs.db (issue #21,
// IT-BKG-05 backstop, SRS §5.2): duplicate PNRs, request tokens and seats
// cannot be inserted even if the application logic has a defect.

#include <gtest/gtest.h>
#include <algorithm>

#include <cstdio>

#include "dal/sqlite_database.h"
#include "dal/sqlite_repositories.h"
#include "seed_data.h"
#include "support/test_support.h"

using namespace rrs;

namespace {

class SqliteSchemaTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto store = SqliteDataStore::open(":memory:");
        ASSERT_TRUE(store.ok()) << formatError(store.error());
        store_ = std::move(store.value());
        DataAccess data = store_->access();
        ASSERT_TRUE(seed::loadSeedData(data, test::testToday(), 10).ok());
    }

    Reservation reservation(const std::string& pnr, const std::string& requestId, const std::string& seat) {
        Reservation r;
        r.pnr = pnr;
        r.requestId = requestId;
        r.accountId = seed::kP1;
        r.bookedBy = seed::kP1;
        r.trainNumber = "20003";
        r.journeyDate = test::testToday().plusDays(1);
        r.classCode = ClassCode::SL;
        r.sourceCode = "SBC";
        r.destinationCode = "MYS";
        r.status = seat.empty() ? TicketStatus::Waitlisted : TicketStatus::Confirmed;
        r.waitlistPosition = seat.empty() ? 1 : 0;
        r.fare = Money::fromRupees(140);
        r.queueSeq = ++seq_;
        r.travellers.push_back(test::traveller("Asha Rao"));
        r.travellers[0].seatNumber = seat;
        return r;
    }

    std::unique_ptr<SqliteDataStore> store_;
    std::int64_t seq_ = 0;
};

}  // namespace

TEST_F(SqliteSchemaTest, MigrateIsIdempotentAndRecordsVersion) {
    ASSERT_TRUE(store_->database().migrate().ok());
    Statement st(store_->database(), "SELECT COUNT(*), MAX(version) FROM schema_version");
    ASSERT_TRUE(st.step());
    EXPECT_EQ(st.integer(0), 1);
    EXPECT_EQ(st.integer(1), 1);
    EXPECT_TRUE(store_->database().integrityCheck().ok());
}

TEST_F(SqliteSchemaTest, IndexesForSearchAndSeatsExist) {
    Statement st(store_->database(), "SELECT name FROM sqlite_master WHERE type = 'index' ORDER BY name");
    std::vector<std::string> names;
    while (st.step()) names.push_back(st.text(0));
    for (const char* expected : {"ix_route_stop_station", "ix_schedule_date", "ix_reservation_trip", "ux_seat"}) {
        EXPECT_NE(std::find(names.begin(), names.end(), expected), names.end()) << expected;
    }
}

TEST_F(SqliteSchemaTest, DuplicatePnrIsRejected) {
    auto& repo = store_->access().reservations;
    ASSERT_TRUE(repo.insert(reservation("1234567890", "r1", "S1-01")).ok());
    EXPECT_EQ(repo.insert(reservation("1234567890", "r2", "S1-02")).code(), ErrorCode::Database);
}

TEST_F(SqliteSchemaTest, DuplicateRequestIdIsRejected) {
    auto& repo = store_->access().reservations;
    ASSERT_TRUE(repo.insert(reservation("1234567890", "same", "S1-01")).ok());
    EXPECT_EQ(repo.insert(reservation("1234567891", "same", "S1-02")).code(), ErrorCode::Database);
}

TEST_F(SqliteSchemaTest, SeatCannotBeAssignedTwiceButIsReusableAfterRelease) {
    auto& repo = store_->access().reservations;
    Reservation first = reservation("1234567890", "r1", "S1-01");
    ASSERT_TRUE(repo.insert(first).ok());
    EXPECT_EQ(repo.insert(reservation("1234567891", "r2", "S1-01")).code(), ErrorCode::Database);

    first.status = TicketStatus::Cancelled;
    first.travellers[0].seatNumber.clear();
    first.cancelledAt = 1;
    ASSERT_TRUE(repo.update(first).ok());
    EXPECT_TRUE(repo.insert(reservation("1234567892", "r3", "S1-01")).ok());
}

TEST_F(SqliteSchemaTest, MalformedPnrAndInconsistentWaitlistRowsAreRejected) {
    auto& repo = store_->access().reservations;
    EXPECT_FALSE(repo.insert(reservation("0123456789", "r1", "S1-01")).ok());
    EXPECT_FALSE(repo.insert(reservation("12345", "r2", "S1-01")).ok());
    Reservation wl = reservation("1234567890", "r3", "");
    wl.waitlistPosition = 0;   // WAITLISTED without a position
    EXPECT_FALSE(repo.insert(wl).ok());
}

TEST_F(SqliteSchemaTest, ReservationNeedsAScheduledTrainAndKnownAccount) {
    auto& repo = store_->access().reservations;
    Reservation r = reservation("1234567890", "r1", "S1-01");
    r.journeyDate = test::testToday().plusDays(200);   // no schedule row
    EXPECT_FALSE(repo.insert(r).ok());
    r = reservation("1234567891", "r2", "S1-01");
    r.accountId = "nobody";
    EXPECT_FALSE(repo.insert(r).ok());
}

TEST_F(SqliteSchemaTest, HostileTextIsStoredLiterallyNotExecuted) {
    auto& repo = store_->access().reservations;
    Reservation r = reservation("1234567890", "r1", "S1-01");
    r.travellers[0].name = "Robert'); DROP TABLE reservation;--";
    ASSERT_TRUE(repo.insert(r).ok());
    auto back = repo.findByPnr("1234567890");
    ASSERT_TRUE(back.ok() && back.value());
    EXPECT_EQ(back.value()->travellers[0].name, "Robert'); DROP TABLE reservation;--");
    EXPECT_TRUE(repo.pnrExists("1234567890").value());
}

TEST(SqliteFile, DataSurvivesReopeningTheFile) {
    const std::string path = ::testing::TempDir() + "rrs_persist_test.db";
    std::remove(path.c_str());
    {
        auto store = SqliteDataStore::open(path);
        ASSERT_TRUE(store.ok());
        DataAccess data = store.value()->access();
        ASSERT_TRUE(seed::loadSeedData(data, test::testToday(), 3).ok());
    }
    {
        auto store = SqliteDataStore::open(path);
        ASSERT_TRUE(store.ok());
        auto train = store.value()->access().trains.findTrain("20001");
        ASSERT_TRUE(train.ok() && train.value());
        EXPECT_EQ(train.value()->stops.size(), 3u);
    }
    std::remove(path.c_str());
}
