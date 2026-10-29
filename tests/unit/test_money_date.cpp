// test_money_date.cpp - Money formatting and Date arithmetic.

#include <gtest/gtest.h>

#include "common/date.h"
#include "common/money.h"
#include "common/result.h"

using namespace rrs;

TEST(Money, FormatsRupeesWithGroupingAndTwoDecimals) {
    EXPECT_EQ(Money::fromPaise(0).toString(), "0.00");
    EXPECT_EQ(Money::fromPaise(8250).toString(), "82.50");
    EXPECT_EQ(Money::fromRupees(1750).toString(), "1,750.00");
    EXPECT_EQ(Money::fromRupees(1234567).toString(), "1,234,567.00");
    EXPECT_EQ(Money::fromPaise(-12605).toString(), "-126.05");
}

TEST(Date, ParsesIsoAndDisplayFormats) {
    auto d = Date::parseDisplay("05-11-2026");
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->iso(), "2026-11-05");
    EXPECT_EQ(Date::parseIso("2026-11-05")->display(), "05-11-2026");
}

TEST(Date, RejectsImpossibleOrMalformedDates) {
    EXPECT_FALSE(Date::parseDisplay("31-02-2026"));
    EXPECT_FALSE(Date::parseDisplay("29-02-2027"));
    EXPECT_TRUE(Date::parseDisplay("29-02-2028"));
    EXPECT_FALSE(Date::parseDisplay("2026-11-05"));
    EXPECT_FALSE(Date::parseIso("2026/11/05"));
    EXPECT_FALSE(Date::parseDisplay("aa-bb-cccc"));
    EXPECT_FALSE(Date::parseDisplay(""));
}

TEST(Date, DayArithmeticCrossesMonthAndYearEnds) {
    Date d = *Date::fromYmd(2026, 12, 30);
    EXPECT_EQ(d.plusDays(3).iso(), "2027-01-02");
    EXPECT_EQ(Date::fromDayNumber(0).iso(), "1970-01-01");
    EXPECT_EQ(d.plusDays(120).dayNumber() - d.dayNumber(), 120);
    EXPECT_LT(d, d.plusDays(1));
}

TEST(ErrorCatalogue, FormatsStandardErrorLine) {
    EXPECT_EQ(formatError({ErrorCode::NoCapacity, ""}),
              "ERROR [E3001]: No seats or waitlist room on this train. Choose another date or class.");
    EXPECT_EQ(errorName(ErrorCode::PnrNotFound), "ERR-PNR-NOT-FOUND");
    EXPECT_EQ(formatError({ErrorCode::Validation, "Invalid age: must be between 1 and 120."}),
              "ERROR [E1001]: Invalid age: must be between 1 and 120. Please re-enter.");
}
