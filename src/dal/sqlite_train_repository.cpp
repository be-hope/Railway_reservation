// sqlite_train_repository.cpp

#include "dal/sqlite_repositories.h"

namespace rrs {

namespace {

void bindMinutes(Statement& st, int index, int minutes) {
    if (minutes < 0) st.bindNull(index);
    else st.bind(index, minutes);
}

}  // namespace

Status SqliteTrainRepository::saveStation(const Station& station) {
    Statement st(db_,
                 "INSERT INTO station(station_code, station_name) VALUES (?, ?) "
                 "ON CONFLICT(station_code) DO UPDATE SET station_name = excluded.station_name");
    return st.bind(1, station.code).bind(2, station.name).execute();
}

Result<std::vector<Station>> SqliteTrainRepository::listStations() {
    Statement st(db_, "SELECT station_code, station_name FROM station ORDER BY station_code");
    std::vector<Station> out;
    while (st.step()) out.push_back(Station{st.text(0), st.text(1)});
    if (!st.status().ok()) return st.status().error();
    return out;
}

Status SqliteTrainRepository::saveTrain(const Train& train) {
    Statement up(db_,
                 "INSERT INTO train(train_number, train_name, is_active) VALUES (?, ?, ?) "
                 "ON CONFLICT(train_number) DO UPDATE SET train_name = excluded.train_name, "
                 "is_active = excluded.is_active");
    Status s = up.bind(1, train.number).bind(2, train.name).bind(3, train.active ? 1 : 0).execute();
    if (!s.ok()) return s;

    Statement del(db_, "DELETE FROM route_stop WHERE train_number = ?");
    s = del.bind(1, train.number).execute();
    if (!s.ok()) return s;

    for (const RouteStop& stop : train.stops) {
        Statement ins(db_,
                      "INSERT INTO route_stop(train_number, stop_seq, station_code, arrival_minutes, "
                      "departure_minutes, day_offset, distance_km) VALUES (?, ?, ?, ?, ?, ?, ?)");
        ins.bind(1, train.number).bind(2, stop.seq).bind(3, stop.stationCode);
        bindMinutes(ins, 4, stop.arrivalMinutes);
        bindMinutes(ins, 5, stop.departureMinutes);
        ins.bind(6, stop.dayOffset).bind(7, stop.distanceKm);
        s = ins.execute();
        if (!s.ok()) return s;
    }
    return Status::success();
}

Status SqliteTrainRepository::setTrainActive(const std::string& trainNumber, bool active) {
    Statement st(db_, "UPDATE train SET is_active = ? WHERE train_number = ?");
    return st.bind(1, active ? 1 : 0).bind(2, trainNumber).execute();
}

Result<std::vector<RouteStop>> SqliteTrainRepository::loadStops(const std::string& trainNumber) {
    Statement st(db_,
                 "SELECT stop_seq, station_code, arrival_minutes, departure_minutes, day_offset, "
                 "distance_km FROM route_stop WHERE train_number = ? ORDER BY stop_seq");
    st.bind(1, trainNumber);
    std::vector<RouteStop> stops;
    while (st.step()) {
        RouteStop s;
        s.seq = static_cast<int>(st.integer(0));
        s.stationCode = st.text(1);
        s.arrivalMinutes = st.isNull(2) ? -1 : static_cast<int>(st.integer(2));
        s.departureMinutes = st.isNull(3) ? -1 : static_cast<int>(st.integer(3));
        s.dayOffset = static_cast<int>(st.integer(4));
        s.distanceKm = static_cast<int>(st.integer(5));
        stops.push_back(s);
    }
    if (!st.status().ok()) return st.status().error();
    return stops;
}

Result<std::optional<Train>> SqliteTrainRepository::findTrain(const std::string& trainNumber) {
    Statement st(db_, "SELECT train_number, train_name, is_active FROM train WHERE train_number = ?");
    st.bind(1, trainNumber);
    if (!st.step()) {
        if (!st.status().ok()) return st.status().error();
        return std::optional<Train>();
    }
    Train t;
    t.number = st.text(0);
    t.name = st.text(1);
    t.active = st.integer(2) != 0;
    auto stops = loadStops(t.number);
    if (!stops.ok()) return stops.error();
    t.stops = std::move(stops.value());
    return std::optional<Train>(std::move(t));
}

Result<std::vector<Train>> SqliteTrainRepository::listTrains() {
    Statement st(db_, "SELECT train_number FROM train ORDER BY train_number");
    std::vector<std::string> numbers;
    while (st.step()) numbers.push_back(st.text(0));
    if (!st.status().ok()) return st.status().error();
    std::vector<Train> out;
    for (const std::string& n : numbers) {
        auto t = findTrain(n);
        if (!t.ok()) return t.error();
        if (t.value()) out.push_back(*t.value());
    }
    return out;
}

Result<std::vector<Train>> SqliteTrainRepository::findTrains(const std::string& source,
                                                             const std::string& destination,
                                                             const Date& date) {
    // Indexed lookup: route_stop(station_code) for both ends, schedule(journey_date).
    Statement st(db_,
                 "SELECT t.train_number FROM train t "
                 "JOIN route_stop a ON a.train_number = t.train_number AND a.station_code = ? "
                 "JOIN route_stop b ON b.train_number = t.train_number AND b.station_code = ? "
                 "JOIN schedule s ON s.train_number = t.train_number AND s.journey_date = ? "
                 "WHERE t.is_active = 1 AND s.is_active = 1 AND a.stop_seq < b.stop_seq "
                 "ORDER BY a.departure_minutes, t.train_number");
    st.bind(1, source).bind(2, destination).bind(3, date.iso());
    std::vector<std::string> numbers;
    while (st.step()) numbers.push_back(st.text(0));
    if (!st.status().ok()) return st.status().error();
    std::vector<Train> out;
    for (const std::string& n : numbers) {
        auto t = findTrain(n);
        if (!t.ok()) return t.error();
        if (t.value()) out.push_back(*t.value());
    }
    return out;
}

Status SqliteTrainRepository::addSchedule(const std::string& trainNumber, const Date& date) {
    Statement st(db_,
                 "INSERT INTO schedule(train_number, journey_date, is_active) VALUES (?, ?, 1) "
                 "ON CONFLICT(train_number, journey_date) DO UPDATE SET is_active = 1");
    return st.bind(1, trainNumber).bind(2, date.iso()).execute();
}

Status SqliteTrainRepository::removeSchedule(const std::string& trainNumber, const Date& date) {
    // Soft delete: reservations keep their foreign key to the schedule row.
    Statement st(db_, "UPDATE schedule SET is_active = 0 WHERE train_number = ? AND journey_date = ?");
    return st.bind(1, trainNumber).bind(2, date.iso()).execute();
}

Result<bool> SqliteTrainRepository::hasSchedule(const std::string& trainNumber, const Date& date) {
    Statement st(db_,
                 "SELECT 1 FROM schedule WHERE train_number = ? AND journey_date = ? AND is_active = 1");
    st.bind(1, trainNumber).bind(2, date.iso());
    bool found = st.step();
    if (!st.status().ok()) return st.status().error();
    return found;
}

Result<std::vector<Date>> SqliteTrainRepository::listSchedules(const std::string& trainNumber) {
    Statement st(db_,
                 "SELECT journey_date FROM schedule WHERE train_number = ? AND is_active = 1 "
                 "ORDER BY journey_date");
    st.bind(1, trainNumber);
    std::vector<Date> out;
    while (st.step()) {
        auto d = Date::parseIso(st.text(0));
        if (d) out.push_back(*d);
    }
    if (!st.status().ok()) return st.status().error();
    return out;
}

}  // namespace rrs
