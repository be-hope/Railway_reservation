// waitlist_manager.cpp

#include "cancel/waitlist_manager.h"

#include <algorithm>
#include <map>

#include "booking/seat_allocator.h"

namespace rrs {

namespace {

bool sameState(const Reservation& a, const Reservation& b) {
    if (a.status != b.status || a.waitlistPosition != b.waitlistPosition) return false;
    for (std::size_t i = 0; i < a.travellers.size(); ++i) {
        if (a.travellers[i].seatNumber != b.travellers[i].seatNumber) return false;
    }
    return true;
}

}  // namespace

PromotionPlan planPromotion(const ClassConfig& config, const std::vector<Reservation>& active) {
    std::vector<Reservation> trip = active;
    int confirmedTravellers = 0;
    int racTravellers = 0;
    std::vector<std::string> occupied;
    std::vector<Reservation*> rac, waitlist;
    for (Reservation& r : trip) {
        if (r.status == TicketStatus::Confirmed) {
            confirmedTravellers += r.travellerCount();
            for (const Traveller& t : r.travellers) {
                if (!t.seatNumber.empty()) occupied.push_back(t.seatNumber);
            }
        } else if (r.status == TicketStatus::Rac) {
            racTravellers += r.travellerCount();
            rac.push_back(&r);
        } else if (r.status == TicketStatus::Waitlisted) {
            waitlist.push_back(&r);
        }
    }
    std::stable_sort(rac.begin(), rac.end(),
                     [](const Reservation* a, const Reservation* b) { return a->queueSeq < b->queueSeq; });
    std::stable_sort(waitlist.begin(), waitlist.end(), [](const Reservation* a, const Reservation* b) {
        if (a->waitlistPosition != b->waitlistPosition) return a->waitlistPosition < b->waitlistPosition;
        return a->queueSeq < b->queueSeq;
    });

    std::vector<Reservation*> queue = rac;
    queue.insert(queue.end(), waitlist.begin(), waitlist.end());

    PromotionPlan plan;
    std::map<std::string, Reservation> before;
    for (const Reservation& r : active) before[r.pnr] = r;

    // Phase 1: confirm from the head of the queue while the party fits.
    for (Reservation* r : queue) {
        const int n = r->travellerCount();
        if (n > config.capacity() - confirmedTravellers) break;
        auto seats = SeatAllocator::allocate(config, occupied, n);
        if (!seats.ok()) break;
        if (r->status == TicketStatus::Rac) racTravellers -= n;
        r->status = TicketStatus::Confirmed;
        r->waitlistPosition = 0;
        for (int i = 0; i < n; ++i) {
            r->travellers[static_cast<std::size_t>(i)].seatNumber = seats.value()[static_cast<std::size_t>(i)];
            occupied.push_back(seats.value()[static_cast<std::size_t>(i)]);
        }
        confirmedTravellers += n;
        plan.promotedPnrs.push_back(r->pnr);
    }

    // Phase 2: move the head of the waitlist into free RAC slots.
    for (Reservation* r : waitlist) {
        if (r->status != TicketStatus::Waitlisted) continue;   // confirmed in phase 1
        const int n = r->travellerCount();
        if (n > config.racQuota - racTravellers) break;
        r->status = TicketStatus::Rac;
        r->waitlistPosition = 0;
        racTravellers += n;
        plan.promotedPnrs.push_back(r->pnr);
    }

    // Renumber the remaining waitlist without gaps, keeping the order.
    int position = 0;
    for (Reservation* r : waitlist) {
        if (r->status == TicketStatus::Waitlisted) r->waitlistPosition = ++position;
    }

    for (const Reservation& r : trip) {
        if (!sameState(r, before[r.pnr])) plan.changed.push_back(r);
    }
    return plan;
}

Result<std::vector<std::string>> WaitlistManager::promote(const std::string& trainNumber,
                                                          const Date& journeyDate, ClassCode classCode) {
    using R = Result<std::vector<std::string>>;
    auto config = config_.findClassConfig(trainNumber, classCode);
    if (!config.ok()) return config.error();
    if (!config.value()) return R::failure(ErrorCode::TrainNotFound);

    auto active = reservations_.listActiveForTrip(trainNumber, journeyDate, classCode);
    if (!active.ok()) return active.error();

    PromotionPlan plan = planPromotion(*config.value(), active.value());
    for (const Reservation& r : plan.changed) {
        Status s = reservations_.update(r);
        if (!s.ok()) return s.error();
    }
    return plan.promotedPnrs;
}

}  // namespace rrs
