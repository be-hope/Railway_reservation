// Private line protocol used by web/server.py. Not a network listener.
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include "auth/auth_service.h"
#include "auth/profile_service.h"
#include "booking/booking_service.h"
#include "cancel/cancellation_service.h"
#include "dal/retention.h"
#include "dal/sqlite_repositories.h"
#include "demo_adapters.h"
#include "seed_data.h"

using namespace rrs;
namespace {
std::string json(const std::string& s) {
    std::ostringstream o; o << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') o << '\\' << c;
        else if (c < 32) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        else o << c;
    }
    return o.str() + '"';
}
template<class T> T checked(Result<T> r) {
    if (!r.ok()) throw std::runtime_error(formatError(r.error()));
    return std::move(r.value());
}
std::string ticket(const Reservation& r) {
    std::string s = "{\"pnr\":" + json(r.pnr) + ",\"train\":" + json(r.trainNumber)
        + ",\"date\":" + json(r.journeyDate.iso()) + ",\"source\":" + json(r.sourceCode)
        + ",\"destination\":" + json(r.destinationCode) + ",\"classCode\":" + json(classCodeString(r.classCode))
        + ",\"status\":" + json(ticketStatusString(r.status)) + ",\"fare\":" + json(r.fare.toString())
        + ",\"refund\":" + (r.refund ? json(r.refund->toString()) : "null") + ",\"travellers\":[";
    for (const auto& t : r.travellers) {
        if (s.back() != '[') s += ',';
        s += "{\"name\":" + json(t.name) + ",\"seat\":" + json(t.seatNumber) + "}";
    }
    return s + "]}";
}
}
int main(int argc, char** argv) {
    try {
        auto store = checked(SqliteDataStore::open(argc > 1 ? argv[1] : "web-demo.db"));
        auto data = store->access(); SystemClock clock;
        if (argc > 2 && std::string(argv[2]) == "--seed-demo") {
            auto existing = checked(data.users.findById(seed::kP1));
            if (!existing) {
                auto result = seed::loadSeedData(data, clock.today(), 121, true);
                if (!result.ok()) throw std::runtime_error(formatError(result.error()));
            }
        }
        auto retained = clearExpiredTravellerIds(store->database(), clock.today());
        if (!retained.ok()) throw std::runtime_error(formatError(retained.error()));
        AppendOnlyFileAuditLogger audit("audit.log", clock);
        AuthService auth(data, clock, audit); PnrGenerator pnr;
        BookingService booking(data, clock, audit, pnr);
        BookingHistoryService history(data.reservations, clock, audit);
        demo::RepositoryTrainSearch trains(data.trains, &clock);
        SeatReleaseService release(data, clock, audit);
        CancellationService cancellation(booking, data.trains, release, clock);
        std::map<std::string, Session> sessions;
        std::cout << "{\"ready\":true}" << std::endl;
        for (std::string line; std::getline(std::cin, line);) {
            try {
                std::istringstream in(line);
                auto field = [&]() { std::string s; if (!(in >> std::quoted(s))) throw std::runtime_error("Missing request field"); return s; };
                const auto action = field(); const auto token = field();
                std::string out;
                if (action == "login") {
                    const auto id = field(); const auto password = field();
                    auto session = checked(auth.login(id, password));
                    if (session.role != Role::Passenger) {
                        auth.logout(session);
                        throw std::runtime_error("This frontend is for passengers. Use the console for staff and administration.");
                    }
                    if (sessions.count(token)) auth.logout(sessions.at(token));
                    sessions.insert_or_assign(token, session);
                    out = "{\"login\":" + json(id) + "}";
                } else if (action == "register") {
                    Registration r; r.name=field(); r.mobile=field(); r.email=field(); r.password=field();
                    checked(auth.registerPassenger(r)); out = "{}";
                } else {
                    auto found = sessions.find(token);
                    if (found == sessions.end()) throw std::runtime_error("Please log in to continue.");
                    const auto& session = found->second;
                    auto access = requireRole(session, {Role::Passenger}, clock);
                    if (!access.ok()) throw std::runtime_error(formatError(access.error()));
                    if (action == "logout") { auth.logout(session); sessions.erase(found); out="{}"; }
                    else if (action == "history") {
                        out="[";
                        for (const auto& r : checked(history.history(session))) { if (out.size()>1) out+=','; out+=ticket(r); }
                        out+=']';
                    } else if (action == "ticket" || action == "cancel") {
                        auto number=field();
                        out=ticket(action=="ticket" ? checked(booking.getTicket(session, number)) : checked(cancellation.cancel(session, number)));
                    } else if (action == "search" || action == "quote" || action == "book") {
                        const auto source=field(); const auto destination=field();
                        auto date=Date::parseIso(field()); auto cls=parseClassCode(field());
                        if (!date || !cls) throw std::runtime_error("Enter a valid date and class.");
                        if (action == "search") {
                            out="[";
                            for (const auto& t : checked(trains.search(source, destination, *date))) {
                                auto availability=booking.checkAvailability(t.trainNumber, *date, *cls);
                                auto fare=booking.quoteFare(t.trainNumber, source, destination, *cls, 1);
                                if (!availability.ok() || !fare.ok()) continue;
                                if (out.size()>1) out+=',';
                                out+="{\"number\":"+json(t.trainNumber)+",\"name\":"+json(t.trainName)+",\"departure\":"+json(t.departure)
                                    +",\"arrival\":"+json(t.arrival)+",\"duration\":"+json(t.duration)+",\"seats\":"+std::to_string(availability->confirmedFree)
                                    +",\"rac\":"+std::to_string(availability->racFree)+",\"fare\":"+json(fare->total.toString())+"}";
                            }
                            out+=']';
                        } else {
                            const auto train=field(); const int count=std::stoi(field());
                            auto fare=checked(booking.quoteFare(train, source, destination, *cls, count));
                            if (action == "quote") out="{\"fare\":"+json(fare.total.toString())+",\"paise\":"+std::to_string(fare.total.paise())+"}";
                            else {
                                BookingRequest r; r.sourceCode=source; r.destinationCode=destination; r.journeyDate=*date;
                                r.classCode=*cls; r.trainNumber=train; r.requestId=field(); r.fareAccepted=true;
                                r.acceptedFare=Money::fromPaise(std::stoll(field()));
                                if (count<1 || count>6) throw std::runtime_error("Enter between 1 and 6 travellers.");
                                for (int i=0;i<count;++i) {
                                    Traveller t; t.name=field(); t.age=std::stoi(field()); const auto gender=field();
                                    t.gender=gender.size()==1 ? gender[0] : '?'; t.idProofNumber=field(); r.travellers.push_back(t);
                                }
                                out=ticket(checked(booking.createBooking(session,r)));
                            }
                        }
                    } else throw std::runtime_error("Unknown action.");
                }
                std::cout << "{\"ok\":true,\"data\":" << out << "}" << std::endl;
            } catch (const std::exception& e) { std::cout << "{\"ok\":false,\"error\":" << json(e.what()) << "}" << std::endl; }
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
