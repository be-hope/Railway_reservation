// test_repositories.cpp - repository contract tests (issue #21). Every test
// runs against the in-memory fakes AND the SQLite implementation, so the
// fakes used by unit tests are proven to behave like the real database.

#include <gtest/gtest.h>

#include "seed_data.h"
#include "support/test_support.h"

using namespace rrs;

namespace {

class RepositoryContract : public ::testing::TestWithParam<std::string> {
protected:
    void SetUp() override {
        backend_ = test::makeBackend(GetParam());
        data_ = std::make_unique<DataAccess>(backend_->access());
        ASSERT_TRUE(seed::loadSeedData(*data_, test::testToday(), 10).ok());
    }
    DataAccess& data() { return *data_; }

    std::unique_ptr<test::Backend> backend_;
    std::unique_ptr<DataAccess> data_;
};

Reservation makeReservation(const std::string& pnr, const std::string& account, const Date& date,
                            TicketStatus status, std::int64_t seq, std::vector<std::string> seats) {
    Reservation r;
    r.pnr = pnr;
    r.requestId = "req-" + pnr;
    r.accountId = account;
    r.bookedBy = account;
    r.trainNumber = "20003";
    r.journeyDate = date;
    r.classCode = ClassCode::SL;
    r.sourceCode = "SBC";
    r.destinationCode = "MYS";
    r.status = status;
    r.waitlistPosition = status == TicketStatus::Waitlisted ? 1 : 0;
    r.fare = Money::fromRupees(140) * static_cast<std::int64_t>(seats.size());
    r.queueSeq = seq;
    for (const std::string& s : seats) {
        Traveller t = test::traveller("T" + std::to_string(r.travellers.size() + 1));
        t.seatNumber = s;
        r.travellers.push_back(t);
    }
    return r;
}

}  // namespace

TEST_P(RepositoryContract, FindTrainsHonoursDirectionScheduleAndActiveFlag) {
    const Date d = test::testToday().plusDays(3);
    auto sbcMas = data().trains.findTrains("SBC", "MAS", d);
    ASSERT_TRUE(sbcMas.ok());
    ASSERT_EQ(sbcMas.value().size(), 1u);
    EXPECT_EQ(sbcMas.value()[0].number, "20001");   // 20002 runs the other way

    EXPECT_TRUE(data().trains.findTrains("MYS", "SBC", d).value().empty());
    EXPECT_EQ(data().trains.findTrains("SBC", "KPD", d).value().size(), 1u);
    EXPECT_TRUE(data().trains.findTrains("SBC", "MAS", test::testToday().plusDays(30)).value().empty());

    ASSERT_TRUE(data().trains.setTrainActive("20001", false).ok());
    EXPECT_TRUE(data().trains.findTrains("SBC", "MAS", d).value().empty());
}

TEST_P(RepositoryContract, TrainRoundTripKeepsOrderedStops) {
    auto t = data().trains.findTrain("20004");
    ASSERT_TRUE(t.ok() && t.value());
    ASSERT_EQ(t.value()->stops.size(), 2u);
    EXPECT_EQ(t.value()->stops[1].stationCode, "SC");
    EXPECT_EQ(t.value()->stops[1].dayOffset, 1);
    EXPECT_EQ(t.value()->stops[1].distanceKm, 570);
    EXPECT_EQ(t.value()->stops[0].arrivalMinutes, -1);
    EXPECT_FALSE(data().trains.findTrain("99999").value().has_value());
}

TEST_P(RepositoryContract, SchedulesCanBeAddedAndRemoved) {
    const Date d = test::testToday().plusDays(50);
    EXPECT_FALSE(data().trains.hasSchedule("20003", d).value());
    ASSERT_TRUE(data().trains.addSchedule("20003", d).ok());
    EXPECT_TRUE(data().trains.hasSchedule("20003", d).value());
    ASSERT_TRUE(data().trains.removeSchedule("20003", d).ok());
    EXPECT_FALSE(data().trains.hasSchedule("20003", d).value());
    EXPECT_EQ(data().trains.listSchedules("20003").value().size(), 10u);
}

TEST_P(RepositoryContract, ClassConfigRoundTrip) {
    auto c = data().config.findClassConfig("20003", ClassCode::AC3);
    ASSERT_TRUE(c.ok() && c.value());
    EXPECT_EQ(c.value()->capacity(), 2);
    EXPECT_EQ(c.value()->racQuota, 1);
    EXPECT_EQ(c.value()->fareMultiplierPercent, 250);
    ClassConfig changed = *c.value();
    changed.coaches = 3;
    ASSERT_TRUE(data().config.saveClassConfig(changed).ok());
    EXPECT_EQ(data().config.findClassConfig("20003", ClassCode::AC3).value()->capacity(), 6);
    EXPECT_EQ(data().config.listClassConfigs("20003").value().size(), 2u);
}

TEST_P(RepositoryContract, ReservationRoundTripAndOccupancy) {
    const Date d = test::testToday().plusDays(2);
    auto& repo = data().reservations;
    ASSERT_TRUE(repo.insert(makeReservation("1000000001", seed::kP1, d, TicketStatus::Confirmed, 1, {"S1-01"})).ok());
    ASSERT_TRUE(repo.insert(makeReservation("1000000002", seed::kP2, d, TicketStatus::Waitlisted, 2, {""})).ok());

    auto occ = repo.occupancy("20003", d, ClassCode::SL);
    ASSERT_TRUE(occ.ok());
    EXPECT_EQ(occ.value().confirmedTravellers, 1);
    EXPECT_EQ(occ.value().waitlistTickets, 1);
    EXPECT_EQ(occ.value().occupiedSeats, std::vector<std::string>{"S1-01"});

    auto back = repo.findByRequestId("req-1000000001");
    ASSERT_TRUE(back.ok() && back.value());
    EXPECT_EQ(back.value()->pnr, "1000000001");
    EXPECT_EQ(back.value()->travellers[0].seatNumber, "S1-01");
    EXPECT_EQ(back.value()->fare.toString(), "140.00");
    EXPECT_EQ(repo.nextQueueSeq().value(), 3);
    EXPECT_EQ(repo.listActiveForTrip("20003", d, ClassCode::SL).value().size(), 2u);
}

TEST_P(RepositoryContract, UpdateReleasesSeatAndCancelledTicketsDropOutOfOccupancy) {
    const Date d = test::testToday().plusDays(2);
    auto& repo = data().reservations;
    Reservation r = makeReservation("1000000001", seed::kP1, d, TicketStatus::Confirmed, 1, {"S1-01"});
    ASSERT_TRUE(repo.insert(r).ok());
    r.status = TicketStatus::Cancelled;
    r.travellers[0].seatNumber.clear();
    r.refund = Money::fromRupees(126);
    r.cancelledAt = 99;
    ASSERT_TRUE(repo.update(r).ok());

    auto occ = repo.occupancy("20003", d, ClassCode::SL).value();
    EXPECT_EQ(occ.confirmedTravellers, 0);
    EXPECT_TRUE(occ.occupiedSeats.empty());
    auto back = repo.findByPnr("1000000001").value();
    ASSERT_TRUE(back);
    EXPECT_EQ(back->status, TicketStatus::Cancelled);
    ASSERT_TRUE(back->refund.has_value());
    EXPECT_EQ(back->refund->toString(), "126.00");
    EXPECT_TRUE(repo.listActiveForTrip("20003", d, ClassCode::SL).value().empty());
    EXPECT_EQ(repo.update(makeReservation("9999999999", seed::kP1, d, TicketStatus::Confirmed, 9, {"S1-02"})).code(),
              ErrorCode::PnrNotFound);
}

TEST_P(RepositoryContract, ListForAccountIsOrderedAndScoped) {
    auto& repo = data().reservations;
    const Date t = test::testToday();
    ASSERT_TRUE(repo.insert(makeReservation("1000000003", seed::kP1, t.plusDays(5), TicketStatus::Confirmed, 1, {"S1-01"})).ok());
    ASSERT_TRUE(repo.insert(makeReservation("1000000001", seed::kP1, t.plusDays(1), TicketStatus::Confirmed, 2, {"S1-01"})).ok());
    ASSERT_TRUE(repo.insert(makeReservation("1000000002", seed::kP2, t.plusDays(2), TicketStatus::Confirmed, 3, {"S1-01"})).ok());
    auto mine = repo.listForAccount(seed::kP1).value();
    ASSERT_EQ(mine.size(), 2u);
    EXPECT_EQ(mine[0].pnr, "1000000001");
    EXPECT_EQ(mine[1].pnr, "1000000003");
    EXPECT_EQ(repo.listByJourneyDate(t, t.plusDays(2)).value().size(), 2u);
    EXPECT_EQ(repo.countActiveForTrain("20003", t.plusDays(2)).value(), 2);
}

TEST_P(RepositoryContract, RollbackDiscardsEverythingSinceBegin) {
    const Date d = test::testToday().plusDays(2);
    {
        TransactionGuard tx(data().unitOfWork);
        ASSERT_TRUE(tx.started().ok());
        ASSERT_TRUE(data().reservations.insert(
            makeReservation("1000000001", seed::kP1, d, TicketStatus::Confirmed, 1, {"S1-01"})).ok());
        // no commit -> guard rolls back
    }
    EXPECT_FALSE(data().reservations.pnrExists("1000000001").value());

    TransactionGuard tx(data().unitOfWork);
    ASSERT_TRUE(data().reservations.insert(
        makeReservation("1000000002", seed::kP1, d, TicketStatus::Confirmed, 1, {"S1-01"})).ok());
    ASSERT_TRUE(tx.commit().ok());
    EXPECT_TRUE(data().reservations.pnrExists("1000000002").value());
}

TEST_P(RepositoryContract, DuplicateAccountsAreRejectedWithE1002) {
    Account a;
    a.accountId = "acc-new";
    a.loginId = "9876500001";   // P1's login
    a.name = "Someone";
    a.passwordHash = "h";
    a.salt = "s";
    EXPECT_EQ(data().users.insert(a).code(), ErrorCode::DuplicateUser);
    a.loginId = "9876500099";
    a.contactNumber = "9876500099";
    EXPECT_TRUE(data().users.insert(a).ok());
    EXPECT_EQ(data().users.findByLoginId("9876500099").value()->accountId, "acc-new");
}

TEST_P(RepositoryContract, ProfileAndAttemptsUpdate) {
    auto p1 = data().users.findById(seed::kP1).value();
    ASSERT_TRUE(p1);
    Account changed = *p1;
    changed.age = 28;
    changed.gender = 'F';
    changed.idProofType = "DL";
    changed.idProofNumber = "KA012026001";
    ASSERT_TRUE(data().users.updateProfile(changed).ok());
    ASSERT_TRUE(data().users.updateAttempts(seed::kP1, 3, 1000).ok());
    auto back = data().users.findById(seed::kP1).value();
    EXPECT_EQ(back->age, 28);
    EXPECT_EQ(back->idProofNumber, "KA012026001");
    EXPECT_EQ(back->failedAttempts, 3);
    EXPECT_EQ(back->lockedUntil, 1000);
}

INSTANTIATE_TEST_SUITE_P(Backends, RepositoryContract, ::testing::ValuesIn(test::backendKinds()),
                         [](const auto& info) { return info.param; });
