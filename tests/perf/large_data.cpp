// large_data.cpp

#include "perf/large_data.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <random>

#include "booking/seat_allocator.h"
#include "seed_data.h"

namespace rrs::perf {

namespace {

// 40 two-letter codes: QA..QZ, RA..RN (none collide with the seed codes).
std::string stationCode(int i) {
    std::string code = "QA";
    code[0] = static_cast<char>('Q' + i / 26);
    code[1] = static_cast<char>('A' + i % 26);
    return code;
}

}  // namespace

double percentileMs(std::vector<double> samples, double percentile) {
    if (samples.empty()) return 0.0;
    std::sort(samples.begin(), samples.end());
    std::size_t rank = static_cast<std::size_t>(std::ceil(percentile / 100.0 * samples.size()));
    if (rank == 0) rank = 1;
    return samples[std::min(rank, samples.size()) - 1];
}

Result<LargeData> generateLargeData(DataAccess& data, const Date& today, const LargeDataSpec& spec) {
    Status seeded = seed::loadSeedData(data, today, spec.scheduleDays);
    if (!seeded.ok()) return seeded.error();

    std::mt19937 rng(spec.seed);
    auto pick = [&rng](int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); };

    TransactionGuard tx(data.unitOfWork);
    if (!tx.started().ok()) return tx.started().error();

    LargeData out;
    for (int i = 0; i < spec.stations; ++i) {
        Station s{stationCode(i), "Station " + stationCode(i)};
        Status st = data.trains.saveStation(s);
        if (!st.ok()) return st.error();
        out.stationCodes.push_back(s.code);
    }

    for (int i = 0; i < spec.trains; ++i) {
        Train t;
        t.number = std::to_string(30001 + i);
        t.name = "Generated Express " + std::to_string(i + 1);
        std::vector<std::string> codes = out.stationCodes;
        std::shuffle(codes.begin(), codes.end(), rng);
        const int stops = pick(4, 9);
        int minute = pick(0, 20) * 60;
        int km = 0;
        for (int s = 0; s < stops; ++s) {
            RouteStop stop;
            stop.seq = s + 1;
            stop.stationCode = codes[static_cast<std::size_t>(s)];
            stop.dayOffset = minute / 1440;
            stop.arrivalMinutes = s == 0 ? -1 : minute % 1440;
            if (s > 0) minute += 5;
            stop.departureMinutes = s == stops - 1 ? -1 : minute % 1440;
            stop.distanceKm = km;
            t.stops.push_back(stop);
            km += pick(30, 150);
            minute += pick(30, 120);
        }
        Status st = data.trains.saveTrain(t);
        if (!st.ok()) return st.error();
        for (ClassCode cls : {ClassCode::SL, ClassCode::AC3}) {
            ClassConfig c;
            c.trainNumber = t.number;
            c.classCode = cls;
            c.coaches = cls == ClassCode::SL ? 4 : 2;
            c.seatsPerCoach = cls == ClassCode::SL ? 72 : 64;
            c.racQuota = 4;
            c.waitlistCapacity = 20;
            c.fareMultiplierPercent = cls == ClassCode::SL ? 100 : 250;
            st = data.config.saveClassConfig(c);
            if (!st.ok()) return st.error();
        }
        for (int d = 0; d < spec.scheduleDays; ++d) {
            st = data.trains.addSchedule(t.number, today.plusDays(d));
            if (!st.ok()) return st.error();
        }
        out.trains.push_back(t);
    }

    // Confirmed reservations with properly allocated seats.
    std::map<std::string, std::vector<std::string>> occupied;   // trip -> seats
    std::map<std::string, ClassConfig> configs;
    std::int64_t seq = 0;
    for (int i = 0; i < spec.reservations; ++i) {
        const Train& t = out.trains[static_cast<std::size_t>(pick(0, spec.trains - 1))];
        const ClassCode cls = pick(0, 2) == 0 ? ClassCode::AC3 : ClassCode::SL;
        const Date date = today.plusDays(pick(0, spec.scheduleDays - 1));
        const int a = pick(0, static_cast<int>(t.stops.size()) - 2);
        const int b = pick(a + 1, static_cast<int>(t.stops.size()) - 1);
        const int travellers = pick(1, 3);

        const std::string configKey = t.number + classCodeString(cls);
        if (!configs.count(configKey)) configs[configKey] = *data.config.findClassConfig(t.number, cls).value();
        const std::string trip = configKey + date.iso();
        auto seats = SeatAllocator::allocate(configs[configKey], occupied[trip], travellers);
        if (!seats.ok()) continue;   // trip full: skip

        Reservation r;
        r.pnr = std::to_string(2000000000LL + i);
        r.requestId = "large-" + std::to_string(i);
        r.accountId = (i % 2) ? seed::kP1 : seed::kP2;
        r.bookedBy = r.accountId;
        r.trainNumber = t.number;
        r.journeyDate = date;
        r.classCode = cls;
        r.sourceCode = t.stops[static_cast<std::size_t>(a)].stationCode;
        r.destinationCode = t.stops[static_cast<std::size_t>(b)].stationCode;
        r.status = TicketStatus::Confirmed;
        const int km = t.stops[static_cast<std::size_t>(b)].distanceKm - t.stops[static_cast<std::size_t>(a)].distanceKm;
        r.fare = Money::fromPaise(static_cast<std::int64_t>(km) * configs[configKey].fareMultiplierPercent * travellers);
        r.queueSeq = ++seq;
        r.bookedAt = 0;
        for (int k = 0; k < travellers; ++k) {
            Traveller tr;
            tr.name = "Passenger " + std::to_string(i);
            tr.age = pick(1, 90);
            tr.gender = "MFO"[pick(0, 2)];
            tr.idProofNumber = "ID" + std::to_string(100000 + i);
            tr.seatNumber = seats.value()[static_cast<std::size_t>(k)];
            occupied[trip].push_back(tr.seatNumber);
            r.travellers.push_back(tr);
        }
        Status st = data.reservations.insert(r);
        if (!st.ok()) return st.error();
        ++out.reservationsInserted;
    }

    Status committed = tx.commit();
    if (!committed.ok()) return committed.error();
    return out;
}

}  // namespace rrs::perf
