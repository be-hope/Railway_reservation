// report_view.h
// Text layout of the operational reports (FR-025) for the operations and
// administrator menus. Lines fit in 80 columns.

#pragma once

#include <string>
#include <vector>

#include "admin/report_service.h"

namespace rrs {

std::string formatBookingStatistics(const BookingStatistics& s);
std::string formatSeatOccupancy(const std::vector<OccupancyRow>& rows);
std::string formatTrainWiseCounts(const std::vector<TrainWiseRow>& rows, const DateRange& range);

}  // namespace rrs
