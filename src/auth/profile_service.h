#pragma once
#include "security/access_control.h"
#include "security/input_validator.h"
#include "ui/passenger_menu.h"
namespace rrs {
class ProfileService : public IProfilePort {
public:
    ProfileService(IUserRepository& users, const IClock& clock, IAuditLogger& audit) : users_(users), access_(clock, audit), audit_(audit) {}
    Result<Account> view(const Session&) override;
    Status update(const Session&, const Account&) override;
private:
    IUserRepository& users_;
    AccessControl access_;
    IAuditLogger& audit_;
};
class BookingHistoryService : public IBookingHistoryPort {
public:
    BookingHistoryService(IReservationRepository& reservations, const IClock& clock, IAuditLogger& audit) : reservations_(reservations), access_(clock, audit) {}
    Result<std::vector<Reservation>> history(const Session&) override;
private:
    IReservationRepository& reservations_;
    AccessControl access_;
};
}
