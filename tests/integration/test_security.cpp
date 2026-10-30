#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include "auth/auth_service.h"
#include "auth/profile_service.h"
#include "admin/reservation_admin_service.h"
#include "dal/retention.h"
#include "dal/sqlite_repositories.h"
#include "security/input_validator.h"
#include "seed_data.h"
#include "support/booking_helpers.h"
#include "support/test_support.h"
using namespace rrs;

namespace {
class SecurityTest : public ::testing::Test {
protected:
    FixedClock clock{test::testToday()};
    test::RecordingAuditLogger audit;
    std::unique_ptr<SqliteDataStore> store;
    std::unique_ptr<AuthService> auth;
    void SetUp() override {
        auto opened = SqliteDataStore::open(":memory:"); ASSERT_TRUE(opened.ok());
        store = std::move(opened.value());
        auto data = store->access(); ASSERT_TRUE(seed::loadSeedData(data, clock.today()).ok());
        auth = std::make_unique<AuthService>(data, clock, audit);
    }
    Registration registration(std::string mobile = "9123456789", std::string email = "asha@example.com") {
        return {mobile, email, "Test@1234", "Asha Rao"};
    }
    Session login() {
        EXPECT_TRUE(auth->registerPassenger(registration()).ok());
        auto s = auth->login("9123456789", "Test@1234"); EXPECT_TRUE(s.ok());
        return s.value();
    }
};
}
TEST_F(SecurityTest, SEC_T01_SaltedHashesAndNoCredentialDisclosure) {
    auto first = auth->registerPassenger(registration()); ASSERT_TRUE(first.ok());
    auto second = auth->registerPassenger(registration("9234567890", "second@example.com")); ASSERT_TRUE(second.ok());
    auto data = store->access();
    auto a = *data.users.findById(first.value()).value();
    auto b = *data.users.findById(second.value()).value();
    EXPECT_EQ(a.passwordHash.size(), 64u); EXPECT_EQ(a.salt.size(), 32u);
    EXPECT_NE(a.passwordHash, b.passwordHash); EXPECT_NE(a.salt, b.salt);
    EXPECT_TRUE(PasswordHasher::verify("Test@1234", a.passwordHash, a.salt));
    EXPECT_FALSE(PasswordHasher::verify("wrong", a.passwordHash, a.salt));
    EXPECT_FALSE(PasswordHasher::verify("Test@1234", "bad", "bad"));
    ProfileService profiles(data.users, clock, audit);
    auto session = auth->login("asha@example.com", "Test@1234"); ASSERT_TRUE(session.ok());
    auto profile = profiles.view(session.value()); ASSERT_TRUE(profile.ok());
    EXPECT_TRUE(profile->passwordHash.empty()); EXPECT_TRUE(profile->salt.empty());
    Statement dump(store->database(), "SELECT count(*) FROM account WHERE password_hash = ? OR salt = ?");
    dump.bind(1, "Test@1234").bind(2, "Test@1234"); ASSERT_TRUE(dump.step()); EXPECT_EQ(dump.integer(0), 0);
    for (const auto& e : audit.entries) { EXPECT_EQ(e.detail.find("Test@1234"), std::string::npos); }
}
TEST_F(SecurityTest, UT_AUTH_02_DuplicatesAndBothLoginAliases) {
    ASSERT_TRUE(auth->registerPassenger(registration()).ok());
    EXPECT_EQ(auth->registerPassenger(registration()).code(), ErrorCode::DuplicateUser);
    EXPECT_EQ(auth->registerPassenger(registration("9345678901", "ASHA@EXAMPLE.COM")).code(), ErrorCode::DuplicateUser);
    EXPECT_TRUE(auth->login("ASHA@EXAMPLE.COM", "Test@1234").ok());
    EXPECT_TRUE(auth->login("9123456789", "Test@1234").ok());
    EXPECT_EQ(auth->login("unknown@example.com", "wrong").code(), ErrorCode::AuthInvalid);
    EXPECT_EQ(auth->login("9123456789", "wrong").code(), ErrorCode::AuthInvalid);
}
TEST_F(SecurityTest, SEC_T04_ExpiredTamperedAndRevokedSessions) {
    auto s = login();
    auto changed = s; changed.role = Role::Admin;
    EXPECT_EQ(requireRole(changed, {Role::Admin}, clock).code(), ErrorCode::SessionExpired);
    changed = s; changed.accountId = seed::kP2;
    EXPECT_EQ(requireRole(changed, {Role::Passenger}, clock).code(), ErrorCode::SessionExpired);
    changed = s; changed.sessionId += "x";
    EXPECT_EQ(requireRole(changed, {Role::Passenger}, clock).code(), ErrorCode::SessionExpired);
    changed = s; changed.expiresAt += 86400;
    EXPECT_EQ(requireRole(changed, {Role::Passenger}, clock).code(), ErrorCode::SessionExpired);
    EXPECT_EQ(requireRole(Session{}, {Role::Passenger}, clock).code(), ErrorCode::SessionExpired);
    clock.advanceSeconds(899); EXPECT_TRUE(requireRole(s, {Role::Passenger}, clock).ok());
    clock.advanceSeconds(899); EXPECT_TRUE(requireRole(s, {Role::Passenger}, clock).ok());
    clock.advanceSeconds(900); EXPECT_EQ(requireRole(s, {Role::Passenger}, clock).code(), ErrorCode::SessionExpired);
    auto again = auth->login("9123456789", "Test@1234"); ASSERT_TRUE(again.ok());
    auto copy = again.value(); auth->logout(again.value());
    EXPECT_EQ(requireRole(copy, {Role::Passenger}, clock).code(), ErrorCode::SessionExpired);
}
TEST_F(SecurityTest, SEC_T06_FiveFailuresLockForExactlyFiveMinutes) {
    ASSERT_TRUE(auth->registerPassenger(registration()).ok());
    for (int i = 0; i < 4; ++i) EXPECT_EQ(auth->login("9123456789", "wrong").code(), ErrorCode::AuthInvalid);
    EXPECT_EQ(auth->login("9123456789", "wrong").code(), ErrorCode::AccountLocked);
    clock.advanceSeconds(299); EXPECT_EQ(auth->login("9123456789", "Test@1234").code(), ErrorCode::AccountLocked);
    clock.advanceSeconds(1); EXPECT_TRUE(auth->login("9123456789", "Test@1234").ok());
    auto account = store->access().users.findByLoginId("9123456789"); ASSERT_TRUE(account.ok());
    EXPECT_EQ(account.value()->failedAttempts, 0); EXPECT_EQ(account.value()->lockedUntil, 0);
    EXPECT_EQ(audit.count("ACCOUNT_LOCKED"), 1);
}
TEST_F(SecurityTest, SuccessfulLoginResetsFailureCountAndLockExpiryRestartsCounter) {
    ASSERT_TRUE(auth->registerPassenger(registration()).ok());
    for (int i = 0; i < 4; ++i) auth->login("9123456789", "wrong");
    ASSERT_TRUE(auth->login("9123456789", "Test@1234").ok());
    for (int i = 0; i < 4; ++i) EXPECT_EQ(auth->login("9123456789", "wrong").code(), ErrorCode::AuthInvalid);
    EXPECT_EQ(auth->login("9123456789", "wrong").code(), ErrorCode::AccountLocked);
    clock.advanceSeconds(300);
    EXPECT_EQ(auth->login("9123456789", "wrong").code(), ErrorCode::AuthInvalid);
}
TEST_F(SecurityTest, IT_AUTH_03_ProfileValidationOwnershipPersistenceAndMasking) {
    auto s = login(); auto data = store->access();
    ProfileService profiles(data.users, clock, audit);
    auto value = profiles.view(s); ASSERT_TRUE(value.ok());
    Account update = value.value(); update.age = 28; update.idProofType = "DL"; update.idProofNumber = "KA1234567890";
    update.role = Role::Admin; update.passwordHash = "attacker";
    ASSERT_TRUE(profiles.update(s, update).ok());
    auto stored = *data.users.findById(s.accountId).value();
    EXPECT_EQ(stored.role, Role::Passenger); EXPECT_NE(stored.passwordHash, "attacker");
    auto masked = profiles.view(s); ASSERT_TRUE(masked.ok());
    EXPECT_EQ(masked->idProofNumber, maskIdProof("KA1234567890"));
    update = masked.value(); update.name = "Updated Name";
    ASSERT_TRUE(profiles.update(s, update).ok());
    EXPECT_EQ(data.users.findById(s.accountId).value()->idProofNumber, "KA1234567890");
    update.contactNumber = "invalid"; EXPECT_EQ(profiles.update(s, update).code(), ErrorCode::Validation);
    update = masked.value(); update.accountId = seed::kP2;
    EXPECT_EQ(profiles.update(s, update).code(), ErrorCode::Forbidden);
    update = masked.value(); update.contactNumber = "9876500002";
    EXPECT_EQ(profiles.update(s, update).code(), ErrorCode::DuplicateUser);
}
TEST_F(SecurityTest, IT_BKG_07_SEC_T08_HistoryIsPrivateOrderedAndIdsMasked) {
    auto data = store->access(); PnrGenerator pnr; BookingService booking(data, clock, audit, pnr);
    auto p1 = test::sessionFor(seed::kP1, Role::Passenger, clock);
    auto p2 = test::sessionFor(seed::kP2, Role::Passenger, clock);
    auto request = [&](std::string id, int days) {
        return test::bookingRequest(booking, id, "20001", clock.today().plusDays(days), "SBC", "MAS", ClassCode::SL, {test::traveller("Asha")});
    };
    auto late = booking.createBooking(p1, request("late", 5)); ASSERT_TRUE(late.ok());
    auto early = booking.createBooking(p1, request("early", 2)); ASSERT_TRUE(early.ok());
    auto other = booking.createBooking(p2, request("other", 3)); ASSERT_TRUE(other.ok());
    BookingHistoryService history(data.reservations, clock, audit);
    auto rows = history.history(p1); ASSERT_TRUE(rows.ok()); ASSERT_EQ(rows->size(), 2u);
    EXPECT_EQ(rows.value()[0].pnr, early->pnr); EXPECT_EQ(rows.value()[1].pnr, late->pnr);
    EXPECT_EQ(rows.value()[0].travellers[0].idProofNumber, maskIdProof(test::traveller("Asha").idProofNumber));
    EXPECT_EQ(booking.getTicket(p1, other->pnr).code(), ErrorCode::Forbidden);
    EXPECT_EQ(booking.getTicket(p1, "' OR 1=1 --").code(), ErrorCode::Validation);
    EXPECT_EQ(history.history(test::sessionFor(seed::kS1, Role::Staff, clock)).code(), ErrorCode::Forbidden);
}
TEST_F(SecurityTest, IT_STF_01_02_StaffBookingsAndCorrectionsPreserveInvariants) {
    auto data = store->access(); PnrGenerator pnr; BookingService booking(data, clock, audit, pnr);
    ReservationAdminService admin(data, booking, clock, audit);
    auto staff = test::sessionFor(seed::kS1, Role::Staff, clock);
    auto passenger = test::sessionFor(seed::kP1, Role::Passenger, clock);
    auto request = test::bookingRequest(booking, "staff-book", "20001", clock.today().plusDays(2), "SBC", "MAS", ClassCode::SL, {test::traveller("Asha")});
    request.onBehalfOf = seed::kP2;
    EXPECT_EQ(admin.bookOnBehalf(passenger, request).code(), ErrorCode::Forbidden);
    auto booked = admin.bookOnBehalf(staff, request); ASSERT_TRUE(booked.ok());
    EXPECT_EQ(booked->accountId, seed::kP2); EXPECT_EQ(booked->bookedBy, seed::kS1);
    auto corrections = booked->travellers; corrections[0].name = "Corrected Name"; corrections[0].seatNumber = "forged";
    EXPECT_EQ(admin.correctReservation(passenger, booked->pnr, corrections).code(), ErrorCode::Forbidden);
    auto corrected = admin.correctReservation(staff, booked->pnr, corrections); ASSERT_TRUE(corrected.ok());
    EXPECT_EQ(corrected->travellers[0].name, "Corrected Name");
    EXPECT_EQ(corrected->travellers[0].seatNumber, booked->travellers[0].seatNumber);
    EXPECT_EQ(corrected->fare, booked->fare); EXPECT_EQ(corrected->accountId, booked->accountId);
    EXPECT_EQ(admin.correctReservation(staff, booked->pnr, {}).code(), ErrorCode::Validation);
    corrections[0].age = 0; EXPECT_EQ(admin.correctReservation(staff, booked->pnr, corrections).code(), ErrorCode::Validation);
    EXPECT_EQ(audit.count("RESERVATION_CORRECTED"), 1);
}
TEST_F(SecurityTest, SEC_T02_CompleteRolePermissionMatrix) {
    AccessControl access(clock, audit);
    const bool matrix[4][9] = {
        {true,true,true,true,true,false,false,false,false},
        {true,false,false,true,true,true,false,false,false},
        {false,false,false,true,true,true,true,true,true},
        {false,false,false,false,false,false,false,true,false}
    };
    const Role roles[] = {Role::Passenger, Role::Staff, Role::Admin, Role::Ops};
    for (int r = 0; r < 4; ++r) for (int p = 0; p < 9; ++p) {
        auto s = test::sessionFor("actor", roles[r], clock);
        EXPECT_EQ(access.require(s, static_cast<Permission>(p)).ok(), matrix[r][p]) << r << "," << p;
    }
    auto s = test::sessionFor("actor", Role::Passenger, clock);
    EXPECT_EQ(access.require(s, Permission::ViewPnr, "another").code(), ErrorCode::Forbidden);
    EXPECT_EQ(access.require(s, static_cast<Permission>(999)).code(), ErrorCode::Forbidden);
}
TEST_F(SecurityTest, SEC_T07_RetentionBoundaryAndIdempotence) {
    auto data = store->access(); PnrGenerator pnr; BookingService booking(data, clock, audit, pnr);
    auto s = test::sessionFor(seed::kP1, Role::Passenger, clock);
    auto request = test::bookingRequest(booking, "retention", "20001", clock.today(), "SBC", "MAS", ClassCode::SL, {test::traveller("Asha")});
    auto booked = booking.createBooking(s, request); ASSERT_TRUE(booked.ok());
    auto at90 = clearExpiredTravellerIds(store->database(), clock.today().plusDays(90)); ASSERT_TRUE(at90.ok()); EXPECT_EQ(at90.value(), 0);
    auto at91 = clearExpiredTravellerIds(store->database(), clock.today().plusDays(91)); ASSERT_TRUE(at91.ok()); EXPECT_EQ(at91.value(), 1);
    EXPECT_TRUE(data.reservations.findByPnr(booked->pnr).value()->travellers[0].idProofNumber.empty());
    EXPECT_EQ(clearExpiredTravellerIds(store->database(), clock.today().plusDays(92)).value(), 0);
}
TEST(InputValidatorSecurity, SEC_T03_BoundariesAndHostileInputs) {
    EXPECT_TRUE(InputValidator::mobile("9876543210").ok()); EXPECT_FALSE(InputValidator::mobile("987654321").ok());
    EXPECT_TRUE(InputValidator::email("asha+rail@example.com").ok()); EXPECT_FALSE(InputValidator::email("asha@").ok());
    EXPECT_FALSE(InputValidator::password("abcdefgh").ok()); EXPECT_FALSE(InputValidator::password("12345678").ok());
    EXPECT_TRUE(InputValidator::password("abc12345").ok());
    EXPECT_TRUE(InputValidator::age(120).ok()); EXPECT_FALSE(InputValidator::age(121).ok());
    EXPECT_TRUE(InputValidator::station("SBC").ok()); EXPECT_FALSE(InputValidator::station("sbc").ok());
    EXPECT_TRUE(InputValidator::pnr("1234567890").ok()); EXPECT_FALSE(InputValidator::pnr("0234567890").ok());
    for (const std::string& value : {std::string(1000, 'A'), std::string("\n"), std::string("' OR 1=1 --"), std::string("%s%n")}) {
        EXPECT_FALSE(InputValidator::mobile(value).ok()); EXPECT_FALSE(InputValidator::email(value).ok());
        EXPECT_FALSE(InputValidator::idProof(value).ok()); EXPECT_FALSE(InputValidator::pnr(value).ok());
    }
    EXPECT_FALSE(InputValidator::name(std::string("bad\0name", 8)).ok());
    EXPECT_TRUE(InputValidator::name("O'Connor").ok());
}
TEST(AuditSecurity, SEC_T05_AppendSanitizeAndDetectTampering) {
    const auto path = ::testing::TempDir() + "rrs_security_audit.log";
    std::remove(path.c_str());
    FixedClock clock(test::testToday()); AppendOnlyFileAuditLogger log(path, clock);
    log.log("LOGIN_FAILED", "actor\nforged", "invalid credentials");
    log.log("UNAUTHORIZED_ATTEMPT", "actor", "view|pnr");
    EXPECT_TRUE(log.verify());
    std::ifstream input(path); std::string text((std::istreambuf_iterator<char>(input)), {}); input.close();
    EXPECT_NE(text.find("LOGIN_FAILED"), std::string::npos);
    EXPECT_EQ(text.find("actor\nforged"), std::string::npos);
    auto original = text; text[0] = '9';
    { std::ofstream output(path); output << text; }
    EXPECT_FALSE(log.verify()); EXPECT_THROW(log.log("EVENT", "actor", "detail"), std::runtime_error);
    { std::ofstream output(path); output << original; }
    EXPECT_TRUE(log.verify());
    std::remove(path.c_str());
}

TEST(PasswordSecurity, PBKDF2MatchesIndependentPythonHashlibVector) {
    EXPECT_TRUE(PasswordHasher::verify("Test@1234",
        "0b09637be6233fb257564afe5c7b270f11f5275afad6f824e0849bb9b0b99816",
        "000102030405060708090a0b0c0d0e0f"));
}
TEST(AuthPersistence, ProfileAndLockoutSurviveDatabaseReopen) {
    const auto path = ::testing::TempDir() + "rrs_auth_persistence.db";
    std::remove(path.c_str());
    FixedClock clock(test::testToday()); test::RecordingAuditLogger audit;
    std::string accountId;
    {
        auto opened = SqliteDataStore::open(path); ASSERT_TRUE(opened.ok());
        auto data = opened.value()->access(); AuthService auth(data, clock, audit);
        auto registered = auth.registerPassenger({"9123456789", "asha@example.com", "Test@1234", "Asha"}); ASSERT_TRUE(registered.ok());
        accountId = registered.value();
        auto session = auth.login("9123456789", "Test@1234"); ASSERT_TRUE(session.ok());
        ProfileService profiles(data.users, clock, audit);
        auto value = profiles.view(session.value()); ASSERT_TRUE(value.ok());
        value->name = "Persisted Name"; value->age = 30; value->idProofType = "DL"; value->idProofNumber = "DL123456";
        ASSERT_TRUE(profiles.update(session.value(), value.value()).ok());
        for (int i = 0; i < 5; ++i) auth.login("9123456789", "wrong");
    }
    {
        auto opened = SqliteDataStore::open(path); ASSERT_TRUE(opened.ok());
        auto data = opened.value()->access(); AuthService auth(data, clock, audit);
        EXPECT_EQ(auth.login("9123456789", "Test@1234").code(), ErrorCode::AccountLocked);
        clock.advanceSeconds(300);
        auto session = auth.login("9123456789", "Test@1234"); ASSERT_TRUE(session.ok());
        EXPECT_EQ(session->accountId, accountId);
        ProfileService profiles(data.users, clock, audit);
        auto value = profiles.view(session.value()); ASSERT_TRUE(value.ok());
        EXPECT_EQ(value->name, "Persisted Name"); EXPECT_EQ(value->idProofNumber, maskIdProof("DL123456"));
    }
    std::remove(path.c_str());
}
TEST_F(SecurityTest, StaffCorrectionRejectsDepartedAndCancelledTickets) {
    auto data = store->access(); PnrGenerator pnr; BookingService booking(data, clock, audit, pnr);
    ReservationAdminService admin(data, booking, clock, audit);
    auto staff = test::sessionFor(seed::kS1, Role::Staff, clock);
    auto passenger = test::sessionFor(seed::kP1, Role::Passenger, clock);
    auto request = test::bookingRequest(booking, "departed", "20001", clock.today(), "SBC", "MAS", ClassCode::SL, {test::traveller("Asha")});
    auto booked = booking.createBooking(passenger, request); ASSERT_TRUE(booked.ok());
    EXPECT_EQ(admin.correctReservation(staff, booked->pnr, booked->travellers).code(), ErrorCode::CancelDeparted);
    Reservation cancelled = booked.value(); cancelled.status = TicketStatus::Cancelled;
    for (auto& t : cancelled.travellers) t.seatNumber.clear();
    ASSERT_TRUE(data.reservations.update(cancelled).ok());
    EXPECT_EQ(admin.correctReservation(staff, booked->pnr, booked->travellers).code(), ErrorCode::AlreadyCancelled);
    EXPECT_EQ(audit.count("RESERVATION_CORRECTED"), 0);
}
