#pragma once
#include "booking/booking_service.h"
#include "security/access_control.h"
namespace rrs {
// Corrections change traveller identity fields only. PNR, fare, route, capacity,
// status, owner and allocated seats remain unchanged.
class ReservationAdminService {
public:
    ReservationAdminService(DataAccess data, IBookingService& booking, const IClock& clock, IAuditLogger& audit)
        : data_(data), booking_(booking), clock_(clock), access_(clock, audit), audit_(audit) {}
    Result<Reservation> bookOnBehalf(const Session&, const BookingRequest&);
    Result<Reservation> correctReservation(const Session&, const std::string& pnr, const std::vector<Traveller>& corrections);
private:
    DataAccess data_;
    IBookingService& booking_;
    const IClock& clock_;
    AccessControl access_;
    IAuditLogger& audit_;
};
}
