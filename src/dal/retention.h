#pragma once
#include "dal/sqlite_database.h"
namespace rrs {
// Clears only traveller ID numbers after the 90-day journey retention window.
// Account profile IDs remain until the owner changes/deletes the profile.
Result<int> clearExpiredTravellerIds(SqliteDatabase& db, const Date& today);
}
