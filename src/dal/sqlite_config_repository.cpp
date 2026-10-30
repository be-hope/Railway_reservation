// sqlite_config_repository.cpp

#include "dal/sqlite_repositories.h"

namespace rrs {

namespace {

const char* const kConfigColumns =
    "train_number, class_code, coaches, seats_per_coach, rac_quota, wl_capacity, fare_multiplier_pct";

ClassConfig readConfig(const Statement& st) {
    ClassConfig c;
    c.trainNumber = st.text(0);
    c.classCode = parseClassCode(st.text(1)).value_or(ClassCode::SL);
    c.coaches = static_cast<int>(st.integer(2));
    c.seatsPerCoach = static_cast<int>(st.integer(3));
    c.racQuota = static_cast<int>(st.integer(4));
    c.waitlistCapacity = static_cast<int>(st.integer(5));
    c.fareMultiplierPercent = static_cast<int>(st.integer(6));
    return c;
}

}  // namespace

Status SqliteConfigRepository::saveClassConfig(const ClassConfig& c) {
    Statement st(db_,
                 "INSERT INTO class_config(train_number, class_code, coaches, seats_per_coach, "
                 "rac_quota, wl_capacity, fare_multiplier_pct) VALUES (?, ?, ?, ?, ?, ?, ?) "
                 "ON CONFLICT(train_number, class_code) DO UPDATE SET coaches = excluded.coaches, "
                 "seats_per_coach = excluded.seats_per_coach, rac_quota = excluded.rac_quota, "
                 "wl_capacity = excluded.wl_capacity, fare_multiplier_pct = excluded.fare_multiplier_pct");
    return st.bind(1, c.trainNumber)
        .bind(2, classCodeString(c.classCode))
        .bind(3, c.coaches)
        .bind(4, c.seatsPerCoach)
        .bind(5, c.racQuota)
        .bind(6, c.waitlistCapacity)
        .bind(7, c.fareMultiplierPercent)
        .execute();
}

Result<std::optional<ClassConfig>> SqliteConfigRepository::findClassConfig(
    const std::string& trainNumber, ClassCode classCode) {
    std::string sql = std::string("SELECT ") + kConfigColumns +
                      " FROM class_config WHERE train_number = ? AND class_code = ?";
    Statement st(db_, sql.c_str());
    st.bind(1, trainNumber).bind(2, classCodeString(classCode));
    if (!st.step()) {
        if (!st.status().ok()) return st.status().error();
        return std::optional<ClassConfig>();
    }
    return std::optional<ClassConfig>(readConfig(st));
}

Result<std::vector<ClassConfig>> SqliteConfigRepository::listClassConfigs(const std::string& trainNumber) {
    std::string sql = std::string("SELECT ") + kConfigColumns +
                      " FROM class_config WHERE train_number = ? ORDER BY class_code DESC";
    Statement st(db_, sql.c_str());
    st.bind(1, trainNumber);
    std::vector<ClassConfig> out;
    while (st.step()) out.push_back(readConfig(st));
    if (!st.status().ok()) return st.status().error();
    return out;
}

}  // namespace rrs
