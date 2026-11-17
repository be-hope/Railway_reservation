// passenger_menu.cpp

#include "ui/passenger_menu.h"

#include <cstdio>
#include <random>

#include "ui/ticket_view.h"

namespace rrs {

namespace {

constexpr int kMaxBookingRetries = 2;

std::string hhmm(int minutes) {
    if (minutes < 0) return "--:--";
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", minutes / 60, minutes % 60);
    return buf;
}

std::string pad(std::string s, std::size_t width) {
    if (s.size() < width) s.append(width - s.size(), ' ');
    return s;
}

}  // namespace

std::string randomRequestId() {
    static std::mt19937_64 engine{std::random_device{}()};
    static const char* const kHex = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 32; ++i) id += kHex[engine() % 16];
    return id;
}

PassengerMenu::PassengerMenu(ConsoleIO& io, PassengerPorts ports, Session session,
                             const IClock& clock, RequestIdFactory newRequestId)
    : io_(io),
      ports_(ports),
      session_(std::move(session)),
      clock_(clock),
      newRequestId_(std::move(newRequestId)) {}

void PassengerMenu::enter(const std::string& screen) {
    screens_.push_back(screen);
    io_.line();
    io_.line("[Passenger] " + screen);
    io_.rule();
}

void PassengerMenu::run() {
    while (!inputEnded_) {
        io_.line();
        io_.rule();
        io_.line("[Passenger] Main menu");
        io_.rule();
        io_.line("1 Search trains        2 Schedule, route & fare");
        io_.line("3 Check availability   4 Book ticket");
        io_.line("5 My bookings          6 PNR status");
        io_.line("7 Cancel ticket        8 My profile");
        io_.line("0 Logout");
        auto choice = io_.readInt("Choice: ", 0, 8);
        if (!choice || *choice == 0) break;
        auto access = requireRole(session_, {Role::Passenger}, clock_);
        if (!access.ok()) { io_.line(formatError(access.error())); break; }
        switch (*choice) {
            case 1: searchTrains(); break;
            case 2: showRoute(); break;
            case 3: checkAvailability(); break;
            case 4: bookTicket(); break;
            case 5: myBookings(); break;
            case 6: pnrStatus(); break;
            case 7: cancelTicket(); break;
            case 8: myProfile(); break;
            default: break;
        }
    }
    io_.ok("Logged out.");
}

// ---------------------------------------------------------------- input helpers

std::optional<std::string> PassengerMenu::readStation(const std::string& prompt) {
    while (true) {
        auto text = io_.readLine(prompt);
        if (!text) { inputEnded_ = true; return std::nullopt; }
        std::string code = toUpperAscii(*text);
        bool ok = code.size() >= 2 && code.size() <= 5;
        for (char c : code) ok = ok && c >= 'A' && c <= 'Z';
        if (ok) return code;
        io_.line("Invalid station code: 2 to 5 letters, e.g. SBC. Please re-enter.");
    }
}

std::optional<Date> PassengerMenu::readDate(const std::string& prompt) {
    while (true) {
        auto text = io_.readLine(prompt);
        if (!text) { inputEnded_ = true; return std::nullopt; }
        auto date = Date::parseDisplay(*text);
        const Date today = clock_.today();
        if (date && *date >= today && *date <= today.plusDays(kBookingWindowDays)) return date;
        io_.line("Invalid date: use DD-MM-YYYY, from today to today + 120 days. Please re-enter.");
    }
}

std::optional<ClassCode> PassengerMenu::readClass() {
    while (true) {
        auto text = io_.readLine("Class (SL = Sleeper, 3A = AC 3-Tier): ");
        if (!text) { inputEnded_ = true; return std::nullopt; }
        if (auto c = parseClassCode(toUpperAscii(*text))) return c;
        io_.line("Invalid class: enter SL or 3A.");
    }
}

std::optional<Traveller> PassengerMenu::readTraveller(int position) {
    while (true) {
        io_.line("Traveller " + std::to_string(position) + ":");
        Traveller t;
        auto name = io_.readLine("  Name: ");
        if (!name) { inputEnded_ = true; return std::nullopt; }
        t.name = *name;
        auto age = io_.readInt("  Age (1-120): ", 1, 120);
        if (!age) { inputEnded_ = true; return std::nullopt; }
        t.age = *age;
        auto gender = io_.readLine("  Gender (M/F/O): ");
        if (!gender) { inputEnded_ = true; return std::nullopt; }
        std::string g = toUpperAscii(*gender);
        t.gender = g.size() == 1 ? g[0] : '?';
        auto id = io_.readLine("  ID proof number: ");
        if (!id) { inputEnded_ = true; return std::nullopt; }
        t.idProofNumber = *id;

        Status valid = validateTraveller(t, position);
        if (valid.ok()) return t;
        io_.error(formatError(valid.error()));
    }
}

std::string PassengerMenu::trainName(const std::string& trainNumber) {
    auto train = ports_.trains.route(trainNumber);
    return train.ok() ? train.value().name : std::string();
}

// ---------------------------------------------------------------- screens

void PassengerMenu::searchTrains() {
    enter("Search");
    auto source = readStation("From station code: ");
    if (!source) return;
    auto destination = readStation("To station code: ");
    if (!destination) return;
    auto date = readDate("Journey date (DD-MM-YYYY): ");
    if (!date) return;

    auto results = ports_.trains.search(*source, *destination, *date);
    if (!results.ok()) { io_.error(formatError(results.error())); return; }
    const auto& rows = results.value();
    if (rows.empty()) {
        io_.line("No trains found for this route and date.");
        return;
    }
    io_.line(" #  Train       Name                         Dep    Arr    Duration");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const TrainSummary& r = rows[i];
        io_.line(pad(std::to_string(i + 1), 3) + pad(r.trainNumber, 12) + pad(r.trainName.substr(0, 28), 29) +
                 pad(r.departure, 7) + pad(r.arrival, 7) + r.duration);
    }
    auto pick = io_.readInt("Select a train to book (0 = back): ", 0, static_cast<int>(rows.size()));
    if (!pick) { inputEnded_ = true; return; }
    if (*pick == 0) return;
    const TrainSummary& chosen = rows[static_cast<std::size_t>(*pick - 1)];
    bookFlow(chosen.trainNumber, chosen.trainName, *date, *source, *destination);
}

void PassengerMenu::bookTicket() {
    enter("Book ticket");
    auto number = io_.readLine("Train number: ");
    if (!number) { inputEnded_ = true; return; }
    auto date = readDate("Journey date (DD-MM-YYYY): ");
    if (!date) return;
    auto source = readStation("From station code: ");
    if (!source) return;
    auto destination = readStation("To station code: ");
    if (!destination) return;
    bookFlow(*number, trainName(*number), *date, *source, *destination);
}

void PassengerMenu::bookFlow(const std::string& trainNumber, const std::string& name, const Date& date,
                             const std::string& source, const std::string& destination) {
    // Step: select train and class -> availability and fare per traveller.
    enter("Select train and class");
    io_.line("Train " + trainNumber + (name.empty() ? "" : " " + name) + "  " + source + " -> " +
             destination + "  " + date.display());
    auto cls = readClass();
    if (!cls) return;
    auto availability = ports_.booking.checkAvailability(trainNumber, date, *cls);
    if (!availability.ok()) { io_.error(formatError(availability.error())); return; }
    io_.out() << formatAvailability(availability.value());
    auto single = ports_.booking.quoteFare(trainNumber, source, destination, *cls, 1);
    if (!single.ok()) { io_.error(formatError(single.error())); return; }
    io_.line("Fare per traveller: Rs. " + single.value().perTraveller.toString());

    // Step: enter travellers.
    enter("Enter travellers");
    auto count = io_.readInt("Number of travellers (1-6): ", kMinTravellers, kMaxTravellers);
    if (!count) { inputEnded_ = true; return; }
    BookingRequest request;
    request.trainNumber = trainNumber;
    request.journeyDate = date;
    request.sourceCode = source;
    request.destinationCode = destination;
    request.classCode = *cls;
    for (int i = 1; i <= *count; ++i) {
        auto t = readTraveller(i);
        if (!t) return;
        request.travellers.push_back(*t);
    }

    // Step: confirm fare (FR-012). Rejecting it creates nothing.
    enter("Confirm fare");
    auto quote = ports_.booking.quoteFare(trainNumber, source, destination, *cls, *count);
    if (!quote.ok()) { io_.error(formatError(quote.error())); return; }
    io_.out() << formatFareQuote(quote.value());
    auto accept = io_.readYesNo("Accept the fare and book? (Y/N): ");
    if (!accept) { inputEnded_ = true; return; }
    if (!*accept) {
        io_.ok("Fare not accepted. No booking was made.");
        return;
    }
    request.fareAccepted = true;
    request.acceptedFare = quote.value().total;
    request.requestId = newRequestId_();

    // A storage failure can be retried with the same request id: the service
    // returns the original reservation if the first attempt was saved (NFR-004).
    auto booked = ports_.booking.createBooking(session_, request);
    for (int retry = 0; !booked.ok() && booked.code() == ErrorCode::Database && retry < kMaxBookingRetries;
         ++retry) {
        io_.error(formatError(booked.error()));
        auto again = io_.readYesNo("Retry the same booking? (Y/N): ");
        if (!again || !*again) return;
        booked = ports_.booking.createBooking(session_, request);
    }
    if (!booked.ok()) { io_.error(formatError(booked.error())); return; }

    // Step: view ticket.
    enter("View ticket");
    io_.ok("Booking saved. Status: " + statusText(booked.value()) + ".");
    io_.out() << formatTicket(booked.value(), name);
}

void PassengerMenu::showRoute() {
    enter("Schedule, route & fare");
    auto number = io_.readLine("Train number: ");
    if (!number) { inputEnded_ = true; return; }
    auto train = ports_.trains.route(*number);
    if (!train.ok()) { io_.error(formatError(train.error())); return; }
    const Train& t = train.value();
    io_.line("Train " + t.number + " " + t.name);
    io_.line(" #  Station  Arr    Dep    Day  Km");
    for (const RouteStop& s : t.stops) {
        io_.line(pad(std::to_string(s.seq), 4) + pad(s.stationCode, 9) + pad(hhmm(s.arrivalMinutes), 7) +
                 pad(hhmm(s.departureMinutes), 7) + pad(std::to_string(s.dayOffset + 1), 5) +
                 std::to_string(s.distanceKm));
    }
    if (t.stops.size() < 2) return;
    const std::string& from = t.stops.front().stationCode;
    const std::string& to = t.stops.back().stationCode;
    for (ClassCode c : {ClassCode::SL, ClassCode::AC3}) {
        auto q = ports_.booking.quoteFare(t.number, from, to, c, 1);
        if (q.ok()) {
            io_.line("Fare " + from + " -> " + to + " " + className(c) + ": Rs. " +
                     q.value().perTraveller.toString());
        }
    }
}

void PassengerMenu::checkAvailability() {
    enter("Check availability");
    auto number = io_.readLine("Train number: ");
    if (!number) { inputEnded_ = true; return; }
    auto date = readDate("Journey date (DD-MM-YYYY): ");
    if (!date) return;
    auto cls = readClass();
    if (!cls) return;
    auto a = ports_.booking.checkAvailability(*number, *date, *cls);
    if (!a.ok()) { io_.error(formatError(a.error())); return; }
    io_.out() << formatAvailability(a.value());
}

void PassengerMenu::myBookings() {
    enter("My bookings");
    auto list = ports_.history.history(session_);
    if (!list.ok()) { io_.error(formatError(list.error())); return; }
    if (list.value().empty()) { io_.line("You have no bookings yet."); return; }
    const Date today = clock_.today();
    io_.line("PNR         Date        Train   Journey      Status                   When");
    for (const Reservation& r : list.value()) {
        io_.line(pad(r.pnr, 12) + pad(r.journeyDate.display(), 12) + pad(r.trainNumber, 8) +
                 pad(r.sourceCode + "->" + r.destinationCode, 13) + pad(statusText(r), 25) +
                 (r.journeyDate >= today ? "Upcoming" : "Past"));
    }
}

void PassengerMenu::pnrStatus() {
    enter("PNR status");
    auto pnr = io_.readLine("PNR (10 digits): ");
    if (!pnr) { inputEnded_ = true; return; }
    auto ticket = ports_.booking.getTicket(session_, *pnr);
    if (!ticket.ok()) { io_.error(formatError(ticket.error())); return; }
    io_.out() << formatTicket(ticket.value(), trainName(ticket.value().trainNumber));
}

void PassengerMenu::cancelTicket() {
    enter("Cancel ticket");
    if (ports_.cancellation == nullptr) {
        io_.line("Cancellation is not available yet.");
        return;
    }
    auto pnr = io_.readLine("PNR to cancel (10 digits): ");
    if (!pnr) { inputEnded_ = true; return; }
    auto sure = io_.readYesNo("Cancel this ticket? (Y/N): ");
    if (!sure) { inputEnded_ = true; return; }
    if (!*sure) return;
    auto cancelled = ports_.cancellation->cancel(session_, *pnr);
    if (!cancelled.ok()) { io_.error(formatError(cancelled.error())); return; }
    io_.ok("Ticket " + cancelled.value().pnr + " cancelled.");
    io_.out() << formatTicket(cancelled.value(), trainName(cancelled.value().trainNumber));
}

void PassengerMenu::myProfile() {
    enter("My profile");
    auto profile = ports_.profile.view(session_);
    if (!profile.ok()) { io_.error(formatError(profile.error())); return; }
    const Account& a = profile.value();
    io_.line("Name     : " + a.name);
    io_.line("Age      : " + (a.age > 0 ? std::to_string(a.age) : std::string("-")));
    io_.line("Gender   : " + std::string(1, a.gender));
    io_.line("Mobile   : " + a.contactNumber);
    io_.line("E-mail   : " + a.email);
    io_.line("ID proof : " + a.idProofType + " " + maskIdProof(a.idProofNumber));

    auto edit = io_.readYesNo("Edit profile? (Y/N): ");
    if (!edit) { inputEnded_ = true; return; }
    if (!*edit) return;
    Account changes = a;
    auto name = io_.readLine("Name (blank = keep): ");
    if (!name) { inputEnded_ = true; return; }
    if (!name->empty()) changes.name = *name;
    auto age = io_.readLine("Age 1-120 (blank = keep): ");
    if (!age) { inputEnded_ = true; return; }
    if (!age->empty()) {
        bool digits = age->size() <= 3;
        for (char c : *age) digits = digits && c >= '0' && c <= '9';
        changes.age = digits ? std::stoi(*age) : -1;
    }
    auto gender = io_.readLine("Gender M/F/O (blank = keep): ");
    if (!gender) { inputEnded_ = true; return; }
    if (!gender->empty()) changes.gender = toUpperAscii(*gender)[0];
    auto idType = io_.readLine("ID proof type (blank = keep): ");
    if (!idType) { inputEnded_ = true; return; }
    if (!idType->empty()) changes.idProofType = *idType;
    auto idNumber = io_.readLine("ID proof number (blank = keep): ");
    if (!idNumber) { inputEnded_ = true; return; }
    if (!idNumber->empty()) changes.idProofNumber = *idNumber;

    Status saved = ports_.profile.update(session_, changes);
    if (!saved.ok()) { io_.error(formatError(saved.error())); return; }
    io_.ok("Profile saved.");
}

}  // namespace rrs
