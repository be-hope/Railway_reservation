// seed_data.cpp

#include "seed_data.h"

#include <string>
#include <vector>

namespace rrs::seed {

namespace {

int at(int hour, int minute) {
    return hour * 60 + minute;
}

RouteStop stop(int seq, const char* code, int arrival, int departure,
               int km, int dayOffset = 0) {
    RouteStop s;
    s.seq = seq;
    s.stationCode = code;
    s.arrivalMinutes = arrival;
    s.departureMinutes = departure;
    s.distanceKm = km;
    s.dayOffset = dayOffset;
    return s;
}

ClassConfig cls(const char* train, ClassCode code, int coaches,
                int seats, int rac, int wl) {
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

Account account(const char* id, Role role, const char* login,
                const char* name, const char* mobile) {
    Account a;
    a.accountId = id;
    a.role = role;
    a.loginId = login;
    a.name = name;
    a.contactNumber = mobile;
    a.passwordHash = "!";
    a.salt = "!";
    return a;
}

// Inserts a seed account only if it does not already exist.
// Existing accounts are checked rather than overwritten.
Status ensureAccount(DataAccess& data, const Account& expected) {
    auto existing = data.users.findById(expected.accountId);

    if (!existing.ok()) {
        return existing.error();
    }

    if (existing.value()) {
        const Account& actual = *existing.value();

        if (actual.role != expected.role ||
            actual.loginId != expected.loginId ||
            actual.name != expected.name ||
            actual.contactNumber != expected.contactNumber) {
            return Status::failure(
                ErrorCode::DuplicateUser,
                "Seed account conflicts with existing account: " + expected.accountId);
        }

        return Status::success();
    }

    return data.users.insert(expected);
}

Status seedBaseData(DataAccess& data, const Date& firstDay, int scheduleDays) {
    const std::vector<Station> stations = {
        {"SBC", "Bengaluru City"},
        {"KPD", "Katpadi Jn"},
        {"MAS", "Chennai Central"},
        {"MYS", "Mysuru Jn"},
        {"SC", "Secunderabad Jn"}
    };

    for (const Station& s : stations) {
        Status result = data.trains.saveStation(s);
        if (!result.ok()) return result;
    }

    const std::vector<Train> trains = {
        {
            "20001", "Bengaluru-Chennai Express", true,
            {
                stop(1, "SBC", -1, at(6, 0), 0),
                stop(2, "KPD", at(9, 5), at(9, 10), 210),
                stop(3, "MAS", at(11, 30), -1, 350)
            }
        },
        {
            "20002", "Chennai-Bengaluru Express", true,
            {
                stop(1, "MAS", -1, at(14, 0), 0),
                stop(2, "KPD", at(16, 5), at(16, 10), 140),
                stop(3, "SBC", at(20, 30), -1, 350)
            }
        },
        {
            "20003", "Bengaluru-Mysuru Intercity", true,
            {
                stop(1, "SBC", -1, at(7, 0), 0),
                stop(2, "MYS", at(10, 0), -1, 140)
            }
        },
        {
            "20004", "Bengaluru-Hyderabad Express", true,
            {
                stop(1, "SBC", -1, at(18, 0), 0),
                stop(2, "SC", at(6, 30), -1, 570, 1)
            }
        }
    };

    for (const Train& t : trains) {
        Status result = data.trains.saveTrain(t);
        if (!result.ok()) return result;
    }

    const std::vector<ClassConfig> configs = {
        cls("20001", ClassCode::SL, 4, 72, 0, 20),
        cls("20001", ClassCode::AC3, 2, 64, 0, 20),
        cls("20002", ClassCode::SL, 4, 72, 0, 20),
        cls("20002", ClassCode::AC3, 2, 64, 0, 20),
        cls("20003", ClassCode::SL, 1, 2, 0, 2),
        cls("20003", ClassCode::AC3, 1, 2, 1, 2),
        cls("20004", ClassCode::SL, 4, 72, 0, 20),
        cls("20004", ClassCode::AC3, 2, 64, 0, 20)
    };

    for (const ClassConfig& c : configs) {
        Status result = data.config.saveClassConfig(c);
        if (!result.ok()) return result;
    }

    for (const Train& t : trains) {
        for (int d = 0; d < scheduleDays; ++d) {
            Status result = data.trains.addSchedule(
                t.number, firstDay.plusDays(d));

            if (!result.ok()) return result;
        }
    }

    const std::vector<Account> accounts = {
        account(kP1, Role::Passenger, "9876500001", "Asha Rao", "9876500001"),
        account(kP2, Role::Passenger, "9876500002", "Ravi Kumar", "9876500002"),
        account(kS1, Role::Staff, "staff01", "Reservation Staff", ""),
        account(kA1, Role::Admin, "admin01", "System Administrator", ""),
        account(kO1, Role::Ops, "ops01", "Railway Operations", "")
    };

    for (const Account& a : accounts) {
        Status result = ensureAccount(data, a);
        if (!result.ok()) return result;
    }

    return Status::success();
}

Reservation makeReservation(
    const std::string& pnr,
    const std::string& accountId,
    const std::string& bookedBy,
    const std::string& trainNumber,
    const Date& journeyDate,
    ClassCode classCode,
    const std::string& source,
    const std::string& destination,
    TicketStatus status,
    int waitlistPosition,
    Money fare,
    EpochSeconds bookedAt,
    const Traveller& traveller) {

    Reservation r;
    r.pnr = pnr;
    r.requestId = "seed-request-" + pnr;
    r.accountId = accountId;
    r.bookedBy = bookedBy;
    r.trainNumber = trainNumber;
    r.journeyDate = journeyDate;
    r.classCode = classCode;
    r.sourceCode = source;
    r.destinationCode = destination;
    r.status = status;
    r.waitlistPosition = waitlistPosition;
    r.fare = fare;
    r.bookedAt = bookedAt;
    r.travellers.push_back(traveller);

    return r;
}

// Verify that a previously inserted sample reservation still matches
// the deterministic seed definition.
bool matchesSeedReservation(const Reservation& actual,
                            const Reservation& expected) {
    if (actual.pnr != expected.pnr ||
        actual.requestId != expected.requestId ||
        actual.accountId != expected.accountId ||
        actual.bookedBy != expected.bookedBy ||
        actual.trainNumber != expected.trainNumber ||
        actual.journeyDate != expected.journeyDate ||
        actual.classCode != expected.classCode ||
        actual.sourceCode != expected.sourceCode ||
        actual.destinationCode != expected.destinationCode ||
        actual.status != expected.status ||
        actual.waitlistPosition != expected.waitlistPosition ||
        actual.fare != expected.fare ||
        actual.refund != expected.refund ||
        actual.bookedAt != expected.bookedAt ||
        actual.cancelledAt != expected.cancelledAt ||
        actual.travellers.size() != expected.travellers.size()) {
        return false;
    }

    for (std::size_t i = 0; i < actual.travellers.size(); ++i) {
        const Traveller& a = actual.travellers[i];
        const Traveller& e = expected.travellers[i];

        if (a.name != e.name ||
            a.age != e.age ||
            a.gender != e.gender ||
            a.idProofNumber != e.idProofNumber ||
            a.seatNumber != e.seatNumber) {
            return false;
        }
    }

    return actual.queueSeq > 0;
}

// Inserts a sample reservation once. A conflicting existing record is an error.
Status ensureReservation(DataAccess& data, Reservation expected) {
    auto existing = data.reservations.findByPnr(expected.pnr);

    if (!existing.ok()) {
        return existing.error();
    }

    if (existing.value()) {
        if (matchesSeedReservation(*existing.value(), expected)) {
            return Status::success();
        }

        return Status::failure(
            ErrorCode::Database,
            "Seed reservation conflicts with existing PNR: " + expected.pnr);
    }

    auto request = data.reservations.findByRequestId(expected.requestId);

    if (!request.ok()) {
        return request.error();
    }

    if (request.value()) {
        return Status::failure(
            ErrorCode::Database,
            "Seed request ID already belongs to another reservation: " +
                expected.requestId);
    }

    auto nextSequence = data.reservations.nextQueueSeq();

    if (!nextSequence.ok()) {
        return nextSequence.error();
    }

    expected.queueSeq = nextSequence.value();

    return data.reservations.insert(expected);
}

Status seedSampleReservations(DataAccess& data, const Date& firstDay) {
    const EpochSeconds bookedAt =
        static_cast<EpochSeconds>(firstDay.dayNumber()) * 86400 + 8 * 3600;

    Traveller asha;
    asha.name = "Asha Rao";
    asha.age = 29;
    asha.gender = 'F';
    asha.idProofNumber = "DL-SEED-0001";
    asha.seatNumber = "S1-01";

    Traveller ravi;
    ravi.name = "Ravi Kumar";
    ravi.age = 34;
    ravi.gender = 'M';
    ravi.idProofNumber = "DL-SEED-0002";
    ravi.seatNumber = "B1-01";

    Traveller meera;
    meera.name = "Meera Sharma";
    meera.age = 26;
    meera.gender = 'F';
    meera.idProofNumber = "DL-SEED-0003";
    meera.seatNumber = "";

    Traveller kiran;
    kiran.name = "Kiran Kumar";
    kiran.age = 41;
    kiran.gender = 'M';
    kiran.idProofNumber = "DL-SEED-0004";
    kiran.seatNumber = "B1-01";

    const std::vector<Reservation> reservations = {
        makeReservation(
            "9000000001", kP1, kP1, "20001", firstDay.plusDays(2),
            ClassCode::SL, "SBC", "MAS", TicketStatus::Confirmed, 0,
            Money::fromRupees(350), bookedAt, asha),

        makeReservation(
            "9000000002", kP2, kP2, "20002", firstDay.plusDays(3),
            ClassCode::AC3, "MAS", "SBC", TicketStatus::Confirmed, 0,
            Money::fromRupees(875), bookedAt, ravi),

        makeReservation(
            "9000000003", kP1, kS1, "20003", firstDay.plusDays(4),
            ClassCode::SL, "SBC", "MYS", TicketStatus::Waitlisted, 1,
            Money::fromRupees(140), bookedAt, meera),

        makeReservation(
            "9000000004", kP2, kS1, "20004", firstDay.plusDays(5),
            ClassCode::AC3, "SBC", "SC", TicketStatus::Confirmed, 0,
            Money::fromRupees(1425), bookedAt, kiran)
    };

    for (const Reservation& r : reservations) {
        Status result = ensureReservation(data, r);
        if (!result.ok()) return result;
    }

    return Status::success();
}

}  // namespace

Status loadSeedData(DataAccess& data, const Date& firstDay, int scheduleDays) {
    TransactionGuard tx(data.unitOfWork);

    if (!tx.started().ok()) {
        return tx.started();
    }

    Status result = seedBaseData(data, firstDay, scheduleDays);
    if (!result.ok()) return result;

    return tx.commit();
}

Status loadDemoSeedData(DataAccess& data, const Date& firstDay, int scheduleDays) {
    if (scheduleDays < 6) {
        return Status::failure(
            ErrorCode::Validation,
            "Complete demo seed requires at least 6 schedule days");
    }

    TransactionGuard tx(data.unitOfWork);

    if (!tx.started().ok()) {
        return tx.started();
    }

    Status result = seedBaseData(data, firstDay, scheduleDays);
    if (!result.ok()) return result;

    result = seedSampleReservations(data, firstDay);
    if (!result.ok()) return result;

    return tx.commit();
}

}  // namespace rrs::seed