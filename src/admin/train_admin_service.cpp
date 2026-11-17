#include "admin/train_admin_service.h"
#include "security/input_validator.h"
#include <algorithm>
#include <set>
namespace rrs {
Status TrainAdminService::validateRoute(const std::vector<RouteStop>& stops) {
    if (stops.size() < 2 || stops.size() > 100) return Status::failure(ErrorCode::Validation, "Route requires 2-100 stops.");
    auto stations = data_.trains.listStations(); if (!stations.ok()) return stations.error();
    std::set<std::string> seen;
    int previousDeparture = -1, previousDistance = -1;
    for (std::size_t i = 0; i < stops.size(); ++i) {
        const auto& s = stops[i];
        bool known = std::any_of(stations->begin(), stations->end(), [&](const Station& st) { return st.code == s.stationCode; });
        if (!known || !InputValidator::station(s.stationCode).ok() || !seen.insert(s.stationCode).second ||
            s.seq != static_cast<int>(i + 1) || s.dayOffset < 0 || s.dayOffset > 30 ||
            s.distanceKm <= previousDistance || (i == 0 && s.distanceKm != 0) ||
            s.arrivalMinutes < -1 || s.arrivalMinutes >= 1440 || s.departureMinutes < -1 || s.departureMinutes >= 1440)
            return Status::failure(ErrorCode::Validation, "Invalid route station, order, distance or time.");
        if ((i == 0 && s.arrivalMinutes != -1) || (i > 0 && s.arrivalMinutes < 0) ||
            (i + 1 == stops.size() && s.departureMinutes != -1) || (i + 1 < stops.size() && s.departureMinutes < 0))
            return Status::failure(ErrorCode::Validation, "Invalid origin or destination time.");
        const int arrival = s.dayOffset * 1440 + s.arrivalMinutes;
        const int departure = s.dayOffset * 1440 + s.departureMinutes;
        if ((i > 0 && arrival < previousDeparture) || (i + 1 < stops.size() && i > 0 && departure < arrival))
            return Status::failure(ErrorCode::Validation, "Route times must be chronological.");
        previousDistance = s.distanceKm; previousDeparture = departure;
    }
    return Status::success();
}
Status TrainAdminService::save(const Session& s, const Train& train) {
    TransactionGuard tx(data_.unitOfWork); if (!tx.started().ok()) return tx.started();
    auto saved = data_.trains.saveTrain(train); if (!saved.ok()) return saved;
    saved = tx.commit(); if (!saved.ok()) return saved;
    audit_.log("TRAIN_CHANGED", s.accountId, "train=" + train.number);
    return Status::success();
}
Result<Train> TrainAdminService::getTrain(const Session& s, const std::string& number) {
    auto allowed = access_.require(s, Permission::ManageTrains); if (!allowed.ok()) return allowed.error();
    auto found = data_.trains.findTrain(number); if (!found.ok()) return found.error();
    if (!found.value()) return Result<Train>::failure(ErrorCode::TrainNotFound);
    return *found.value();
}
Status TrainAdminService::addTrain(const Session& s, const Train& train) {
    auto allowed = access_.require(s, Permission::ManageTrains); if (!allowed.ok()) return allowed;
    if (train.number.empty() || train.number.size() > 10 || !std::all_of(train.number.begin(), train.number.end(), [](char c) { return c >= '0' && c <= '9'; }) ||
        !InputValidator::name(train.name).ok() || train.name.size() > 60) return Status::failure(ErrorCode::Validation, "Invalid train number or name.");
    if (!train.stops.empty()) { auto valid = validateRoute(train.stops); if (!valid.ok()) return valid; }
    auto found = data_.trains.findTrain(train.number); if (!found.ok()) return found.error();
    if (found.value()) return Status::failure(ErrorCode::Validation, "Train number already exists.");
    return save(s, train);
}
Status TrainAdminService::updateTrainName(const Session& s, const std::string& n, const std::string& name) {
    auto train = getTrain(s, n); if (!train.ok()) return train.error();
    if (!InputValidator::name(name).ok() || name.size() > 60) return Status::failure(ErrorCode::Validation, "Invalid train name.");
    train->name = name; return save(s, train.value());
}
Status TrainAdminService::deactivateTrain(const Session& s, const std::string& n) {
    auto train = getTrain(s, n); if (!train.ok()) return train.error();
    TransactionGuard tx(data_.unitOfWork); if (!tx.started().ok()) return tx.started();
    auto count = data_.reservations.countActiveForTrain(n, clock_.today()); if (!count.ok()) return count.error();
    if (count.value() > 0) return Status::failure(ErrorCode::TrainInUse);
    auto saved = data_.trains.setTrainActive(n, false); if (!saved.ok()) return saved;
    saved = tx.commit(); if (!saved.ok()) return saved;
    audit_.log("TRAIN_CHANGED", s.accountId, "deactivated train=" + n); return Status::success();
}
Status TrainAdminService::activateTrain(const Session& s, const std::string& n) {
    auto train = getTrain(s, n); if (!train.ok()) return train.error();
    train->active = true; return save(s, train.value());
}
Status TrainAdminService::setRoute(const Session& s, const std::string& n, const std::vector<RouteStop>& stops) {
    auto train = getTrain(s, n); if (!train.ok()) return train.error();
    auto valid = validateRoute(stops); if (!valid.ok()) return valid;
    TransactionGuard tx(data_.unitOfWork); if (!tx.started().ok()) return tx.started();
    auto count = data_.reservations.countActiveForTrain(n, clock_.today()); if (!count.ok()) return count.error();
    if (count.value() > 0) return Status::failure(ErrorCode::TrainInUse, "Cannot change a route with active future reservations.");
    train->stops = stops;
    auto saved = data_.trains.saveTrain(train.value()); if (!saved.ok()) return saved;
    saved = tx.commit(); if (!saved.ok()) return saved;
    audit_.log("TRAIN_CHANGED", s.accountId, "route updated train=" + n); return Status::success();
}
Result<std::vector<RouteStop>> TrainAdminService::getRoute(const Session& s, const std::string& n) {
    auto train = getTrain(s, n); if (!train.ok()) return train.error(); return train->stops;
}
}
