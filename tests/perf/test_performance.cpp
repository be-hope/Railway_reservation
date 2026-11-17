// test_performance.cpp - PERF-01 and PERF-02 (NFR-001, issue #7) on the
// large database (200 trains, 40 stations, 20,000 reservations) stored in a
// real SQLite file. Latencies are printed as P50 / P95 / max.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <random>

#include "booking/booking_service.h"
#include "auth/password_hasher.h"
#include "dal/sqlite_repositories.h"
#include "perf/large_data.h"
#include "seed_data.h"
#include "support/booking_helpers.h"
#include "support/test_support.h"

using namespace rrs;
using Clock = std::chrono::steady_clock;

namespace {

constexpr double kTargetMs = 2000.0;   // NFR-001: 2.0 s

double elapsedMs(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void report(const std::string& name, const std::vector<double>& ms) {
    std::printf("[%s] n=%zu  P50=%.2f ms  P95=%.2f ms  max=%.2f ms\n", name.c_str(), ms.size(),
                perf::percentileMs(ms, 50), perf::percentileMs(ms, 95), perf::percentileMs(ms, 100));
    ::testing::Test::RecordProperty(name + "_p95_ms", std::to_string(perf::percentileMs(ms, 95)));
}

class PerformanceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        auto token = PasswordHasher::randomToken();
        ASSERT_TRUE(token.ok());
        path_ = ::testing::TempDir() + "rrs_perf_" + token.value() + ".db";
        std::remove(path_.c_str());
        auto store = SqliteDataStore::open(path_);
        ASSERT_TRUE(store.ok());
        store_ = store.value().release();
        DataAccess data = store_->access();
        const auto start = Clock::now();
        auto generated = perf::generateLargeData(data, test::testToday());
        ASSERT_TRUE(generated.ok()) << formatError(generated.error());
        large_ = new perf::LargeData(generated.value());
        std::printf("[setup] generated %zu trains, %zu stations, %d reservations in %.0f ms\n",
                    large_->trains.size(), large_->stationCodes.size(), large_->reservationsInserted,
                    elapsedMs(start));
    }
    static void TearDownTestSuite() {
        delete large_;
        delete store_;
        std::remove(path_.c_str());
    }

    static std::string path_;
    static SqliteDataStore* store_;
    static perf::LargeData* large_;
};

std::string PerformanceTest::path_;
SqliteDataStore* PerformanceTest::store_ = nullptr;
perf::LargeData* PerformanceTest::large_ = nullptr;

}  // namespace

TEST_F(PerformanceTest, LargeDatasetHasTheTestPlanSize) {
    EXPECT_EQ(large_->trains.size(), 200u);
    EXPECT_EQ(large_->stationCodes.size(), 40u);
    EXPECT_EQ(large_->reservationsInserted, 20000);
    Statement count(store_->database(), "SELECT COUNT(*) FROM reservation");
    ASSERT_TRUE(count.step());
    EXPECT_EQ(count.integer(0), 20000);
}

// PERF-01: 100 sequential searches with varied routes and dates; at least
// 95 finish within 2.0 s.
TEST_F(PerformanceTest, PERF_01_SearchResponseTime) {
    DataAccess data = store_->access();
    std::mt19937 rng(7);
    std::vector<double> ms;
    int withResults = 0;
    for (int i = 0; i < 100; ++i) {
        const Train& t = large_->trains[rng() % large_->trains.size()];
        const std::size_t a = rng() % (t.stops.size() - 1);
        const std::size_t b = a + 1 + rng() % (t.stops.size() - a - 1);
        const Date date = test::testToday().plusDays(static_cast<long>(rng() % 30));
        const auto start = Clock::now();
        auto found = data.trains.findTrains(t.stops[a].stationCode, t.stops[b].stationCode, date);
        ms.push_back(elapsedMs(start));
        ASSERT_TRUE(found.ok());
        if (!found.value().empty()) ++withResults;
    }
    report("PERF-01 search", ms);
    int within = 0;
    for (double v : ms) within += v <= kTargetMs ? 1 : 0;
    EXPECT_GE(within, 95);
    EXPECT_EQ(withResults, 100);   // every query names a real leg of a running train
}

// PERF-02: 50 availability queries and 50 bookings; P95 <= 2.0 s each.
TEST_F(PerformanceTest, PERF_02_AvailabilityAndBookingLatency) {
    DataAccess data = store_->access();
    FixedClock clock(test::testToday());
    NullAuditLogger audit;
    PnrGenerator pnr;
    BookingService booking(data, clock, audit, pnr);
    std::mt19937 rng(11);

    std::vector<double> availabilityMs, bookingMs;
    for (int i = 0; i < 50; ++i) {
        const Train& t = large_->trains[rng() % large_->trains.size()];
        const Date date = test::testToday().plusDays(static_cast<long>(rng() % 30));
        const auto start = Clock::now();
        auto a = booking.checkAvailability(t.number, date, i % 2 ? ClassCode::SL : ClassCode::AC3);
        availabilityMs.push_back(elapsedMs(start));
        ASSERT_TRUE(a.ok()) << formatError(a.error());
    }
    for (int i = 0; i < 50; ++i) {
        const Train& t = large_->trains[rng() % large_->trains.size()];
        BookingRequest r = test::bookingRequest(
            booking, "perf-book-" + std::to_string(i), t.number, test::testToday().plusDays(static_cast<long>(rng() % 30)),
            t.stops.front().stationCode, t.stops.back().stationCode, ClassCode::SL,
            {test::traveller("Perf Passenger")});
        const auto start = Clock::now();
        auto booked = booking.createBooking(test::sessionFor(seed::kP1, Role::Passenger, clock), r);
        bookingMs.push_back(elapsedMs(start));
        ASSERT_TRUE(booked.ok()) << formatError(booked.error());
    }
    report("PERF-02 availability", availabilityMs);
    report("PERF-02 booking", bookingMs);
    EXPECT_LE(perf::percentileMs(availabilityMs, 95), kTargetMs);
    EXPECT_LE(perf::percentileMs(bookingMs, 95), kTargetMs);
}

// Repeated searches of the same leg stay fast (no degradation over time).
TEST_F(PerformanceTest, RepeatedSearchesDoNotDegrade) {
    DataAccess data = store_->access();
    const Train& t = large_->trains.front();
    std::vector<double> first, last;
    for (int i = 0; i < 500; ++i) {
        const auto start = Clock::now();
        ASSERT_TRUE(data.trains.findTrains(t.stops[0].stationCode, t.stops[1].stationCode, test::testToday()).ok());
        (i < 50 ? first : last).push_back(elapsedMs(start));
    }
    report("repeated search (first 50)", first);
    report("repeated search (last 450)", last);
    EXPECT_LE(perf::percentileMs(last, 95), kTargetMs);
}
