// report_service.h
// Operational reports (FR-025, AC-025, SRS Appendix A "Report contents").
// Readable by ADMIN and OPS (VIEW_REPORTS); every figure is computed from the
// stored reservations through the repositories, so it equals a direct query.
//
// Definitions:
//   * a reservation is counted by its journey date
//   * "total fare booked" sums the fares of tickets that are not cancelled;
//     "total refunds" sums the refunds of cancelled tickets
//   * occupancy % = confirmed seats / capacity, two decimals
//   * cancellation ratio = cancelled reservations / all reservations

#pragma once

#include <string>
#include <vector>

#include "common/clock.h"
#include "common/session.h"
#include "dal/repositories.h"

namespace rrs {

constexpr long kMaxReportRangeDays = 366;

struct DateRange {
    Date from;
    Date to;
};

struct BookingStatistics {
    DateRange range;
    int total = 0;
    int confirmed = 0;
    int rac = 0;
    int waitlisted = 0;
    int cancelled = 0;
    Money totalFare;
    Money totalRefunds;
};

struct OccupancyRow {
    std::string trainNumber;
    std::string trainName;
    Date journeyDate;
    ClassCode classCode = ClassCode::SL;
    int capacity = 0;
    int confirmedSeats = 0;
    int occupancyBasisPoints = 0;   // 10000 = 100.00 %
    int racTravellers = 0;
    int waitlistedTickets = 0;
};

struct TrainWiseRow {
    std::string trainNumber;
    std::string trainName;
    int reservations = 0;            // all statuses
    int travellers = 0;              // on tickets that are not cancelled
    int sleeperReservations = 0;
    int ac3Reservations = 0;
    int cancelled = 0;
    int cancellationBasisPoints = 0; // cancelled / reservations
};

class ReportService {
public:
    ReportService(DataAccess data, const IClock& clock) : data_(data), clock_(clock) {}

    Result<BookingStatistics> bookingStatistics(const Session& session, const DateRange& range);
    // One row per configured class of the train on that date.
    Result<std::vector<OccupancyRow>> seatOccupancy(const Session& session, const std::string& trainNumber,
                                                    const Date& journeyDate);
    // One row per train (all trains, including those without bookings).
    Result<std::vector<TrainWiseRow>> trainWiseCounts(const Session& session, const DateRange& range);

private:
    Status checkAccess(const Session& session);
    static Status checkRange(const DateRange& range);

    DataAccess data_;
    const IClock& clock_;
};

// "87.50 %"
std::string formatBasisPoints(int basisPoints);
// Rounded half up; 0 when the denominator is 0.
int ratioBasisPoints(long numerator, long denominator);

}  // namespace rrs
