// seed_data.cpp

#include "seed_data.h"

#include <vector>
#include "auth/password_hasher.h"

namespace rrs::seed {

namespace {

int at(int hour, int minute) { return hour * 60 + minute; }

RouteStop stop(int seq, const char* code, int arrival, int departure, int km, int dayOffset = 0) {
    RouteStop s;
    s.seq = seq;
    s.stationCode = code;
    s.arrivalMinutes = arrival;
    s.departureMinutes = departure;
    s.distanceKm = km;
    s.dayOffset = dayOffset;
    return s;
}

ClassConfig cls(const char* train, ClassCode code, int coaches, int seats, int rac, int wl) {
    ClassConfig c;
    c.trainNumber = train;
    c.classCode = code;
    c.coaches = coaches;
    c.seatsPerCoach = seats;
    c.racQuota = rac;
    c.waitlistCapacity = wl;
    c.fareMultiplierPercent = code == ClassCode::SL ? 100 : 250;
    return c;
}

Account account(const char* id, Role role, const char* login, const char* name,
                const char* mobile) {
    Account a;
    a.accountId = id;
    a.role = role;
    a.loginId = login;
    a.name = name;
    a.contactNumber = mobile;
    a.passwordHash = "!";   // login disabled - see header note
    a.salt = "!";
    return a;
}

}  // namespace

Status loadSeedData(DataAccess& data, const Date& firstDay, int scheduleDays, bool enableDemoLogins) {
    TransactionGuard tx(data.unitOfWork);
    if (!tx.started().ok()) return tx.started();

    const std::vector<Station> stations = {
        {"SBC", "Bengaluru City"}, {"KPD", "Katpadi Jn"}, {"MAS", "Chennai Central"},
        {"MYS", "Mysuru Jn"},      {"SC", "Secunderabad Jn"}};
    for (const Station& s : stations) {
        Status st = data.trains.saveStation(s);
        if (!st.ok()) return st;
    }

    const std::vector<Train> trains = {
        {"20001", "Bengaluru-Chennai Express", true,
         {stop(1, "SBC", -1, at(6, 0), 0), stop(2, "KPD", at(9, 5), at(9, 10), 210),
          stop(3, "MAS", at(11, 30), -1, 350)}},
        {"20002", "Chennai-Bengaluru Express", true,
         {stop(1, "MAS", -1, at(14, 0), 0), stop(2, "KPD", at(16, 5), at(16, 10), 140),
          stop(3, "SBC", at(20, 30), -1, 350)}},
        {"20003", "Bengaluru-Mysuru Intercity", true,
         {stop(1, "SBC", -1, at(7, 0), 0), stop(2, "MYS", at(10, 0), -1, 140)}},
        {"20004", "Bengaluru-Hyderabad Express", true,
         {stop(1, "SBC", -1, at(18, 0), 0), stop(2, "SC", at(6, 30), -1, 570, 1)}},
    };
    for (const Train& t : trains) {
        Status st = data.trains.saveTrain(t);
        if (!st.ok()) return st;
    }

    const std::vector<ClassConfig> configs = {
        cls("20001", ClassCode::SL, 4, 72, 0, 20), cls("20001", ClassCode::AC3, 2, 64, 0, 20),
        cls("20002", ClassCode::SL, 4, 72, 0, 20), cls("20002", ClassCode::AC3, 2, 64, 0, 20),
        cls("20003", ClassCode::SL, 1, 2, 0, 2),   cls("20003", ClassCode::AC3, 1, 2, 1, 2),
        cls("20004", ClassCode::SL, 4, 72, 0, 20), cls("20004", ClassCode::AC3, 2, 64, 0, 20),
    };
    for (const ClassConfig& c : configs) {
        Status st = data.config.saveClassConfig(c);
        if (!st.ok()) return st;
    }

    for (const Train& t : trains) {
        for (int d = 0; d < scheduleDays; ++d) {
            Status st = data.trains.addSchedule(t.number, firstDay.plusDays(d));
            if (!st.ok()) return st;
        }
    }

    const std::vector<Account> accounts = {
        account(kP1, Role::Passenger, "9876500001", "Asha Rao", "9876500001"),
        account(kP2, Role::Passenger, "9876500002", "Ravi Kumar", "9876500002"),
        account(kS1, Role::Staff, "staff01", "Reservation Staff", ""),
        account(kA1, Role::Admin, "admin01", "System Administrator", ""),
        account(kO1, Role::Ops, "ops01", "Railway Operations", ""),
    };
    for (Account a : accounts) {
        if (enableDemoLogins) {
            auto digest = PasswordHasher::hash("Test@1234");
            if (!digest.ok()) return digest.error();
            a.passwordHash = digest->hash; a.salt = digest->salt;
        }
        Status st = data.users.insert(a);
        if (!st.ok()) return st;
    }
    return tx.commit();
}

}  // namespace rrs::seed
