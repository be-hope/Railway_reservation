// test_booking_validator.cpp - UT-BKG-02 traveller-list validation (FR-010,
// BR-05) and hostile input (SEC-003).

#include <gtest/gtest.h>

#include "booking/booking_request_validator.h"

using namespace rrs;

namespace {

const Date kToday = *Date::fromYmd(2026, 11, 2);

Traveller valid() {
    Traveller t;
    t.name = "Asha Rao";
    t.age = 28;
    t.gender = 'F';
    t.idProofNumber = "DL-KA012026001";
    return t;
}

BookingRequest request(int travellers) {
    BookingRequest r;
    r.requestId = "req-1";
    r.trainNumber = "20003";
    r.journeyDate = kToday.plusDays(7);
    r.sourceCode = "SBC";
    r.destinationCode = "MYS";
    r.travellers.assign(static_cast<std::size_t>(travellers), valid());
    return r;
}

ErrorCode codeFor(const Traveller& t) { return validateTraveller(t, 1).code(); }

}  // namespace

// UT-BKG-02 step 1: lists of 1 and 6 accepted; 0 and 7 rejected.
TEST(BookingValidator, UT_BKG_02_TravellerCountBoundaries) {
    EXPECT_EQ(validateBookingRequest(request(0), kToday).code(), ErrorCode::Validation);
    EXPECT_TRUE(validateBookingRequest(request(1), kToday).ok());
    EXPECT_TRUE(validateBookingRequest(request(6), kToday).ok());
    EXPECT_EQ(validateBookingRequest(request(7), kToday).code(), ErrorCode::Validation);
}

// UT-BKG-02 step 2: age 0, gender X, empty name, empty ID proof.
TEST(BookingValidator, UT_BKG_02_InvalidTravellerFields) {
    Traveller t = valid();
    t.age = 0;
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
    t = valid();
    t.gender = 'X';
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
    t = valid();
    t.name = "";
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
    t = valid();
    t.idProofNumber = "";
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
}

TEST(BookingValidator, AgeBoundaryValues) {
    Traveller t = valid();
    for (int age : {1, 120}) {
        t.age = age;
        EXPECT_TRUE(validateTraveller(t, 1).ok()) << age;
    }
    for (int age : {0, 121, -5}) {
        t.age = age;
        EXPECT_FALSE(validateTraveller(t, 1).ok()) << age;
    }
}

TEST(BookingValidator, MessageNamesTheTravellerAndField) {
    BookingRequest r = request(3);
    r.travellers[2].age = 0;
    Status s = validateBookingRequest(r, kToday);
    ASSERT_FALSE(s.ok());
    EXPECT_NE(s.error().detail.find("Traveller 3"), std::string::npos);
    EXPECT_NE(s.error().detail.find("age"), std::string::npos);
}

TEST(BookingValidator, JourneyDateWindowIsTodayToTodayPlus120) {
    BookingRequest r = request(1);
    r.journeyDate = kToday;
    EXPECT_TRUE(validateBookingRequest(r, kToday).ok());
    r.journeyDate = kToday.plusDays(120);
    EXPECT_TRUE(validateBookingRequest(r, kToday).ok());
    r.journeyDate = kToday.plusDays(121);
    EXPECT_FALSE(validateBookingRequest(r, kToday).ok());
    r.journeyDate = kToday.plusDays(-1);
    EXPECT_FALSE(validateBookingRequest(r, kToday).ok());
}

TEST(BookingValidator, RejectsSameSourceAndDestination) {
    BookingRequest r = request(1);
    r.destinationCode = "SBC";
    EXPECT_FALSE(validateBookingRequest(r, kToday).ok());
}

TEST(BookingValidator, RejectsBadRequestIdTrainAndStation) {
    BookingRequest r = request(1);
    r.requestId = "";
    EXPECT_FALSE(validateBookingRequest(r, kToday).ok());
    r = request(1);
    r.requestId = std::string(37, 'a');
    EXPECT_FALSE(validateBookingRequest(r, kToday).ok());
    r = request(1);
    r.trainNumber = "20003; DROP TABLE";
    EXPECT_FALSE(validateBookingRequest(r, kToday).ok());
    r = request(1);
    r.sourceCode = "sbc";
    EXPECT_FALSE(validateBookingRequest(r, kToday).ok());
}

// SEC-003: 1,000-character, control-character and SQL-meta input end in a
// clean validation error.
TEST(BookingValidator, HostileInputIsRejectedCleanly) {
    Traveller t = valid();
    t.name = std::string(1000, 'A');
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
    t = valid();
    t.name = std::string("Asha\x01Rao");
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
    t = valid();
    t.idProofNumber = "' OR 1=1 --";
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
    t = valid();
    t.idProofNumber = "%s%n";
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
    t = valid();
    t.name = "   ";
    EXPECT_EQ(codeFor(t), ErrorCode::Validation);
}
