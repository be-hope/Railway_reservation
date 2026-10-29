// test_pnr_generator.cpp - UT-BKG-03 PNR generation (FR-014, issue #31).

#include <gtest/gtest.h>

#include <set>

#include "booking/pnr_generator.h"

using namespace rrs;

// UT-BKG-03 step 1: 10,000 PNRs, all 10 digits, first digit 1-9, no duplicates.
TEST(PnrGenerator, UT_BKG_03_TenThousandUniqueValidPnrs) {
    PnrGenerator gen;
    std::set<std::string> issued;
    for (int i = 0; i < 10000; ++i) {
        auto pnr = gen.generate([&](const std::string& p) { return Result<bool>(issued.count(p) > 0); });
        ASSERT_TRUE(pnr.ok());
        EXPECT_TRUE(PnrGenerator::isValidPnr(pnr.value())) << pnr.value();
        EXPECT_TRUE(issued.insert(pnr.value()).second) << "duplicate " << pnr.value();
    }
    EXPECT_EQ(issued.size(), 10000u);
}

// UT-BKG-03 step 2: a forced collision on the first draw makes it draw again.
TEST(PnrGenerator, UT_BKG_03_CollisionDrawsANewValue) {
    std::uint64_t next = 0;
    PnrGenerator gen([&]() { return next++; });   // 1000000000, 1000000001, ...
    int calls = 0;
    auto pnr = gen.generate([&](const std::string& p) {
        ++calls;
        return Result<bool>(p == "1000000000");
    });
    ASSERT_TRUE(pnr.ok());
    EXPECT_EQ(pnr.value(), "1000000001");
    EXPECT_EQ(calls, 2);
}

TEST(PnrGenerator, GivesUpAfterFiveCollisions) {
    PnrGenerator gen([]() { return std::uint64_t{42}; });
    int calls = 0;
    auto pnr = gen.generate([&](const std::string&) {
        ++calls;
        return Result<bool>(true);
    });
    EXPECT_EQ(pnr.code(), ErrorCode::Database);
    EXPECT_EQ(calls, PnrGenerator::kMaxAttempts);
}

TEST(PnrGenerator, PropagatesStorageErrorsFromTheExistsCheck) {
    PnrGenerator gen;
    auto pnr = gen.generate([](const std::string&) { return Result<bool>::failure(ErrorCode::Database, "x"); });
    EXPECT_EQ(pnr.code(), ErrorCode::Database);
}

TEST(PnrGenerator, RangeCoversFirstAndLastValues) {
    PnrGenerator low([]() { return std::uint64_t{0}; });
    PnrGenerator high([]() { return std::uint64_t{8999999999ULL}; });
    auto never = [](const std::string&) { return Result<bool>(false); };
    EXPECT_EQ(low.generate(never).value(), "1000000000");
    EXPECT_EQ(high.generate(never).value(), "9999999999");
}

// UT-CAN-05 style format checks (also used before any database access).
TEST(PnrGenerator, IsValidPnrRejectsMalformedValues) {
    EXPECT_TRUE(PnrGenerator::isValidPnr("1234567890"));
    EXPECT_FALSE(PnrGenerator::isValidPnr("12AB"));
    EXPECT_FALSE(PnrGenerator::isValidPnr("123456789"));
    EXPECT_FALSE(PnrGenerator::isValidPnr("12345678901"));
    EXPECT_FALSE(PnrGenerator::isValidPnr("' OR 1=1"));
    EXPECT_FALSE(PnrGenerator::isValidPnr("0123456789"));
}
