// passenger_demo.cpp
// Runs the passenger menu on a demo database seeded with the STP §6 data,
// logged in as passenger P1 (Asha Rao). Development/demo tool only - the real
// entry point with login is part of module integration (#43).
//
// Usage: rrs_passenger_demo [database-file]   (default: rrs_demo.db)

#include <iostream>

#include "booking/booking_service.h"
#include "common/audit.h"
#include "dal/sqlite_repositories.h"
#include "demo_adapters.h"
#include "seed_data.h"
#include "ui/passenger_menu.h"

int main(int argc, char** argv) {
    using namespace rrs;
    const std::string path = argc > 1 ? argv[1] : "rrs_demo.db";

    auto store = SqliteDataStore::open(path);
    if (!store.ok()) {
        std::cerr << formatError(store.error()) << "\n";
        return 1;
    }
    DataAccess data = store.value()->access();
    SystemClock clock;

    auto existing = data.users.findById(seed::kP1);
    if (existing.ok() && !existing.value()) {
        Status seeded = seed::loadSeedData(data, clock.today());
        if (!seeded.ok()) {
            std::cerr << formatError(seeded.error()) << "\n";
            return 1;
        }
        std::cout << "OK: demo database created at " << path << "\n";
    }

    AppendOnlyFileAuditLogger audit("audit.log", clock);
    PnrGenerator pnr;
    BookingService booking(data, clock, audit, pnr);
    demo::RepositoryTrainSearch trains(data.trains);
    demo::RepositoryBookingHistory history(data.reservations, clock);
    demo::RepositoryProfile profile(data.users, clock);
    SeatReleaseService release(data, clock, audit);
    demo::InterimCancellation cancellation(booking, release);

    Session session;
    session.sessionId = "demo";
    session.accountId = seed::kP1;
    session.role = Role::Passenger;
    session.expiresAt = clock.now() + 8 * 3600;   // demo session, no idle timeout

    ConsoleIO io(std::cin, std::cout);
    io.line("Railway Reservation System - passenger demo (logged in as Asha Rao)");
    PassengerMenu menu(io, PassengerPorts{booking, trains, history, profile, &cancellation}, session, clock,
                       randomRequestId);
    menu.run();
    return 0;
}
