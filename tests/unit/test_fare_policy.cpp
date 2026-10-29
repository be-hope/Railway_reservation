// test_fare_policy.cpp - UT-BKG-01 total fare calculation (FR-011).

#include <gtest/gtest.h>

#include "booking/fare_policy.h"

using namespace rrs;

namespace {
constexpr int kSL = 100;   // multiplier 1.0
constexpr int k3A = 250;   // multiplier 2.5
}  // namespace

// UT-BKG-01: 350 km 3A x2; 350 km SL x1; 350 km SL x6; 33 km 3A x1.
TEST(FarePolicy, UT_BKG_01_TotalFareCases) {
    EXPECT_EQ(totalFare(350, k3A, 2).toString(), "1,750.00");
    EXPECT_EQ(totalFare(350, kSL, 1).toString(), "350.00");
    EXPECT_EQ(totalFare(350, kSL, 6).toString(), "2,100.00");
    EXPECT_EQ(totalFare(33, k3A, 1).toString(), "82.50");
}

TEST(FarePolicy, SeedFaresFromTestPlan) {
    EXPECT_EQ(farePerTraveller(210, kSL).toString(), "210.00");
    EXPECT_EQ(farePerTraveller(210, k3A).toString(), "525.00");
    EXPECT_EQ(farePerTraveller(140, kSL).toString(), "140.00");
    EXPECT_EQ(farePerTraveller(570, k3A).toString(), "1,425.00");
    EXPECT_EQ(totalFare(350, k3A, 3).toString(), "2,625.00");   // ST-BKG-02
}

TEST(FarePolicy, RoundsHalfUpToThePaisa) {
    FareRules rules;
    rules.paisePerKm = 1;   // 0.01 per km
    EXPECT_EQ(farePerTraveller(1, 250, rules).paise(), 3);   // 2.5 paise -> 3
    EXPECT_EQ(farePerTraveller(1, 240, rules).paise(), 2);   // 2.4 paise -> 2
}

TEST(FarePolicy, NonPositiveInputsGiveZero) {
    EXPECT_EQ(totalFare(0, kSL, 1).paise(), 0);
    EXPECT_EQ(totalFare(100, kSL, 0).paise(), 0);
    EXPECT_EQ(totalFare(100, 0, 1).paise(), 0);
}

TEST(FarePolicy, RateIsConfigurable) {
    FareRules rules;
    rules.paisePerKm = 150;   // 1.50 per km
    EXPECT_EQ(totalFare(100, k3A, 2, rules).toString(), "750.00");
}
