// seed_data.h
// Baseline data of STP §6 (stations, trains 20001-20004, classes, accounts)
// loaded through the repository interfaces, so it works for SQLite and for
// the in-memory fakes. Used by the passenger demo and the integration tests.
//
// NOTE: the official seed script and test accounts are issue #22. Accounts
// created here have a disabled password hash ("!") - they cannot log in
// until AuthService (#58) stores real PBKDF2 hashes.

#pragma once

#include "common/date.h"
#include "common/result.h"
#include "dal/repositories.h"

namespace rrs::seed {

// Account ids of the seed users.
inline constexpr const char* kP1 = "acc-p1";   // Asha Rao, 9876500001
inline constexpr const char* kP2 = "acc-p2";   // Ravi Kumar, 9876500002
inline constexpr const char* kS1 = "acc-s1";   // staff01
inline constexpr const char* kA1 = "acc-a1";   // admin01
inline constexpr const char* kO1 = "acc-o1";   // ops01

// Stations, trains, class configuration and accounts; every train gets a
// schedule for each day from `firstDay` to firstDay + scheduleDays - 1.
Status loadSeedData(DataAccess& data, const Date& firstDay, int scheduleDays = 121);

}  // namespace rrs::seed
