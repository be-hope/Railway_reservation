// ticket_view.cpp

#include "ui/ticket_view.h"

#include <sstream>

namespace rrs {

std::string formatAvailability(const Availability& a) {
    std::ostringstream os;
    os << "Availability  Train " << a.trainNumber << "  " << a.journeyDate.display() << "  "
       << className(a.classCode) << " (" << classCodeString(a.classCode) << ")\n";
    os << "  Confirmed seats free : " << a.confirmedFree << " of " << a.capacity << "\n";
    os << "  RAC slots free       : " << a.racFree << " of " << a.racQuota << "\n";
    os << "  Waitlist             : " << a.waitlistCount << " of " << a.waitlistCapacity << "\n";
    return os.str();
}

std::string formatFareQuote(const FareQuote& q) {
    std::ostringstream os;
    os << "Fare  " << q.sourceCode << " -> " << q.destinationCode << "  " << q.distanceKm << " km  "
       << className(q.classCode) << "\n";
    os << "  Per traveller : Rs. " << q.perTraveller.toString() << "\n";
    os << "  Travellers    : " << q.travellers << "\n";
    os << "  Total fare    : Rs. " << q.total.toString() << "\n";
    return os.str();
}

std::string statusText(const Reservation& r) {
    switch (r.status) {
        case TicketStatus::Confirmed:  return "Confirmed";
        case TicketStatus::Rac:        return "RAC";
        case TicketStatus::Waitlisted: return "Waitlisted - position " + std::to_string(r.waitlistPosition);
        case TicketStatus::Cancelled:  return "Cancelled";
    }
    return "Cancelled";
}

std::string formatTicket(const Reservation& r, const std::string& trainName) {
    std::ostringstream os;
    os << std::string(58, '=') << "\n";
    os << "TICKET   PNR " << r.pnr << "\n";
    os << std::string(58, '=') << "\n";
    os << "Train   : " << r.trainNumber;
    if (!trainName.empty()) os << " " << trainName;
    os << "\n";
    os << "Journey : " << r.sourceCode << " -> " << r.destinationCode << "  on "
       << r.journeyDate.display() << "\n";
    os << "Class   : " << className(r.classCode) << " (" << classCodeString(r.classCode) << ")\n";
    os << "Status  : " << statusText(r) << "\n";
    os << "Travellers:\n";
    int i = 1;
    for (const Traveller& t : r.travellers) {
        os << "  " << i++ << ". " << t.name << ", " << t.age << ", " << t.gender
           << "  ID " << maskIdProof(t.idProofNumber) << "  Seat "
           << (t.seatNumber.empty() ? "-" : t.seatNumber) << "\n";
    }
    os << "Fare    : Rs. " << r.fare.toString() << "\n";
    if (r.refund) os << "Refund  : Rs. " << r.refund->toString() << "\n";
    os << std::string(58, '=') << "\n";
    return os.str();
}

}  // namespace rrs
