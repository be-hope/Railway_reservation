// report_view.cpp

#include "ui/report_view.h"

#include <sstream>

namespace rrs {

namespace {

std::string pad(std::string s, std::size_t width) {
    if (s.size() > width) s.resize(width);
    if (s.size() < width) s.append(width - s.size(), ' ');
    return s;
}

std::string padLeft(const std::string& s, std::size_t width) {
    return s.size() >= width ? s : std::string(width - s.size(), ' ') + s;
}

}  // namespace

std::string formatBookingStatistics(const BookingStatistics& s) {
    std::ostringstream os;
    os << "BOOKING STATISTICS  " << s.range.from.display() << " to " << s.range.to.display() << "\n";
    os << std::string(58, '=') << "\n";
    os << "Total reservations : " << s.total << "\n";
    os << "  Confirmed        : " << s.confirmed << "\n";
    os << "  RAC              : " << s.rac << "\n";
    os << "  Waitlisted       : " << s.waitlisted << "\n";
    os << "  Cancelled        : " << s.cancelled << "\n";
    os << "Total fare booked  : Rs. " << s.totalFare.toString() << "\n";
    os << "Total refunds      : Rs. " << s.totalRefunds.toString() << "\n";
    return os.str();
}

std::string formatSeatOccupancy(const std::vector<OccupancyRow>& rows) {
    std::ostringstream os;
    if (rows.empty()) return "No classes configured for this train.\n";
    os << "SEAT OCCUPANCY  Train " << rows[0].trainNumber << " " << rows[0].trainName << "  "
       << rows[0].journeyDate.display() << "\n";
    os << std::string(58, '=') << "\n";
    os << "Class  Capacity  Booked  Occupancy    RAC  Waitlist\n";
    for (const OccupancyRow& r : rows) {
        os << pad(classCodeString(r.classCode), 7) << padLeft(std::to_string(r.capacity), 8)
           << padLeft(std::to_string(r.confirmedSeats), 8) << padLeft(formatBasisPoints(r.occupancyBasisPoints), 11)
           << padLeft(std::to_string(r.racTravellers), 7) << padLeft(std::to_string(r.waitlistedTickets), 10) << "\n";
    }
    return os.str();
}

std::string formatTrainWiseCounts(const std::vector<TrainWiseRow>& rows, const DateRange& range) {
    std::ostringstream os;
    os << "TRAIN-WISE RESERVATIONS  " << range.from.display() << " to " << range.to.display() << "\n";
    os << std::string(78, '=') << "\n";
    os << "Train  Name                      Resv  Travellers   SL   3A  Cancelled  Ratio\n";
    for (const TrainWiseRow& r : rows) {
        os << pad(r.trainNumber, 7) << pad(r.trainName, 24) << padLeft(std::to_string(r.reservations), 6)
           << padLeft(std::to_string(r.travellers), 12) << padLeft(std::to_string(r.sleeperReservations), 5)
           << padLeft(std::to_string(r.ac3Reservations), 5) << padLeft(std::to_string(r.cancelled), 11)
           << padLeft(formatBasisPoints(r.cancellationBasisPoints), 9) << "\n";
    }
    return os.str();
}

}  // namespace rrs
