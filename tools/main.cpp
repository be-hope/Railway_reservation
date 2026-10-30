// main.cpp
// Sprint 3 – Application integration entry point
// Closes #43 (Integrate passenger, train and booking modules)
//
// NFR-005 : Single entry point that wires all modules together.
// ST-BKG-01: Login succeeds for a registered user; role drives menu routing.
//
// Usage: rrs_app [db-path]
//   db-path defaults to rrs.db in the current directory.
//
// Flow:
//   1. Open / seed the SQLite database.
//   2. Show a login prompt (username + password).
//   3. Route to the appropriate role menu:
//       Passenger  → PassengerMenu
//       Staff      → StaffMenu  (simple text placeholder for now)
//       Admin      → AdminMenu  (TrainAdminService + ScheduleCoachService)
//       Ops        → OpsMenu    (placeholder)
//   4. On logout, return to step 2.  On exit, quit.

#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

// ── Core domain / infrastructure ─────────────────────────────
#include "common/audit.h"
#include "common/clock.h"
#include "common/domain.h"
#include "common/result.h"
#include "common/session.h"

// ── DAL ──────────────────────────────────────────────────────
#include "dal/sqlite_data_store.h"
#include "dal/repositories.h"

// ── Business services ─────────────────────────────────────────
#include "booking/booking_service.h"
#include "booking/seat_release_service.h"
#include "admin/schedule_coach_service.h"
#include "admin/train_admin_service.h"

// ── UI ────────────────────────────────────────────────────────
#include "ui/passenger_menu.h"

// ── Demo adapters (interim port implementations) ──────────────
#include "demo_adapters.h"   // RepositoryTrainSearch, RepositoryBookingHistory,
                              // RepositoryProfile, InterimCancellation

namespace rrs {

// ─────────────────────────────────────────────────────────────
// Utility helpers
// ─────────────────────────────────────────────────────────────

namespace {

/// Read a single line from stdin; returns std::nullopt on EOF.
std::optional<std::string> readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return std::nullopt;
    return line;
}

/// Minimal "masked" password reader.  On a real terminal we would disable
/// echo; this is adequate for a console demo build.
std::string readPassword(const std::string& prompt) {
    std::cout << prompt;
    std::string pw;
    if (!std::getline(std::cin, pw)) return {};
    return pw;
}

void clearScreen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

void printBanner() {
    std::cout << "\n"
              << "╔══════════════════════════════════════════╗\n"
              << "║     Railway Reservation System  v3.0    ║\n"
              << "╚══════════════════════════════════════════╝\n\n";
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────
// Minimal admin console (FR-021, #55)
// ─────────────────────────────────────────────────────────────

class AdminMenu {
public:
    AdminMenu(TrainAdminService& trainAdmin, ScheduleCoachService& sched)
        : trainAdmin_(trainAdmin), sched_(sched) {}

    void run(Session& session) {
        while (true) {
            std::cout << "\n── Admin Menu ──────────────────────────────\n"
                      << "  1. Add train\n"
                      << "  2. Update train name\n"
                      << "  3. Deactivate train\n"
                      << "  4. Activate train\n"
                      << "  5. Show train details\n"
                      << "  6. Set route (replace all stops)\n"
                      << "  7. Show route\n"
                      << "  8. Add schedule date\n"
                      << "  9. Remove schedule date\n"
                      << " 10. List schedule dates\n"
                      << "  0. Logout\n"
                      << "────────────────────────────────────────────\n";

            auto choice = readLine("Choice: ");
            if (!choice) break;

            if (*choice == "0") break;

            if (*choice == "1") {
                auto num  = readLine("Train number: "); if (!num)  continue;
                auto name = readLine("Train name  : "); if (!name) continue;
                Train t; t.number = *num; t.name = *name; t.active = true;
                Status s = trainAdmin_.addTrain(session, t);
                std::cout << (s.ok() ? "Train added.\n" : "Error: " + s.message() + "\n");

            } else if (*choice == "2") {
                auto num  = readLine("Train number  : "); if (!num)  continue;
                auto name = readLine("New name      : "); if (!name) continue;
                Status s = trainAdmin_.updateTrainName(session, *num, *name);
                std::cout << (s.ok() ? "Name updated.\n" : "Error: " + s.message() + "\n");

            } else if (*choice == "3") {
                auto num = readLine("Train number: "); if (!num) continue;
                Status s = trainAdmin_.deactivateTrain(session, *num);
                std::cout << (s.ok() ? "Train deactivated.\n" : "Error: " + s.message() + "\n");

            } else if (*choice == "4") {
                auto num = readLine("Train number: "); if (!num) continue;
                Status s = trainAdmin_.activateTrain(session, *num);
                std::cout << (s.ok() ? "Train activated.\n" : "Error: " + s.message() + "\n");

            } else if (*choice == "5") {
                auto num = readLine("Train number: "); if (!num) continue;
                auto r = trainAdmin_.getTrain(session, *num);
                if (r.ok()) {
                    const Train& t = r.value();
                    std::cout << "Number : " << t.number << "\n"
                              << "Name   : " << t.name   << "\n"
                              << "Active : " << (t.active ? "yes" : "no") << "\n"
                              << "Stops  : " << t.stops.size() << "\n";
                } else {
                    std::cout << "Error: " << r.error().message() << "\n";
                }

            } else if (*choice == "6") {
                auto num = readLine("Train number: "); if (!num) continue;
                auto nStr = readLine("Number of stops: "); if (!nStr) continue;
                int n = 0;
                try { n = std::stoi(*nStr); } catch (...) { std::cout << "Invalid.\n"; continue; }
                std::vector<RouteStop> stops;
                for (int i = 0; i < n; ++i) {
                    RouteStop rs;
                    rs.seq = i + 1;
                    auto code = readLine("  Stop " + std::to_string(i+1) + " station code : ");
                    if (!code) break;
                    rs.stationCode = *code;
                    auto dist = readLine("  Cumulative distance (km) : ");
                    if (!dist) break;
                    try { rs.distanceKm = std::stoi(*dist); } catch (...) { rs.distanceKm = 0; }
                    auto arr = readLine("  Arrival minutes from origin : ");
                    if (!arr) break;
                    try { rs.arrivalMinutes = std::stoi(*arr); } catch (...) {}
                    auto dep = readLine("  Departure minutes from origin: ");
                    if (!dep) break;
                    try { rs.departureMinutes = std::stoi(*dep); } catch (...) {}
                    rs.dayOffset = 0;
                    stops.push_back(rs);
                }
                Status s = trainAdmin_.setRoute(session, *num, stops);
                std::cout << (s.ok() ? "Route updated.\n" : "Error: " + s.message() + "\n");

            } else if (*choice == "7") {
                auto num = readLine("Train number: "); if (!num) continue;
                auto r = trainAdmin_.getRoute(session, *num);
                if (r.ok()) {
                    for (const RouteStop& rs : r.value()) {
                        std::cout << "  " << rs.seq << ". " << rs.stationCode
                                  << "  arr=" << rs.arrivalMinutes
                                  << " dep=" << rs.departureMinutes
                                  << " dist=" << rs.distanceKm << "km\n";
                    }
                    if (r.value().empty()) std::cout << "  (no stops set)\n";
                } else {
                    std::cout << "Error: " << r.error().message() << "\n";
                }

            } else if (*choice == "8") {
                auto num  = readLine("Train number     : "); if (!num)  continue;
                auto dStr = readLine("Date (YYYY-MM-DD): "); if (!dStr) continue;
                auto date = Date::parse(*dStr);
                if (!date) { std::cout << "Invalid date format.\n"; continue; }
                Status s = trainAdmin_.addSchedule(session, *num, *date);
                std::cout << (s.ok() ? "Schedule added.\n" : "Error: " + s.message() + "\n");

            } else if (*choice == "9") {
                auto num  = readLine("Train number     : "); if (!num)  continue;
                auto dStr = readLine("Date (YYYY-MM-DD): "); if (!dStr) continue;
                auto date = Date::parse(*dStr);
                if (!date) { std::cout << "Invalid date format.\n"; continue; }
                Status s = trainAdmin_.removeSchedule(session, *num, *date);
                std::cout << (s.ok() ? "Schedule removed.\n" : "Error: " + s.message() + "\n");

            } else if (*choice == "10") {
                auto num = readLine("Train number: "); if (!num) continue;
                auto r = trainAdmin_.listSchedules(session, *num);
                if (r.ok()) {
                    for (const Date& d : r.value())
                        std::cout << "  " << d.toString() << "\n";
                    if (r.value().empty()) std::cout << "  (no future schedules)\n";
                } else {
                    std::cout << "Error: " << r.error().message() << "\n";
                }
            } else {
                std::cout << "Unknown option.\n";
            }
        }
        std::cout << "Admin logged out.\n";
    }

private:
    TrainAdminService& trainAdmin_;
    ScheduleCoachService& sched_;
};

// ─────────────────────────────────────────────────────────────
// Placeholder menus for Staff / Ops roles
// ─────────────────────────────────────────────────────────────

void runStaffMenu(Session& session) {
    std::cout << "\n── Staff Menu ──────────────────────────────\n"
              << "  [Placeholder — staff functions to be implemented in a future sprint]\n"
              << "  Press ENTER to logout.\n";
    std::string dummy;
    std::getline(std::cin, dummy);
    std::cout << "Staff logged out.\n";
}

void runOpsMenu(Session& session) {
    std::cout << "\n── Operations Menu ─────────────────────────\n"
              << "  [Placeholder — ops functions to be implemented in a future sprint]\n"
              << "  Press ENTER to logout.\n";
    std::string dummy;
    std::getline(std::cin, dummy);
    std::cout << "Ops logged out.\n";
}

// ─────────────────────────────────────────────────────────────
// Application entry point
// ─────────────────────────────────────────────────────────────

int appMain(const std::string& dbPath) {
    // 1. Open database ────────────────────────────────────────
    SqliteDataStore store;
    Status opened = store.open(dbPath);
    if (!opened.ok()) {
        std::cerr << "Cannot open database at '" << dbPath << "': "
                  << opened.message() << "\n";
        return 1;
    }
    DataAccess data = store.dataAccess();

    // Seed reference data if the database is empty.
    store.seedIfEmpty();

    // 2. Infrastructure singletons ────────────────────────────
    SystemClock clock;
    DatabaseAuditLogger audit(data);

    // 3. Business services ────────────────────────────────────
    BookingService        booking(data, clock, audit);
    SeatReleaseService    seatRelease(data, clock, audit);
    ScheduleCoachService  schedCoach(data, clock, audit);
    TrainAdminService     trainAdmin(data, clock, audit);

    // 4. Port adapters for PassengerMenu ──────────────────────
    RepositoryTrainSearch    trainSearch(data);
    RepositoryBookingHistory bookingHistory(data);
    RepositoryProfile        profile(data);
    InterimCancellation      cancellation(data, clock, audit);

    PassengerPorts passengerPorts{
        booking,
        trainSearch,
        bookingHistory,
        profile,
        &cancellation
    };

    // 5. Admin console ────────────────────────────────────────
    AdminMenu adminMenu(trainAdmin, schedCoach);

    // 6. Main login loop ──────────────────────────────────────
    printBanner();

    while (true) {
        std::cout << "── Login ───────────────────────────────────\n";
        auto usernameOpt = readLine("Username (or 'exit'): ");
        if (!usernameOpt || *usernameOpt == "exit") {
            std::cout << "Goodbye.\n";
            break;
        }
        std::string password = readPassword("Password          : ");

        // Authenticate against the user repository.
        Result<Session> sessionResult = data.users.authenticate(*usernameOpt, password);
        if (!sessionResult.ok()) {
            std::cout << "Login failed: " << sessionResult.error().message() << "\n\n";
            continue;
        }

        Session session = sessionResult.value();
        std::cout << "\nWelcome, " << session.displayName
                  << " [" << roleName(session.role) << "]\n\n";

        // Route to the appropriate menu.
        switch (session.role) {
            case Role::Passenger: {
                PassengerMenu menu(passengerPorts);
                menu.run(session);
                break;
            }
            case Role::Admin:
                adminMenu.run(session);
                break;
            case Role::Staff:
                runStaffMenu(session);
                break;
            case Role::Ops:
                runOpsMenu(session);
                break;
        }
    }

    return 0;
}

} // namespace rrs

// ─────────────────────────────────────────────────────────────
// C main()
// ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::string dbPath = (argc >= 2) ? argv[1] : "rrs.db";
    try {
        return rrs::appMain(dbPath);
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "Fatal unknown error.\n";
        return 3;
    }
}
