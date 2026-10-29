// demo_adapters.h
// Interim implementations of the passenger-menu ports that other modules
// own (train search #10, booking history #33, profile #6). They read the
// repositories directly and exist only so the menu can be demonstrated and
// tested end to end; replace each one with the owning module's service.

#pragma once

#include "cancel/seat_release_service.h"
#include "common/clock.h"
#include "dal/repositories.h"
#include "ui/passenger_menu.h"

namespace rrs::demo {

// "HH:MM", and "hh:mm" duration between two stops (correct across midnight).
TrainSummary summarize(const Train& train, const std::string& source, const std::string& destination);

class RepositoryTrainSearch : public ITrainSearchPort {
public:
    explicit RepositoryTrainSearch(ITrainRepository& trains) : trains_(trains) {}
    Result<std::vector<TrainSummary>> search(const std::string& source, const std::string& destination,
                                             const Date& date) override;
    Result<Train> route(const std::string& trainNumber) override;

private:
    ITrainRepository& trains_;
};

class RepositoryBookingHistory : public IBookingHistoryPort {
public:
    RepositoryBookingHistory(IReservationRepository& reservations, const IClock& clock)
        : reservations_(reservations), clock_(clock) {}
    Result<std::vector<Reservation>> history(const Session& session) override;

private:
    IReservationRepository& reservations_;
    const IClock& clock_;
};

class RepositoryProfile : public IProfilePort {
public:
    RepositoryProfile(IUserRepository& users, const IClock& clock) : users_(users), clock_(clock) {}
    Result<Account> view(const Session& session) override;
    Status update(const Session& session, const Account& changes) override;

private:
    IUserRepository& users_;
    const IClock& clock_;
};

// Cancels through SeatReleaseService after the ownership check of
// BookingService::getTicket. The departure-time rule and the refund belong to
// CancellationService (#47, #48); until it exists no refund is recorded.
class InterimCancellation : public ICancellationPort {
public:
    InterimCancellation(IBookingService& booking, SeatReleaseService& release)
        : booking_(booking), release_(release) {}
    Result<Reservation> cancel(const Session& session, const std::string& pnr) override;

private:
    IBookingService& booking_;
    SeatReleaseService& release_;
};

}  // namespace rrs::demo
