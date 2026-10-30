// seat_allocator.cpp

#include "booking/seat_allocator.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace rrs {

namespace {

int digitsFor(int n) {
    int d = 1;
    while (n >= 10) { n /= 10; ++d; }
    return d;
}

bool parsePositive(const std::string& s, int& out) {
    if (s.empty() || s.size() > 6) return false;
    int v = 0;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        v = v * 10 + (c - '0');
    }
    if (v <= 0) return false;
    out = v;
    return true;
}

}  // namespace

std::string seatLabel(ClassCode classCode, int coach, int seat, int seatsPerCoach) {
    const int width = std::max(2, digitsFor(seatsPerCoach));
    std::string number = std::to_string(seat);
    if (static_cast<int>(number.size()) < width) number.insert(0, width - number.size(), '0');
    return coachPrefix(classCode) + std::to_string(coach) + "-" + number;
}

std::optional<SeatPosition> parseSeatLabel(ClassCode classCode, const std::string& label) {
    const std::string prefix = coachPrefix(classCode);
    if (label.compare(0, prefix.size(), prefix) != 0) return std::nullopt;
    const auto dash = label.find('-', prefix.size());
    if (dash == std::string::npos) return std::nullopt;
    SeatPosition p;
    if (!parsePositive(label.substr(prefix.size(), dash - prefix.size()), p.coach)) return std::nullopt;
    if (!parsePositive(label.substr(dash + 1), p.seat)) return std::nullopt;
    return p;
}

bool seatExists(const ClassConfig& config, const std::string& label) {
    auto p = parseSeatLabel(config.classCode, label);
    return p && p->coach <= config.coaches && p->seat <= config.seatsPerCoach;
}

Result<std::vector<std::string>> SeatAllocator::allocate(const ClassConfig& config,
                                                         const std::vector<std::string>& occupied,
                                                         int count) {
    using R = Result<std::vector<std::string>>;
    if (count <= 0) return R::failure(ErrorCode::Validation, "Seat count must be positive.");

    // Compare by position so "S1-7" and "S1-07" are the same seat.
    std::set<std::pair<int, int>> taken;
    for (const std::string& label : occupied) {
        if (auto p = parseSeatLabel(config.classCode, label)) taken.insert({p->coach, p->seat});
    }

    std::vector<std::string> seats;
    for (int coach = 1; coach <= config.coaches && static_cast<int>(seats.size()) < count; ++coach) {
        for (int seat = 1; seat <= config.seatsPerCoach && static_cast<int>(seats.size()) < count; ++seat) {
            if (taken.count({coach, seat}) == 0) {
                seats.push_back(seatLabel(config.classCode, coach, seat, config.seatsPerCoach));
            }
        }
    }
    if (static_cast<int>(seats.size()) < count) {
        return R::failure(ErrorCode::NoCapacity, "not enough free seats");
    }
    return seats;
}

}  // namespace rrs
