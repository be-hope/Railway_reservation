// trip_invariants.h
// Consistency checks for one train/date/class, used after every step of the
// randomised and soak tests:
//   * no seat is held by two active travellers, and every seat exists
//   * confirmed travellers <= capacity, RAC travellers <= RAC quota,
//     waitlisted tickets <= waitlist capacity
//   * only CONFIRMED travellers hold seats
//   * waitlist positions are exactly 1..k and follow booking order
//   * every RAC ticket was booked before every waitlisted ticket
//   * nobody waits while the head of the queue would fit (BR-07: promotion
//     never leaves a ticket behind that should have moved up)

#pragma once

#include <algorithm>
#include <set>
#include <string>

#include "booking/seat_allocator.h"
#include "dal/repositories.h"

namespace rrs::test {

// Returns an empty string when the trip is consistent, else a description.
inline std::string checkTripInvariants(IReservationRepository& reservations, const ClassConfig& config,
                                       const Date& date) {
    auto active = reservations.listActiveForTrip(config.trainNumber, date, config.classCode);
    if (!active.ok()) return "listActiveForTrip failed";
    std::set<std::string> seats;
    int confirmed = 0, rac = 0;
    std::vector<const Reservation*> racQ, wlQ;
    for (const Reservation& r : active.value()) {
        for (const Traveller& t : r.travellers) {
            if (t.seatNumber.empty()) continue;
            if (r.status != TicketStatus::Confirmed) return r.pnr + " holds a seat but is not CONFIRMED";
            if (!seatExists(config, t.seatNumber)) return "seat " + t.seatNumber + " is outside the layout";
            if (!seats.insert(t.seatNumber).second) return "seat " + t.seatNumber + " allocated twice";
        }
        if (r.status == TicketStatus::Confirmed) {
            confirmed += r.travellerCount();
            for (const Traveller& t : r.travellers) {
                if (t.seatNumber.empty()) return r.pnr + " is CONFIRMED without a seat";
            }
        } else if (r.status == TicketStatus::Rac) {
            rac += r.travellerCount();
            racQ.push_back(&r);
        } else if (r.status == TicketStatus::Waitlisted) {
            wlQ.push_back(&r);
        }
    }
    if (confirmed > config.capacity()) return "confirmed travellers exceed capacity";
    if (rac > config.racQuota) return "RAC travellers exceed the quota";
    if (static_cast<int>(wlQ.size()) > config.waitlistCapacity) return "waitlist exceeds capacity";

    std::sort(wlQ.begin(), wlQ.end(),
              [](const Reservation* a, const Reservation* b) { return a->waitlistPosition < b->waitlistPosition; });
    for (std::size_t i = 0; i < wlQ.size(); ++i) {
        if (wlQ[i]->waitlistPosition != static_cast<int>(i) + 1) return "waitlist positions are not 1..k";
        if (i > 0 && wlQ[i]->queueSeq < wlQ[i - 1]->queueSeq) return "waitlist order differs from booking order";
    }
    for (const Reservation* r : racQ) {
        for (const Reservation* w : wlQ) {
            if (w->queueSeq < r->queueSeq) return "a waitlisted ticket was booked before an RAC ticket";
        }
    }

    // The head of the queue must not fit what is free (else it was skipped).
    std::sort(racQ.begin(), racQ.end(),
              [](const Reservation* a, const Reservation* b) { return a->queueSeq < b->queueSeq; });
    const Reservation* head = !racQ.empty() ? racQ.front() : (!wlQ.empty() ? wlQ.front() : nullptr);
    if (head && head->travellerCount() <= config.capacity() - confirmed) {
        return "head of queue " + head->pnr + " fits a free seat but was not promoted";
    }
    if (!wlQ.empty() && wlQ.front()->travellerCount() <= config.racQuota - rac) {
        return "waitlist head " + wlQ.front()->pnr + " fits a free RAC slot but was not promoted";
    }
    return "";
}

}  // namespace rrs::test
