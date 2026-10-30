// test_reliability.cpp - REL-01 soak and REL-02 retry tests (NFR-002,
// NFR-004, issue #7): long runs and retries never create duplicate
// reservations or double-allocated seats.

#include <gtest/gtest.h>

#include <cstdio>
#include <map>
#include <random>
#include <set>

#include "booking/booking_service.h"
#include "cancel/seat_release_service.h"
#include "dal/sqlite_repositories.h"
#include "perf/large_data.h"
#include "seed_data.h"
#include "support/booking_helpers.h"
#include "support/test_support.h"
#include "support/trip_invariants.h"

using namespace rrs;

namespace {

std::string tempDb(const std::string& name) {
    std::string path = ::testing::TempDir() + name;
    std::remove(path.c_str());
    return path;
}

// Database-wide duplicate checks run with plain SQL.
void expectNoDuplicates(SqliteDatabase& db) {
    Statement seats(db,
                    "SELECT COUNT(*) FROM (SELECT 1 FROM traveller WHERE seat_number IS NOT NULL "
                    "GROUP BY train_number, journey_date, class_code, seat_number HAVING COUNT(*) > 1)");
    ASSERT_TRUE(seats.step());
    EXPECT_EQ(seats.integer(0), 0) << "a seat is held by two travellers";
    Statement tokens(db, "SELECT COUNT(*) - COUNT(DISTINCT request_id) FROM reservation");
    ASSERT_TRUE(tokens.step());
    EXPECT_EQ(tokens.integer(0), 0) << "a request token was stored twice";
}

int countReservations(SqliteDatabase& db) {
    Statement st(db, "SELECT COUNT(*) FROM reservation");
    return st.step() ? static_cast<int>(st.integer(0)) : -1;
}

bool isExpectedBusinessError(ErrorCode code) {
    switch (code) {
        case ErrorCode::NoCapacity:
        case ErrorCode::AlreadyCancelled:
        case ErrorCode::PnrNotFound:
            return true;
        default:
            return false;
    }
}

}  // namespace

// REL-01: 1,000 mixed operations on the large database; no crash and at
// least 99 % complete normally (expected business errors excluded).
TEST(Reliability, REL_01_SoakOfThousandMixedOperations) {
    const std::string path = tempDb("rrs_soak.db");
    auto store = SqliteDataStore::open(path);
    ASSERT_TRUE(store.ok());
    DataAccess data = store.value()->access();
    perf::LargeDataSpec spec;
    spec.trains = 50;          // smaller fleet so trips fill up and the waitlist is exercised
    spec.reservations = 5000;
    auto large = perf::generateLargeData(data, test::testToday(), spec);
    ASSERT_TRUE(large.ok());

    FixedClock clock(test::testToday());
    test::RecordingAuditLogger audit;
    PnrGenerator pnr;
    BookingService booking(data, clock, audit, pnr);
    SeatReleaseService release(data, clock, audit);
    const Session p1 = test::sessionFor(seed::kP1, Role::Passenger, clock);

    // Concentrate bookings on a few small trips so seats run out and the
    // RAC/waitlist paths run. The soak only uses hotDate (+40 days), which
    // has no generated bookings, so shrinking these classes is safe.
    std::vector<const Train*> hot;
    for (int i = 0; i < 5; ++i) hot.push_back(&large.value().trains[static_cast<std::size_t>(i)]);
    for (const Train* t : hot) {
        ClassConfig small = *data.config.findClassConfig(t->number, ClassCode::SL).value();
        small.coaches = 1;
        small.seatsPerCoach = 6;
        small.racQuota = 2;
        small.waitlistCapacity = 4;
        ASSERT_TRUE(data.config.saveClassConfig(small).ok());
    }
    const Date hotDate = test::testToday().plusDays(40);
    for (const Train* t : hot) ASSERT_TRUE(data.trains.addSchedule(t->number, hotDate).ok());

    std::mt19937 rng(42);
    std::vector<std::string> pnrs;
    std::map<std::string, int> outcome;
    int normal = 0, abnormal = 0;
    const int kOperations = 1000;
    for (int op = 0; op < kOperations; ++op) {
        const int kind = static_cast<int>(rng() % 100);
        const Train& t = *hot[rng() % hot.size()];
        Status result;
        if (kind < 30) {
            const Train& any = large.value().trains[rng() % large.value().trains.size()];
            auto r = data.trains.findTrains(any.stops.front().stationCode, any.stops.back().stationCode,
                                            test::testToday().plusDays(static_cast<long>(rng() % 30)));
            result = r.ok() ? Status::success() : Status(r.error());
            outcome["search"]++;
        } else if (kind < 45) {
            auto r = booking.checkAvailability(t.number, hotDate, ClassCode::SL);
            result = r.ok() ? Status::success() : Status(r.error());
            outcome["availability"]++;
        } else if (kind < 80) {
            std::vector<Traveller> list(1 + rng() % 2, test::traveller("Soak"));
            BookingRequest req = test::bookingRequest(booking, "soak-" + std::to_string(op), t.number, hotDate,
                                                      t.stops.front().stationCode, t.stops.back().stationCode,
                                                      ClassCode::SL, list);
            auto r = booking.createBooking(p1, req);
            if (r.ok()) pnrs.push_back(r.value().pnr);
            result = r.ok() ? Status::success() : Status(r.error());
            outcome["booking"]++;
        } else if (kind < 90) {
            if (pnrs.empty()) continue;
            auto r = booking.getTicket(p1, pnrs[rng() % pnrs.size()]);
            result = r.ok() ? Status::success() : Status(r.error());
            outcome["ticket"]++;
        } else {
            if (pnrs.empty()) continue;
            auto r = release.cancelAndRelease(pnrs[rng() % pnrs.size()], std::nullopt, seed::kP1);
            result = r.ok() ? Status::success() : Status(r.error());
            outcome["cancel"]++;
        }
        if (result.ok() || isExpectedBusinessError(result.code())) {
            ++normal;
        } else {
            ++abnormal;
            ADD_FAILURE() << "operation " << op << ": " << formatError(result.error());
        }
    }
    const int total = normal + abnormal;
    std::printf("[REL-01] operations=%d normal=%d abnormal=%d (search %d, availability %d, booking %d, "
                "ticket %d, cancel %d)\n",
                total, normal, abnormal, outcome["search"], outcome["availability"], outcome["booking"],
                outcome["ticket"], outcome["cancel"]);
    EXPECT_GE(total, 950);
    EXPECT_GE(normal * 100, total * 99);

    for (const Train* t : hot) {
        ClassConfig c = *data.config.findClassConfig(t->number, ClassCode::SL).value();
        EXPECT_EQ(test::checkTripInvariants(data.reservations, c, hotDate), "") << t->number;
    }
    expectNoDuplicates(store.value()->database());
    EXPECT_TRUE(store.value()->database().integrityCheck().ok());
    store.value().reset();
    std::remove(path.c_str());
}

// REL-02: the confirmation is lost after COMMIT and the process restarts; the
// retry with the same token returns the original PNR and the count stays 1.
TEST(Reliability, REL_02_RetryAfterRestartReturnsTheOriginalPnr) {
    const std::string path = tempDb("rrs_rel02.db");
    FixedClock clock(test::testToday());
    NullAuditLogger audit;
    BookingRequest request;
    std::string firstPnr;
    {
        auto store = SqliteDataStore::open(path);
        ASSERT_TRUE(store.ok());
        DataAccess data = store.value()->access();
        ASSERT_TRUE(seed::loadSeedData(data, test::testToday(), 10).ok());
        PnrGenerator pnr;
        BookingService booking(data, clock, audit, pnr);
        request = test::bookingRequest(booking, "rel02-token", "20003", test::testToday().plusDays(2), "SBC", "MYS",
                                       ClassCode::SL, {test::traveller("Asha Rao")});
        auto first = booking.createBooking(test::sessionFor(seed::kP1, Role::Passenger, clock), request);
        ASSERT_TRUE(first.ok());
        firstPnr = first.value().pnr;   // the client never sees this (lost confirmation)
    }   // process "crashes": connection closed
    {
        auto store = SqliteDataStore::open(path);
        ASSERT_TRUE(store.ok());
        DataAccess data = store.value()->access();
        PnrGenerator pnr;
        BookingService booking(data, clock, audit, pnr);
        for (int retry = 0; retry < 3; ++retry) {
            auto again = booking.createBooking(test::sessionFor(seed::kP1, Role::Passenger, clock), request);
            ASSERT_TRUE(again.ok());
            EXPECT_EQ(again.value().pnr, firstPnr);
        }
        EXPECT_EQ(countReservations(store.value()->database()), 1);
        EXPECT_EQ(booking.checkAvailability("20003", test::testToday().plusDays(2), ClassCode::SL).value().confirmedFree,
                  1);
    }
    std::remove(path.c_str());
}

// Faults before COMMIT: every failed attempt is rolled back, and the
// eventual successful retry creates exactly one reservation.
TEST(Reliability, RetriesAfterStorageFaultsCreateExactlyOneReservation) {
    const std::string path = tempDb("rrs_faults.db");
    auto store = SqliteDataStore::open(path);
    ASSERT_TRUE(store.ok());
    DataAccess raw = store.value()->access();
    ASSERT_TRUE(seed::loadSeedData(raw, test::testToday(), 10).ok());
    test::FaultInjectingReservationRepository faulty(raw.reservations);
    DataAccess data{raw.trains, raw.config, faulty, raw.users, raw.unitOfWork};
    FixedClock clock(test::testToday());
    NullAuditLogger audit;
    PnrGenerator pnr;
    BookingService booking(data, clock, audit, pnr);
    const Session p1 = test::sessionFor(seed::kP1, Role::Passenger, clock);

    BookingRequest request = test::bookingRequest(booking, "fault-token", "20001", test::testToday().plusDays(3),
                                                  "SBC", "MAS", ClassCode::AC3,
                                                  {test::traveller("Asha Rao"), test::traveller("Ravi Rao")});
    faulty.failNextInserts = 3;
    int attempts = 0;
    Result<Reservation> result = Result<Reservation>::failure(ErrorCode::Database);
    while (!result.ok() && attempts < 10) {
        result = booking.createBooking(p1, request);
        ++attempts;
    }
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(attempts, 4);
    EXPECT_EQ(countReservations(store.value()->database()), 1);
    EXPECT_EQ(result.value().travellers[0].seatNumber, "B1-01");   // no seat leaked by the failed attempts

    // A burst of duplicate submissions after success changes nothing.
    for (int i = 0; i < 20; ++i) {
        auto again = booking.createBooking(p1, request);
        ASSERT_TRUE(again.ok());
        EXPECT_EQ(again.value().pnr, result.value().pnr);
    }
    EXPECT_EQ(countReservations(store.value()->database()), 1);
    expectNoDuplicates(store.value()->database());
    store.value().reset();
    std::remove(path.c_str());
}

// Long-running operation: many bookings in one run keep PNRs unique and the
// seat map consistent until the train is full.
TEST(Reliability, FillingATrainLeavesNoDuplicatesAndAContiguousWaitlist) {
    auto store = SqliteDataStore::open(":memory:");
    ASSERT_TRUE(store.ok());
    DataAccess data = store.value()->access();
    ASSERT_TRUE(seed::loadSeedData(data, test::testToday(), 10).ok());
    FixedClock clock(test::testToday());
    NullAuditLogger audit;
    PnrGenerator pnr;
    BookingService booking(data, clock, audit, pnr);
    const Date date = test::testToday().plusDays(1);
    std::set<std::string> seen;
    int confirmed = 0, waitlisted = 0, rejected = 0;
    for (int i = 0; i < 320; ++i) {
        BookingRequest r = test::bookingRequest(booking, "fill-" + std::to_string(i), "20001", date, "SBC", "MAS",
                                                ClassCode::SL, {test::traveller("Filler")});
        auto b = booking.createBooking(test::sessionFor(i % 2 ? seed::kP1 : seed::kP2, Role::Passenger, clock), r);
        if (!b.ok()) {
            ASSERT_EQ(b.code(), ErrorCode::NoCapacity);
            ++rejected;
            continue;
        }
        EXPECT_TRUE(seen.insert(b.value().pnr).second);
        if (b.value().status == TicketStatus::Confirmed) ++confirmed;
        if (b.value().status == TicketStatus::Waitlisted) ++waitlisted;
    }
    EXPECT_EQ(confirmed, 288);
    EXPECT_EQ(waitlisted, 20);
    EXPECT_EQ(rejected, 12);
    ClassConfig c = *data.config.findClassConfig("20001", ClassCode::SL).value();
    EXPECT_EQ(test::checkTripInvariants(data.reservations, c, date), "");
    expectNoDuplicates(store.value()->database());
}
