// test_report_service.cpp - operational reports (issue #41, FR-025):
// IT-RPT-01, IT-RPT-02 and "reports match database totals".

#include <gtest/gtest.h>

#include <map>

#include "admin/report_service.h"
#include "dal/sqlite_database.h"
#include "dal/sqlite_repositories.h"
#include "seed_data.h"
#include "support/test_support.h"
#include "ui/report_view.h"

using namespace rrs;

namespace {

// Inserts reservations directly ("10 reservations seeded" in IT-RPT-01).
class ReservationSeeder {
public:
    explicit ReservationSeeder(IReservationRepository& repo) : repo_(repo) {}

    Reservation add(const std::string& train, const Date& date, ClassCode cls, TicketStatus status,
                    int travellers = 1, std::int64_t farePaise = 14000, std::int64_t refundPaise = -1) {
        Reservation r;
        r.pnr = std::to_string(1000000000LL + ++n_);
        r.requestId = "seed-" + r.pnr;
        r.accountId = seed::kP1;
        r.bookedBy = seed::kP1;
        r.trainNumber = train;
        r.journeyDate = date;
        r.classCode = cls;
        r.sourceCode = train == "20002" ? "MAS" : "SBC";
        r.destinationCode = train == "20001" ? "MAS" : train == "20002" ? "SBC" : train == "20004" ? "SC" : "MYS";
        r.status = status;
        r.waitlistPosition = status == TicketStatus::Waitlisted ? ++wl_[train + date.iso()] : 0;
        r.fare = Money::fromPaise(farePaise);
        if (refundPaise >= 0) r.refund = Money::fromPaise(refundPaise);
        r.queueSeq = n_;
        for (int i = 0; i < travellers; ++i) {
            Traveller t = test::traveller("T" + std::to_string(i));
            if (status == TicketStatus::Confirmed) {
                t.seatNumber = coachPrefix(cls) + "1-" + (seat_[train + date.iso() + classCodeString(cls)] < 9 ? "0" : "") +
                               std::to_string(++seat_[train + date.iso() + classCodeString(cls)]);
            }
            r.travellers.push_back(t);
        }
        EXPECT_TRUE(repo_.insert(r).ok());
        return r;
    }

private:
    IReservationRepository& repo_;
    long long n_ = 0;
    std::map<std::string, int> wl_, seat_;
};

class ReportServiceTest : public ::testing::TestWithParam<std::string> {
protected:
    ReportServiceTest() : clock_(test::testToday()) {}

    void SetUp() override {
        backend_ = test::makeBackend(GetParam());
        data_ = std::make_unique<DataAccess>(backend_->access());
        ASSERT_TRUE(seed::loadSeedData(*data_, test::testToday(), 30).ok());
        reports_ = std::make_unique<ReportService>(*data_, clock_);
        seeder_ = std::make_unique<ReservationSeeder>(data_->reservations);
        ops_ = test::sessionFor(seed::kO1, Role::Ops, clock_);
    }

    Date day(int n) const { return test::testToday().plusDays(n); }
    DateRange range(int from, int to) const { return DateRange{day(from), day(to)}; }

    // IT-RPT-01 data: 6 CONFIRMED, 2 WAITLISTED, 2 CANCELLED.
    void seedTen() {
        seeder_->add("20001", day(1), ClassCode::SL, TicketStatus::Confirmed, 2, 70000);
        seeder_->add("20001", day(1), ClassCode::AC3, TicketStatus::Confirmed, 1, 87500);
        seeder_->add("20001", day(2), ClassCode::SL, TicketStatus::Confirmed);
        seeder_->add("20002", day(2), ClassCode::SL, TicketStatus::Confirmed);
        seeder_->add("20003", day(3), ClassCode::SL, TicketStatus::Confirmed);
        seeder_->add("20003", day(3), ClassCode::SL, TicketStatus::Confirmed);
        seeder_->add("20003", day(3), ClassCode::SL, TicketStatus::Waitlisted);
        seeder_->add("20003", day(3), ClassCode::SL, TicketStatus::Waitlisted);
        seeder_->add("20004", day(4), ClassCode::AC3, TicketStatus::Cancelled, 1, 142500, 128250);
        seeder_->add("20001", day(4), ClassCode::SL, TicketStatus::Cancelled, 1, 35000, 26250);
    }

    FixedClock clock_;
    std::unique_ptr<test::Backend> backend_;
    std::unique_ptr<DataAccess> data_;
    std::unique_ptr<ReportService> reports_;
    std::unique_ptr<ReservationSeeder> seeder_;
    Session ops_;
};

}  // namespace

// IT-RPT-01: report shows 6 / 2 / 2 and a total of 10.
TEST_P(ReportServiceTest, IT_RPT_01_BookingStatisticsByStatus) {
    seedTen();
    auto s = reports_->bookingStatistics(ops_, range(0, 30));
    ASSERT_TRUE(s.ok()) << formatError(s.error());
    EXPECT_EQ(s.value().total, 10);
    EXPECT_EQ(s.value().confirmed, 6);
    EXPECT_EQ(s.value().rac, 0);
    EXPECT_EQ(s.value().waitlisted, 2);
    EXPECT_EQ(s.value().cancelled, 2);
    // 700 + 875 + 140 * 6 (four 140s + two WL) = 2,415.00 ; refunds 1,282.50 + 262.50
    EXPECT_EQ(s.value().totalFare.toString(), "2,415.00");
    EXPECT_EQ(s.value().totalRefunds.toString(), "1,545.00");

    const std::string text = formatBookingStatistics(s.value());
    EXPECT_NE(text.find("Total reservations : 10"), std::string::npos);
    EXPECT_NE(text.find("Cancelled        : 2"), std::string::npos);
}

TEST_P(ReportServiceTest, StatisticsRespectTheDateRange) {
    seedTen();
    auto s = reports_->bookingStatistics(ops_, range(3, 3));
    ASSERT_TRUE(s.ok());
    EXPECT_EQ(s.value().total, 4);
    EXPECT_EQ(s.value().waitlisted, 2);
    EXPECT_EQ(reports_->bookingStatistics(ops_, range(40, 50)).value().total, 0);
}

// IT-RPT-02: 20003 SL (capacity 2) fully booked -> occupancy 100 %.
TEST_P(ReportServiceTest, IT_RPT_02_SeatOccupancy) {
    seedTen();
    auto rows = reports_->seatOccupancy(ops_, "20003", day(3));
    ASSERT_TRUE(rows.ok());
    ASSERT_EQ(rows.value().size(), 2u);
    const OccupancyRow* sl = nullptr;
    for (const OccupancyRow& r : rows.value()) {
        if (r.classCode == ClassCode::SL) sl = &r;
    }
    ASSERT_NE(sl, nullptr);
    EXPECT_EQ(sl->capacity, 2);
    EXPECT_EQ(sl->confirmedSeats, 2);
    EXPECT_EQ(sl->occupancyBasisPoints, 10000);
    EXPECT_EQ(sl->waitlistedTickets, 2);
    EXPECT_NE(formatSeatOccupancy(rows.value()).find("100.00 %"), std::string::npos);

    auto big = reports_->seatOccupancy(ops_, "20001", day(1)).value();
    for (const OccupancyRow& r : big) {
        if (r.classCode == ClassCode::SL) {
            EXPECT_EQ(r.capacity, 288);
            EXPECT_EQ(r.confirmedSeats, 2);
            EXPECT_EQ(formatBasisPoints(r.occupancyBasisPoints), "0.69 %");
        }
    }
    EXPECT_EQ(reports_->seatOccupancy(ops_, "55555", day(1)).code(), ErrorCode::TrainNotFound);
}

// IT-RPT-02: train-wise counts match the stored reservations.
TEST_P(ReportServiceTest, IT_RPT_02_TrainWiseCounts) {
    seedTen();
    auto rows = reports_->trainWiseCounts(ops_, range(0, 30));
    ASSERT_TRUE(rows.ok());
    ASSERT_EQ(rows.value().size(), 4u);   // every train, even without bookings
    std::map<std::string, TrainWiseRow> by;
    for (const TrainWiseRow& r : rows.value()) by[r.trainNumber] = r;
    EXPECT_EQ(by["20001"].reservations, 4);
    EXPECT_EQ(by["20001"].travellers, 4);         // 2 + 1 + 1, cancelled one excluded
    EXPECT_EQ(by["20001"].sleeperReservations, 3);
    EXPECT_EQ(by["20001"].ac3Reservations, 1);
    EXPECT_EQ(by["20001"].cancelled, 1);
    EXPECT_EQ(by["20001"].cancellationBasisPoints, 2500);
    EXPECT_EQ(by["20003"].reservations, 4);
    EXPECT_EQ(by["20004"].cancellationBasisPoints, 10000);
    int total = 0;
    for (const TrainWiseRow& r : rows.value()) total += r.reservations;
    EXPECT_EQ(total, reports_->bookingStatistics(ops_, range(0, 30)).value().total);
    EXPECT_NE(formatTrainWiseCounts(rows.value(), range(0, 30)).find("25.00 %"), std::string::npos);
}

// RBAC: reports are for ADMIN and OPS only (SAD §3.9.3 VIEW_REPORTS).
TEST_P(ReportServiceTest, OnlyAdminAndOpsSeeReports) {
    EXPECT_TRUE(reports_->bookingStatistics(test::sessionFor(seed::kA1, Role::Admin, clock_), range(0, 1)).ok());
    for (Role role : {Role::Passenger, Role::Staff}) {
        Session s = test::sessionFor(seed::kP1, role, clock_);
        EXPECT_EQ(reports_->bookingStatistics(s, range(0, 1)).code(), ErrorCode::Forbidden);
        EXPECT_EQ(reports_->seatOccupancy(s, "20003", day(1)).code(), ErrorCode::Forbidden);
        EXPECT_EQ(reports_->trainWiseCounts(s, range(0, 1)).code(), ErrorCode::Forbidden);
    }
    clock_.advanceSeconds(16 * 60);
    EXPECT_EQ(reports_->bookingStatistics(ops_, range(0, 1)).code(), ErrorCode::SessionExpired);
}

TEST_P(ReportServiceTest, InvalidRangesAreRejected) {
    EXPECT_EQ(reports_->bookingStatistics(ops_, range(5, 4)).code(), ErrorCode::Validation);
    EXPECT_EQ(reports_->trainWiseCounts(ops_, range(0, 400)).code(), ErrorCode::Validation);
}

INSTANTIATE_TEST_SUITE_P(Backends, ReportServiceTest, ::testing::ValuesIn(test::backendKinds()),
                         [](const auto& info) { return info.param; });

// AC-025: the figures equal direct SQL queries on rrs.db.
TEST(ReportServiceSql, ReportsEqualDirectDatabaseQueries) {
    auto store = SqliteDataStore::open(":memory:");
    ASSERT_TRUE(store.ok());
    DataAccess data = store.value()->access();
    ASSERT_TRUE(seed::loadSeedData(data, test::testToday(), 30).ok());
    ReservationSeeder seeder(data.reservations);
    const Date d = test::testToday();
    for (int i = 0; i < 25; ++i) {
        const TicketStatus st = i % 5 == 0 ? TicketStatus::Cancelled
                                           : i % 7 == 0 ? TicketStatus::Waitlisted : TicketStatus::Confirmed;
        seeder.add(i % 2 ? "20001" : "20004", d.plusDays(1 + i % 3), i % 3 ? ClassCode::SL : ClassCode::AC3, st,
                   1 + i % 3, 10000 + i * 100, st == TicketStatus::Cancelled ? 5000 : -1);
    }
    FixedClock clock(test::testToday());
    ReportService reports(data, clock);
    Session ops = test::sessionFor(seed::kO1, Role::Ops, clock);
    auto stats = reports.bookingStatistics(ops, DateRange{d, d.plusDays(10)}).value();

    Statement q(store.value()->database(),
                "SELECT status, COUNT(*) FROM reservation WHERE journey_date BETWEEN ? AND ? GROUP BY status");
    q.bind(1, d.iso()).bind(2, d.plusDays(10).iso());
    std::map<std::string, int> sql;
    int sqlTotal = 0;
    while (q.step()) {
        sql[q.text(0)] = static_cast<int>(q.integer(1));
        sqlTotal += static_cast<int>(q.integer(1));
    }
    EXPECT_EQ(stats.total, sqlTotal);
    EXPECT_EQ(stats.confirmed, sql["CONFIRMED"]);
    EXPECT_EQ(stats.waitlisted, sql["WAITLISTED"]);
    EXPECT_EQ(stats.cancelled, sql["CANCELLED"]);

    Statement fare(store.value()->database(),
                   "SELECT COALESCE(SUM(CASE WHEN status <> 'CANCELLED' THEN fare_paise END), 0), "
                   "COALESCE(SUM(refund_paise), 0) FROM reservation WHERE journey_date BETWEEN ? AND ?");
    fare.bind(1, d.iso()).bind(2, d.plusDays(10).iso());
    ASSERT_TRUE(fare.step());
    EXPECT_EQ(stats.totalFare.paise(), fare.integer(0));
    EXPECT_EQ(stats.totalRefunds.paise(), fare.integer(1));

    auto occ = reports.seatOccupancy(ops, "20001", d.plusDays(2)).value();
    for (const OccupancyRow& row : occ) {
        Statement seats(store.value()->database(),
                        "SELECT COUNT(*) FROM traveller t JOIN reservation r ON r.pnr = t.pnr "
                        "WHERE r.train_number = ? AND r.journey_date = ? AND r.class_code = ? AND r.status = 'CONFIRMED'");
        seats.bind(1, std::string("20001")).bind(2, d.plusDays(2).iso()).bind(3, classCodeString(row.classCode));
        ASSERT_TRUE(seats.step());
        EXPECT_EQ(row.confirmedSeats, seats.integer(0));
    }
}
