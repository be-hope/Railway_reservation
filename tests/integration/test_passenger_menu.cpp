// test_passenger_menu.cpp - passenger console menus (issue #39): scripted
// stdin/stdout runs for ST-BKG-01 / USB-01 (booking in at most six primary
// steps), fare rejection, field-level re-prompts and booking history.

#include <gtest/gtest.h>

#include <sstream>

#include "demo_adapters.h"
#include "seed_data.h"
#include "support/test_support.h"
#include "ui/passenger_menu.h"

using namespace rrs;

namespace {

class PassengerMenuTest : public ::testing::Test {
protected:
    PassengerMenuTest() : clock_(test::testToday()) {}

    void SetUp() override {
        backend_ = test::makeBackend("sqlite");
        data_ = std::make_unique<DataAccess>(backend_->access());
        ASSERT_TRUE(seed::loadSeedData(*data_, test::testToday()).ok());
        booking_ = std::make_unique<BookingService>(*data_, clock_, audit_, pnr_);
        trains_ = std::make_unique<demo::RepositoryTrainSearch>(data_->trains);
        history_ = std::make_unique<demo::RepositoryBookingHistory>(data_->reservations, clock_);
        profile_ = std::make_unique<demo::RepositoryProfile>(data_->users, clock_);
        release_ = std::make_unique<SeatReleaseService>(*data_, clock_, audit_);
        cancellation_ = std::make_unique<demo::InterimCancellation>(*booking_, *release_);
    }

    // Runs the menu as P1 with the given keyboard input; returns the output.
    std::string run(const std::string& input, std::vector<std::string>* screens = nullptr,
                    bool withCancellation = false) {
        std::istringstream in(input);
        std::ostringstream out;
        ConsoleIO io(in, out);
        int n = 0;
        PassengerMenu menu(io, PassengerPorts{*booking_, *trains_, *history_, *profile_,
                                          withCancellation ? cancellation_.get() : nullptr},
                           test::sessionFor(seed::kP1, Role::Passenger, clock_), clock_,
                           [&n]() { return "menu-req-" + std::to_string(++n); });
        menu.run();
        if (screens) *screens = menu.screens();
        return out.str();
    }

    std::string journeyDate(int days = 7) { return test::testToday().plusDays(days).display(); }
    int reservations() { return static_cast<int>(data_->reservations.listForAccount(seed::kP1).value().size()); }

    FixedClock clock_;
    test::RecordingAuditLogger audit_;
    PnrGenerator pnr_;
    std::unique_ptr<test::Backend> backend_;
    std::unique_ptr<DataAccess> data_;
    std::unique_ptr<BookingService> booking_;
    std::unique_ptr<demo::RepositoryTrainSearch> trains_;
    std::unique_ptr<demo::RepositoryBookingHistory> history_;
    std::unique_ptr<demo::RepositoryProfile> profile_;
    std::unique_ptr<SeatReleaseService> release_;
    std::unique_ptr<demo::InterimCancellation> cancellation_;
};

}  // namespace

// ST-BKG-01 / USB-01 / NFR-003: Login -> Search -> Select train and class ->
// Enter travellers -> Confirm fare -> View ticket = 6 primary steps.
TEST_F(PassengerMenuTest, ST_BKG_01_BookingTakesSixPrimarySteps) {
    const std::string input =
        "1\n"                                   // main menu: Search trains
        "SBC\nMYS\n" + journeyDate() + "\n"     // search criteria
        "1\n"                                   // select result 1 (20003)
        "SL\n"                                  // class
        "1\nAsha Rao\n28\nF\nDL-KA012026001\n"  // one traveller
        "Y\n"                                   // accept fare
        "0\n";                                  // logout
    std::vector<std::string> screens;
    const std::string out = run(input, &screens);

    EXPECT_EQ(screens, (std::vector<std::string>{"Search", "Select train and class", "Enter travellers",
                                                 "Confirm fare", "View ticket"}));
    EXPECT_LE(screens.size() + 1, 6u) << "login + menu screens must be <= 6 (NFR-003)";
    EXPECT_NE(out.find("20003"), std::string::npos);
    EXPECT_NE(out.find("Total fare    : Rs. 140.00"), std::string::npos);
    EXPECT_NE(out.find("TICKET   PNR "), std::string::npos);
    EXPECT_NE(out.find("Seat S1-01"), std::string::npos);
    EXPECT_NE(out.find("Status  : Confirmed"), std::string::npos);
    EXPECT_EQ(out.find("DL-KA012026001"), std::string::npos) << "ID proof must be masked";
    EXPECT_NE(out.find("**********6001"), std::string::npos);
    EXPECT_EQ(reservations(), 1);
}

TEST_F(PassengerMenuTest, RejectingTheFareCreatesNoBooking) {
    const std::string input = "1\nSBC\nMYS\n" + journeyDate() + "\n1\nSL\n1\nAsha Rao\n28\nF\nDL-1234\nN\n0\n";
    const std::string out = run(input);
    EXPECT_NE(out.find("Fare not accepted. No booking was made."), std::string::npos);
    EXPECT_EQ(reservations(), 0);
}

TEST_F(PassengerMenuTest, InvalidFieldsAreReportedAndAskedAgain) {
    const std::string input =
        "4\n20003\n" + journeyDate() + "\n"
        "SB1\nSBC\nMYS\n"                   // bad station code, then valid
        "XX\nSL\n"                          // bad class, then valid
        "9\n1\n"                            // 9 travellers rejected, then 1
        "Asha Rao\n0\n28\nX\nDL-1234\n"     // age 0 re-asked; gender X fails validation
        "Asha Rao\n28\nF\nDL-1234\n"        // traveller re-entered
        "Y\n0\n";
    const std::string out = run(input);
    EXPECT_NE(out.find("Invalid station code"), std::string::npos);
    EXPECT_NE(out.find("Invalid class"), std::string::npos);
    EXPECT_NE(out.find("Invalid choice. Enter a number from 1 to 6."), std::string::npos);
    EXPECT_NE(out.find("Invalid choice. Enter a number from 1 to 120."), std::string::npos);
    EXPECT_NE(out.find("ERROR [E1001]: Traveller 1: Invalid gender"), std::string::npos);
    EXPECT_EQ(reservations(), 1);
}

TEST_F(PassengerMenuTest, InvalidMenuChoiceRedisplaysMenu) {
    const std::string out = run("9\nabc\n0\n");
    EXPECT_NE(out.find("Invalid choice. Enter a number from 0 to 8."), std::string::npos);
    EXPECT_NE(out.find("OK: Logged out."), std::string::npos);
}

TEST_F(PassengerMenuTest, EndOfInputLogsOutCleanly) {
    const std::string out = run("1\nSBC\n");
    EXPECT_NE(out.find("OK: Logged out."), std::string::npos);
}

TEST_F(PassengerMenuTest, SearchWithNoResultsSaysSo) {
    const std::string out = run("1\nMYS\nSC\n" + journeyDate() + "\n0\n");
    EXPECT_NE(out.find("No trains found for this route and date."), std::string::npos);
}

TEST_F(PassengerMenuTest, AvailabilityRouteHistoryAndPnrScreens) {
    // Book once, then look at the other screens.
    run("1\nSBC\nMYS\n" + journeyDate() + "\n1\nSL\n1\nAsha Rao\n28\nF\nDL-1234\nY\n0\n");
    const std::string pnr = data_->reservations.listForAccount(seed::kP1).value().at(0).pnr;

    const std::string out = run("3\n20003\n" + journeyDate() + "\nSL\n"
                                "2\n20001\n"
                                "5\n"
                                "6\n" + pnr + "\n"
                                "0\n");
    EXPECT_NE(out.find("Confirmed seats free : 1 of 2"), std::string::npos);
    EXPECT_NE(out.find("KPD"), std::string::npos);
    EXPECT_NE(out.find("Fare SBC -> MAS AC 3-Tier: Rs. 875.00"), std::string::npos);
    EXPECT_NE(out.find(pnr), std::string::npos);
    EXPECT_NE(out.find("Upcoming"), std::string::npos);
    EXPECT_NE(out.find("TICKET   PNR " + pnr), std::string::npos);
}

TEST_F(PassengerMenuTest, CancelShowsNotAvailableWithoutACancellationPort) {
    const std::string out = run("7\n0\n");
    EXPECT_NE(out.find("Cancellation is not available yet."), std::string::npos);
}

// ST-CAN-01 (seat part): book, check status, cancel, the seat is free again.
TEST_F(PassengerMenuTest, CancelReleasesTheSeatThroughTheMenu) {
    run("1\nSBC\nMYS\n" + journeyDate() + "\n1\nSL\n1\nAsha Rao\n28\nF\nDL-1234\nY\n0\n");
    const std::string pnr = data_->reservations.listForAccount(seed::kP1).value().at(0).pnr;
    const std::string out = run("7\n" + pnr + "\nY\n6\n" + pnr + "\n3\n20003\n" + journeyDate() + "\nSL\n0\n",
                                nullptr, true);
    EXPECT_NE(out.find("OK: Ticket " + pnr + " cancelled."), std::string::npos);
    EXPECT_NE(out.find("Status  : Cancelled"), std::string::npos);
    EXPECT_NE(out.find("Confirmed seats free : 2 of 2"), std::string::npos);
}

TEST_F(PassengerMenuTest, CannotCancelAnotherPassengersTicket) {
    auto other = booking_->createBooking(
        test::sessionFor(seed::kP2, Role::Passenger, clock_),
        [&] {
            BookingRequest r;
            r.requestId = "p2-ticket";
            r.trainNumber = "20003";
            r.journeyDate = test::testToday().plusDays(7);
            r.sourceCode = "SBC";
            r.destinationCode = "MYS";
            r.travellers.push_back(test::traveller("Ravi Kumar", 40, 'M'));
            r.fareAccepted = true;
            r.acceptedFare = Money::fromRupees(140);
            return r;
        }());
    ASSERT_TRUE(other.ok());
    const std::string out = run("7\n" + other.value().pnr + "\nY\n0\n", nullptr, true);
    EXPECT_NE(out.find("ERROR [E1006]"), std::string::npos);
    EXPECT_EQ(data_->reservations.findByPnr(other.value().pnr).value()->status, TicketStatus::Confirmed);
}

TEST_F(PassengerMenuTest, ProfileIsShownMaskedAndCanBeEdited) {
    const std::string out = run("8\nY\n\n29\nF\nAadhaar\n123412341234\n8\nN\n0\n");
    EXPECT_NE(out.find("Name     : Asha Rao"), std::string::npos);
    EXPECT_NE(out.find("OK: Profile saved."), std::string::npos);
    EXPECT_NE(out.find("ID proof : Aadhaar ********1234"), std::string::npos);
    EXPECT_EQ(data_->users.findById(seed::kP1).value()->age, 29);
}
