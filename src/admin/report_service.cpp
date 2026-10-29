// report_service.cpp

#include "admin/report_service.h"

#include <map>

namespace rrs {

std::string formatBasisPoints(int bp) {
    std::string frac = std::to_string(bp % 100);
    if (frac.size() < 2) frac.insert(0, "0");
    return std::to_string(bp / 100) + "." + frac + " %";
}

int ratioBasisPoints(long numerator, long denominator) {
    if (denominator <= 0) return 0;
    return static_cast<int>((numerator * 10000 + denominator / 2) / denominator);
}

Status ReportService::checkAccess(const Session& session) {
    return requireRole(session, {Role::Admin, Role::Ops}, clock_);
}

Status ReportService::checkRange(const DateRange& range) {
    if (range.to < range.from) {
        return Status::failure(ErrorCode::Validation, "Invalid date range: 'from' must not be after 'to'.");
    }
    if (range.to.dayNumber() - range.from.dayNumber() >= kMaxReportRangeDays) {
        return Status::failure(ErrorCode::Validation, "Invalid date range: at most 366 days.");
    }
    return Status::success();
}

Result<BookingStatistics> ReportService::bookingStatistics(const Session& session, const DateRange& range) {
    Status access = checkAccess(session);
    if (!access.ok()) return access.error();
    Status valid = checkRange(range);
    if (!valid.ok()) return valid.error();

    auto rows = data_.reservations.listByJourneyDate(range.from, range.to);
    if (!rows.ok()) return rows.error();

    BookingStatistics s;
    s.range = range;
    for (const Reservation& r : rows.value()) {
        ++s.total;
        switch (r.status) {
            case TicketStatus::Confirmed:  ++s.confirmed; break;
            case TicketStatus::Rac:        ++s.rac; break;
            case TicketStatus::Waitlisted: ++s.waitlisted; break;
            case TicketStatus::Cancelled:  ++s.cancelled; break;
        }
        if (r.status == TicketStatus::Cancelled) {
            if (r.refund) s.totalRefunds += *r.refund;
        } else {
            s.totalFare += r.fare;
        }
    }
    return s;
}

Result<std::vector<OccupancyRow>> ReportService::seatOccupancy(const Session& session,
                                                               const std::string& trainNumber,
                                                               const Date& journeyDate) {
    using R = Result<std::vector<OccupancyRow>>;
    Status access = checkAccess(session);
    if (!access.ok()) return access.error();

    auto train = data_.trains.findTrain(trainNumber);
    if (!train.ok()) return train.error();
    if (!train.value()) return R::failure(ErrorCode::TrainNotFound);
    auto configs = data_.config.listClassConfigs(trainNumber);
    if (!configs.ok()) return configs.error();

    std::vector<OccupancyRow> out;
    for (const ClassConfig& c : configs.value()) {
        auto occ = data_.reservations.occupancy(trainNumber, journeyDate, c.classCode);
        if (!occ.ok()) return occ.error();
        OccupancyRow row;
        row.trainNumber = trainNumber;
        row.trainName = train.value()->name;
        row.journeyDate = journeyDate;
        row.classCode = c.classCode;
        row.capacity = c.capacity();
        row.confirmedSeats = occ.value().confirmedTravellers;
        row.occupancyBasisPoints = ratioBasisPoints(row.confirmedSeats, row.capacity);
        row.racTravellers = occ.value().racTravellers;
        row.waitlistedTickets = occ.value().waitlistTickets;
        out.push_back(row);
    }
    return out;
}

Result<std::vector<TrainWiseRow>> ReportService::trainWiseCounts(const Session& session, const DateRange& range) {
    Status access = checkAccess(session);
    if (!access.ok()) return access.error();
    Status valid = checkRange(range);
    if (!valid.ok()) return valid.error();

    auto trains = data_.trains.listTrains();
    if (!trains.ok()) return trains.error();
    auto rows = data_.reservations.listByJourneyDate(range.from, range.to);
    if (!rows.ok()) return rows.error();

    std::map<std::string, TrainWiseRow> byTrain;
    for (const Train& t : trains.value()) {
        TrainWiseRow& row = byTrain[t.number];
        row.trainNumber = t.number;
        row.trainName = t.name;
    }
    for (const Reservation& r : rows.value()) {
        TrainWiseRow& row = byTrain[r.trainNumber];
        row.trainNumber = r.trainNumber;
        ++row.reservations;
        if (r.classCode == ClassCode::SL) ++row.sleeperReservations;
        else ++row.ac3Reservations;
        if (r.status == TicketStatus::Cancelled) ++row.cancelled;
        else row.travellers += r.travellerCount();
    }
    std::vector<TrainWiseRow> out;
    for (auto& kv : byTrain) {
        kv.second.cancellationBasisPoints = ratioBasisPoints(kv.second.cancelled, kv.second.reservations);
        out.push_back(kv.second);
    }
    return out;
}

}  // namespace rrs
