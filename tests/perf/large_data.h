// large_data.h
// Large-data generator for PERF-01/02 and REL-01 (STP §6): 200 trains,
// 40 stations and 20,000 reservations on top of the seed data. The data is
// generated from a fixed seed, so every run uses the same database.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "common/date.h"
#include "common/result.h"
#include "dal/repositories.h"

namespace rrs::perf {

struct LargeDataSpec {
    int stations = 40;
    int trains = 200;
    int reservations = 20000;
    int scheduleDays = 30;
    std::uint32_t seed = 20261102;
};

struct LargeData {
    std::vector<std::string> stationCodes;
    std::vector<Train> trains;   // generated trains (numbers 30001...)
    int reservationsInserted = 0;
};

// Loads the STP seed data, then the generated stations, trains, class
// configurations, schedules and reservations (one transaction).
Result<LargeData> generateLargeData(DataAccess& data, const Date& today, const LargeDataSpec& spec = {});

// Percentile of the samples (nearest rank), in milliseconds.
double percentileMs(std::vector<double> samples, double percentile);

}  // namespace rrs::perf
