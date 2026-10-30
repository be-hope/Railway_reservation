// Development demo. Test credentials are enabled only on explicit --seed-demo.
#include <iostream>
#include <string>
#include "auth/auth_service.h"
#include "auth/profile_service.h"
#include "booking/booking_service.h"
#include "admin/reservation_admin_service.h"
#include "dal/retention.h"
#include "dal/sqlite_repositories.h"
#include "demo_adapters.h"
#include "seed_data.h"
#include "ui/passenger_menu.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace {
std::string read(const std::string& prompt, bool secret = false) {
    std::cout << prompt << std::flush;
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    bool terminal = secret && GetConsoleMode(input, &mode);
    if (terminal) SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT);
#else
    termios mode{};
    bool terminal = secret && tcgetattr(STDIN_FILENO, &mode) == 0;
    if (terminal) { termios hidden = mode; hidden.c_lflag &= ~ECHO; tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden); }
#endif
    std::string value;
    std::getline(std::cin, value);
#ifdef _WIN32
    if (terminal) SetConsoleMode(input, mode);
#else
    if (terminal) tcsetattr(STDIN_FILENO, TCSAFLUSH, &mode);
#endif
    if (secret) std::cout << '\n';
    return value;
}
}
int main(int argc, char** argv) {
    using namespace rrs;
    std::string path = "rrs_demo.db";
    bool seedDemo = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--seed-demo") seedDemo = true;
        else path = argv[i];
    }
    auto store = SqliteDataStore::open(path);
    if (!store.ok()) { std::cerr << formatError(store.error()) << '\n'; return 1; }
    auto data = store.value()->access();
    SystemClock clock;
    if (seedDemo) {
        auto existing = data.users.findById(seed::kP1);
        if (!existing.ok()) { std::cerr << formatError(existing.error()) << '\n'; return 1; }
        if (!existing.value()) {
            auto seeded = seed::loadSeedData(data, clock.today(), 121, true);
            if (!seeded.ok()) { std::cerr << formatError(seeded.error()) << '\n'; return 1; }
        }
        std::cout << "Demo accounts only: 9876500001 / staff01 / admin01 / ops01; password Test@1234\n";
    }
    auto retained = clearExpiredTravellerIds(store.value()->database(), clock.today());
    if (!retained.ok()) { std::cerr << formatError(retained.error()) << '\n'; return 1; }
    AppendOnlyFileAuditLogger audit("audit.log", clock);
    AuthService auth(data, clock, audit);
    PnrGenerator pnr;
    BookingService booking(data, clock, audit, pnr);
    demo::RepositoryTrainSearch trains(data.trains);
    BookingHistoryService history(data.reservations, clock, audit);
    ProfileService profile(data.users, clock, audit);
    ReservationAdminService staff(data, booking, clock, audit);
    ConsoleIO io(std::cin, std::cout);
    while (std::cin) {
        std::cout << "\nRailway Reservation System\n1 Register  2 Login  0 Exit\n";
        auto choice = read("Choice: ");
        if (!std::cin || choice == "0") break;
        if (choice == "1") {
            Registration request;
            request.name = read("Name: "); request.mobile = read("Mobile (or blank): ");
            request.email = read("Email (or blank): "); request.password = read("Password: ", true);
            auto result = auth.registerPassenger(request);
            std::cout << (result.ok() ? "OK: registered. Please log in." : formatError(result.error())) << '\n';
        } else if (choice == "2") {
            auto id = read("Login ID: "); auto password = read("Password: ", true);
            auto result = auth.login(id, password);
            if (!result.ok()) { std::cout << formatError(result.error()) << '\n'; continue; }
            Session session = result.value();
            if (session.role == Role::Passenger) {
                // Cancellation is supplied by the cancellation module when integrated.
                PassengerMenu menu(io, PassengerPorts{booking, trains, history, profile, nullptr}, session, clock, randomRequestId);
                menu.run();
            } else {
                while (std::cin) {
                    std::cout << "Role: " << roleString(session.role) << "\n1 View PNR  2 Correct traveller name  3 Book for passenger  0 Logout\n";
                    auto action = read("Choice: "); if (!std::cin || action == "0") break;
                    if (action == "1" || action == "2") {
                        auto ticket = booking.getTicket(session, read("PNR: "));
                        if (!ticket.ok()) { std::cout << formatError(ticket.error()) << '\n'; continue; }
                        if (action == "1") std::cout << ticket->pnr << " " << ticketStatusString(ticket->status) << '\n';
                        else {
                            auto travellers = ticket->travellers;
                            if (travellers.empty()) continue;
                            travellers[0].name = read("Corrected first traveller name: ");
                            auto corrected = staff.correctReservation(session, ticket->pnr, travellers);
                            std::cout << (corrected.ok() ? "OK: reservation corrected." : formatError(corrected.error())) << '\n';
                        }
                    } else if (action == "3") {
                        BookingRequest request;
                        request.requestId = randomRequestId(); request.onBehalfOf = read("Passenger account ID: ");
                        request.trainNumber = read("Train: "); request.sourceCode = read("Source: "); request.destinationCode = read("Destination: ");
                        auto date = Date::parseDisplay(read("Journey date DD-MM-YYYY: "));
                        auto cls = parseClassCode(read("Class SL/3A: "));
                        if (!date || !cls) { std::cout << "Invalid date or class.\n"; continue; }
                        request.journeyDate = *date; request.classCode = *cls;
                        Traveller traveller; traveller.name = read("Traveller name: ");
                        auto age = io.readInt("Age: ", 1, 120); if (!age) continue;
                        traveller.age = *age;
                        auto gender = read("Gender M/F/O: "); traveller.gender = gender.size() == 1 ? gender[0] : '?';
                        traveller.idProofNumber = read("ID proof: "); request.travellers.push_back(traveller);
                        auto quote = booking.quoteFare(request.trainNumber, request.sourceCode, request.destinationCode, request.classCode, 1);
                        if (!quote.ok()) { std::cout << formatError(quote.error()) << '\n'; continue; }
                        request.acceptedFare = quote->total;
                        request.fareAccepted = read("Fare " + quote->total.toString() + ". Accept y/n: ") == "y";
                        auto created = staff.bookOnBehalf(session, request);
                        std::cout << (created.ok() ? "OK: PNR " + created->pnr : formatError(created.error())) << '\n';
                    }
                }
            }
            auth.logout(session);
        }
    }
    return 0;
}
