// passenger_menu.h
// Passenger console menu (SAD §4.5, NFR-003, USB-01):
//   1 Search trains · 2 Schedule, route & fare · 3 Check availability ·
//   4 Book ticket · 5 My bookings · 6 PNR status · 7 Cancel ticket ·
//   8 My profile · 0 Logout
//
// Booking path in six primary steps (login happens before this menu):
//   Login -> Search -> Select train and class -> Enter travellers ->
//   Confirm fare -> View ticket
//
// The menu talks only to business-layer ports (never to a repository,
// SAD §3.5). Ports owned by other modules are declared here so the menu can be
// wired to whatever implements them: train search (#10), booking history
// (#33), PNR status (#32), cancellation (#46), profile (#6).

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "booking/booking_service.h"
#include "common/clock.h"
#include "common/session.h"
#include "ui/console_io.h"

namespace rrs {

// One search-result row (FR-006).
struct TrainSummary {
    std::string trainNumber;
    std::string trainName;
    std::string departure;   // HH:MM at the source
    std::string arrival;     // HH:MM at the destination
    std::string duration;    // hh:mm, correct across midnight
};

class ITrainSearchPort {
public:
    virtual ~ITrainSearchPort() = default;
    virtual Result<std::vector<TrainSummary>> search(const std::string& source,
                                                     const std::string& destination,
                                                     const Date& date) = 0;
    virtual Result<Train> route(const std::string& trainNumber) = 0;
};

class IBookingHistoryPort {
public:
    virtual ~IBookingHistoryPort() = default;
    // Own reservations ordered by journey date (FR-015).
    virtual Result<std::vector<Reservation>> history(const Session& session) = 0;
};

class ICancellationPort {
public:
    virtual ~ICancellationPort() = default;
    // Returns the cancelled reservation (status CANCELLED, refund filled in).
    virtual Result<Reservation> cancel(const Session& session, const std::string& pnr) = 0;
};

class IProfilePort {
public:
    virtual ~IProfilePort() = default;
    virtual Result<Account> view(const Session& session) = 0;
    virtual Status update(const Session& session, const Account& changes) = 0;
};

struct PassengerPorts {
    IBookingService& booking;
    ITrainSearchPort& trains;
    IBookingHistoryPort& history;
    IProfilePort& profile;
    ICancellationPort* cancellation = nullptr;   // optional until #46 lands
};

class PassengerMenu {
public:
    using RequestIdFactory = std::function<std::string()>;

    PassengerMenu(ConsoleIO& io, PassengerPorts ports, Session session, const IClock& clock,
                  RequestIdFactory newRequestId);

    // Runs until the passenger chooses 0 (Logout) or input ends.
    void run();

    // Primary screens visited since the menu started, in order. Used by the
    // USB-01 / ST-BKG-01 tests to count booking steps.
    const std::vector<std::string>& screens() const { return screens_; }

private:
    void enter(const std::string& screen);
    void searchTrains();
    void showRoute();
    void checkAvailability();
    void bookTicket();
    void myBookings();
    void pnrStatus();
    void cancelTicket();
    void myProfile();

    // Shared booking flow once train, date and stations are known.
    void bookFlow(const std::string& trainNumber, const std::string& trainName, const Date& date,
                  const std::string& source, const std::string& destination);

    std::optional<std::string> readStation(const std::string& prompt);
    std::optional<Date> readDate(const std::string& prompt);
    std::optional<ClassCode> readClass();
    std::optional<Traveller> readTraveller(int position);
    std::string trainName(const std::string& trainNumber);

    ConsoleIO& io_;
    PassengerPorts ports_;
    Session session_;
    const IClock& clock_;
    RequestIdFactory newRequestId_;
    std::vector<std::string> screens_;
    bool inputEnded_ = false;
};

// Random 32-hex-digit request token for BookingRequest::requestId.
std::string randomRequestId();

}  // namespace rrs
