// seed_data.h
// Baseline data of STP §6 (stations, trains 20001-20004, classes, accounts)
// loaded through the repository interfaces, so it works for SQLite and for
// the in-memory fakes. Used by the passenger demo and the integration tests.
//
// NOTE: the official seed script and test accounts are issue #22. Accounts
// created here have a disabled password hash ("!") - they cannot log in
// until AuthService (#58) stores real PBKDF2 hashes.

// seed_data.h
// Deterministic baseline and demo data for the Railway Reservation System.

#pragma once

#include "common/date.h"
#include "common/result.h"
#include "dal/repositories.h"

namespace rrs::seed {

// Stable IDs of the seed users.
inline constexpr const char* kP1 = "acc-p1";  // Asha Rao
inline constexpr const char* kP2 = "acc-p2";  // Ravi Kumar
inline constexpr const char* kS1 = "acc-s1";  // Reservation staff
inline constexpr const char* kA1 = "acc-a1";  // Administrator
inline constexpr const char* kO1 = "acc-o1";  // Operations

// Loads stations, trains, class configurations, schedules and accounts.
// Does not insert reservations. Used by integration tests.
Status loadSeedData(DataAccess& data, const Date& firstDay, int scheduleDays = 121);

// Loads the complete deterministic demo dataset, including sample reservations.
// Repeated calls with the same date do not duplicate seed records.
// scheduleDays must be at least 6.
Status loadDemoSeedData(DataAccess& data, const Date& firstDay, int scheduleDays = 121);

}  // namespace rrs::seed