#include "management_menu.h"
#include "ui/report_view.h"
#include "ui/ticket_view.h"
namespace rrs {
namespace {
std::string text(ConsoleIO& io, const std::string& prompt) { return io.readLine(prompt).value_or(""); }
std::optional<Date> date(ConsoleIO& io) { auto d = Date::parseDisplay(text(io, "Date DD-MM-YYYY: ")); if (!d) io.line("Invalid date."); return d; }
void show(ConsoleIO& io, const Status& s) { io.line(s.ok() ? "OK: change saved." : formatError(s.error())); }
void report(ConsoleIO& io, ReportService& reports, const Session& s, const IClock& clock) {
    auto stats = reports.bookingStatistics(s, {clock.today(), clock.today().plusDays(120)});
    if (stats.ok()) io.line(formatBookingStatistics(stats.value())); else io.line(formatError(stats.error()));
    auto rows = reports.trainWiseCounts(s, {clock.today(), clock.today().plusDays(120)});
    if (rows.ok()) io.line(formatTrainWiseCounts(rows.value(), {clock.today(), clock.today().plusDays(120)})); else io.line(formatError(rows.error()));
}
}
void runManagementMenu(ConsoleIO& io, const Session& s, const DataAccess& data, const IClock& clock, IAuditLogger& audit, IBookingService& booking, CancellationService& cancellation) {
    ReservationAdminService corrections(data, booking, clock, audit);
    TrainAdminService trains(data, clock, audit); ScheduleCoachService schedules(data, clock, audit); ReportService reports(data, clock);
    while (true) {
        io.line("[" + roleString(s.role) + "] Management");
        if (s.role == Role::Admin) io.line("1 Add train  2 Rename  3 Deactivate  4 Activate  5 Set route\n6 Add schedule  7 Remove schedule  8 Configure class\n9 Reports  10 List trains  11 Occupancy  12 View PNR  13 Correct name  14 Cancel ticket  0 Logout");
        else io.line("9 Reports  11 Occupancy  12 View PNR  13 Correct name  14 Cancel ticket  0 Logout");
        auto choice = io.readInt("Choice: ", 0, 14); if (!choice || *choice == 0) break;
        auto allowed = requireRole(s, {Role::Admin, Role::Ops}, clock); if (!allowed.ok()) { io.line(formatError(allowed.error())); break; }
        if (*choice == 9) { report(io, reports, s, clock); continue; }
        if (*choice == 11) {
            auto n = text(io, "Train: "); auto d = date(io); if (!d) continue;
            auto rows = reports.seatOccupancy(s, n, *d);
            io.line(rows.ok() ? formatSeatOccupancy(rows.value()) : formatError(rows.error())); continue;
        }
        if (s.role != Role::Admin) { io.line("ERROR: administrator permission required."); continue; }
        if (*choice >= 12) {
            auto pnr = text(io, "PNR: ");
            if (*choice == 14) {
                auto cancelled = cancellation.cancel(s, pnr);
                io.line(cancelled.ok() ? "OK: cancellation saved; refund " + cancelled->refund->toString() : formatError(cancelled.error()));
            } else {
                auto ticket = booking.getTicket(s, pnr);
                if (!ticket.ok()) { io.line(formatError(ticket.error())); continue; }
                if (*choice == 12) io.line(formatTicket(ticket.value(), ""));
                else {
                    auto travellers = ticket->travellers;
                    if (travellers.empty()) continue;
                    travellers[0].name = text(io, "Corrected first traveller name: ");
                    auto corrected = corrections.correctReservation(s, pnr, travellers);
                    io.line(corrected.ok() ? "OK: reservation corrected." : formatError(corrected.error()));
                }
            }
            continue;
        }
        if (*choice == 10) {
            auto rows = trains.listTrains(s);
            if (!rows.ok()) { io.line(formatError(rows.error())); continue; }
            for (const auto& t : rows.value()) io.line(t.number + " " + t.name + (t.active ? " ACTIVE" : " INACTIVE"));
            continue;
        }
        auto n = text(io, "Train: ");
        if (*choice == 1) { Train t; t.number = n; t.name = text(io, "Name: "); show(io, trains.addTrain(s, t)); }
        else if (*choice == 2) show(io, trains.updateTrainName(s, n, text(io, "New name: ")));
        else if (*choice == 3) show(io, trains.deactivateTrain(s, n));
        else if (*choice == 4) show(io, trains.activateTrain(s, n));
        else if (*choice == 5) {
            auto count = io.readInt("Number of stops (2-100): ", 2, 100); if (!count) break;
            std::vector<RouteStop> route;
            for (int i = 0; i < *count; ++i) {
                RouteStop stop; stop.seq = i + 1; stop.stationCode = text(io, "Station code: ");
                auto km = io.readInt("Cumulative km: ", 0, 100000);
                auto day = io.readInt("Day offset (0=first day): ", 0, 30);
                auto arr = io.readInt("Arrival minutes 0-1439 (-1 at origin): ", -1, 1439);
                auto dep = io.readInt("Departure minutes 0-1439 (-1 at terminus): ", -1, 1439);
                if (!km || !day || !arr || !dep) return;
                stop.distanceKm = *km; stop.dayOffset = *day; stop.arrivalMinutes = *arr; stop.departureMinutes = *dep; route.push_back(stop);
            }
            show(io, trains.setRoute(s, n, route));
        } else if (*choice == 6 || *choice == 7) {
            auto d = date(io); if (!d) continue;
            show(io, *choice == 6 ? trains.addSchedule(s, n, *d) : trains.removeSchedule(s, n, *d));
        } else if (*choice == 8) {
            auto cls = parseClassCode(text(io, "Class SL/3A: ")); if (!cls) { io.line("Invalid class."); continue; }
            auto coaches = io.readInt("Coaches: ", 1, 30); auto seats = io.readInt("Seats per coach: ", 1, 99);
            auto rac = io.readInt("RAC quota: ", 0, 2970); auto wl = io.readInt("Waitlist capacity: ", 0, 999);
            auto multiplier = io.readInt("Fare multiplier percent (100=1x): ", 1, 1000);
            if (!coaches || !seats || !rac || !wl || !multiplier) break;
            ClassConfig c; c.trainNumber = n; c.classCode = *cls; c.coaches = *coaches; c.seatsPerCoach = *seats;
            c.racQuota = *rac; c.waitlistCapacity = *wl; c.fareMultiplierPercent = *multiplier;
            auto changed = schedules.configureClass(s, c); io.line(changed.ok() ? "OK: configuration saved." : formatError(changed.error()));
        }
    }
}
}
