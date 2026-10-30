// sqlite_reservation_repository.cpp

#include "dal/sqlite_repositories.h"

namespace rrs {

namespace {

const char* const kReservationColumns =
    "pnr, request_id, account_id, booked_by, train_number, journey_date, class_code, "
    "source_code, destination_code, status, wl_position, fare_paise, refund_paise, "
    "queue_seq, booked_at, cancelled_at";

std::string select(const char* whereAndOrder) {
    return std::string("SELECT ") + kReservationColumns + " FROM reservation " + whereAndOrder;
}

Reservation readReservation(const Statement& st) {
    Reservation r;
    r.pnr = st.text(0);
    r.requestId = st.text(1);
    r.accountId = st.text(2);
    r.bookedBy = st.text(3);
    r.trainNumber = st.text(4);
    r.journeyDate = Date::parseIso(st.text(5)).value_or(Date());
    r.classCode = parseClassCode(st.text(6)).value_or(ClassCode::SL);
    r.sourceCode = st.text(7);
    r.destinationCode = st.text(8);
    r.status = parseTicketStatus(st.text(9)).value_or(TicketStatus::Cancelled);
    r.waitlistPosition = st.isNull(10) ? 0 : static_cast<int>(st.integer(10));
    r.fare = Money::fromPaise(st.integer(11));
    if (!st.isNull(12)) r.refund = Money::fromPaise(st.integer(12));
    r.queueSeq = st.integer(13);
    r.bookedAt = st.integer(14);
    r.cancelledAt = st.isNull(15) ? 0 : st.integer(15);
    return r;
}

}  // namespace

Status SqliteReservationRepository::insert(const Reservation& r) {
    Statement st(db_,
                 "INSERT INTO reservation(pnr, request_id, account_id, booked_by, train_number, "
                 "journey_date, class_code, source_code, destination_code, status, wl_position, "
                 "fare_paise, refund_paise, queue_seq, booked_at, cancelled_at) "
                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    st.bind(1, r.pnr).bind(2, r.requestId).bind(3, r.accountId).bind(4, r.bookedBy)
        .bind(5, r.trainNumber).bind(6, r.journeyDate.iso()).bind(7, classCodeString(r.classCode))
        .bind(8, r.sourceCode).bind(9, r.destinationCode).bind(10, ticketStatusString(r.status));
    if (r.status == TicketStatus::Waitlisted) st.bind(11, r.waitlistPosition);
    else st.bindNull(11);
    st.bind(12, r.fare.paise());
    if (r.refund) st.bind(13, r.refund->paise());
    else st.bindNull(13);
    st.bind(14, r.queueSeq).bind(15, r.bookedAt);
    if (r.cancelledAt > 0) st.bind(16, r.cancelledAt);
    else st.bindNull(16);
    Status s = st.execute();
    if (!s.ok()) return s;

    int seq = 1;
    for (const Traveller& t : r.travellers) {
        Statement tv(db_,
                     "INSERT INTO traveller(pnr, seq, name, age, gender, id_proof_number, seat_number, "
                     "train_number, journey_date, class_code) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        s = tv.bind(1, r.pnr).bind(2, seq++).bind(3, t.name).bind(4, t.age)
                .bind(5, std::string(1, t.gender)).bindOptional(6, t.idProofNumber)
                .bindOptional(7, t.seatNumber).bind(8, r.trainNumber).bind(9, r.journeyDate.iso())
                .bind(10, classCodeString(r.classCode))
                .execute();
        if (!s.ok()) return s;
    }
    return Status::success();
}

Status SqliteReservationRepository::update(const Reservation& r) {
    Statement st(db_,
                 "UPDATE reservation SET status = ?, wl_position = ?, refund_paise = ?, "
                 "cancelled_at = ? WHERE pnr = ?");
    st.bind(1, ticketStatusString(r.status));
    if (r.status == TicketStatus::Waitlisted) st.bind(2, r.waitlistPosition);
    else st.bindNull(2);
    if (r.refund) st.bind(3, r.refund->paise());
    else st.bindNull(3);
    if (r.cancelledAt > 0) st.bind(4, r.cancelledAt);
    else st.bindNull(4);
    st.bind(5, r.pnr);
    Status s = st.execute();
    if (!s.ok()) return s;
    if (db_.changes() == 0) return Status::failure(ErrorCode::PnrNotFound, r.pnr);

    // Seats first all released, then re-assigned, so swapping seats between
    // travellers of one PNR never trips ux_seat.
    Statement clear(db_, "UPDATE traveller SET seat_number = NULL WHERE pnr = ?");
    s = clear.bind(1, r.pnr).execute();
    if (!s.ok()) return s;
    int seq = 1;
    for (const Traveller& t : r.travellers) {
        Statement tv(db_,
                     "UPDATE traveller SET seat_number = ?, name = ?, age = ?, gender = ?, "
                     "id_proof_number = ? WHERE pnr = ? AND seq = ?");
        s = tv.bindOptional(1, t.seatNumber).bind(2, t.name).bind(3, t.age)
                .bind(4, std::string(1, t.gender)).bindOptional(5, t.idProofNumber)
                .bind(6, r.pnr).bind(7, seq++)
                .execute();
        if (!s.ok()) return s;
    }
    return Status::success();
}

Status SqliteReservationRepository::loadTravellers(Reservation& r) {
    Statement st(db_,
                 "SELECT name, age, gender, id_proof_number, seat_number FROM traveller "
                 "WHERE pnr = ? ORDER BY seq");
    st.bind(1, r.pnr);
    r.travellers.clear();
    while (st.step()) {
        Traveller t;
        t.name = st.text(0);
        t.age = static_cast<int>(st.integer(1));
        std::string g = st.text(2);
        t.gender = g.empty() ? 'O' : g[0];
        t.idProofNumber = st.text(3);
        t.seatNumber = st.text(4);
        r.travellers.push_back(t);
    }
    return st.status();
}

Result<std::vector<Reservation>> SqliteReservationRepository::query(Statement& st) {
    std::vector<Reservation> out;
    while (st.step()) out.push_back(readReservation(st));
    if (!st.status().ok()) return st.status().error();
    for (Reservation& r : out) {
        Status s = loadTravellers(r);
        if (!s.ok()) return s.error();
    }
    return out;
}

Result<std::optional<Reservation>> SqliteReservationRepository::findByPnr(const std::string& pnr) {
    std::string sql = select("WHERE pnr = ?");
    Statement st(db_, sql.c_str());
    st.bind(1, pnr);
    auto rows = query(st);
    if (!rows.ok()) return rows.error();
    if (rows.value().empty()) return std::optional<Reservation>();
    return std::optional<Reservation>(rows.value().front());
}

Result<std::optional<Reservation>> SqliteReservationRepository::findByRequestId(
    const std::string& requestId) {
    std::string sql = select("WHERE request_id = ?");
    Statement st(db_, sql.c_str());
    st.bind(1, requestId);
    auto rows = query(st);
    if (!rows.ok()) return rows.error();
    if (rows.value().empty()) return std::optional<Reservation>();
    return std::optional<Reservation>(rows.value().front());
}

Result<bool> SqliteReservationRepository::pnrExists(const std::string& pnr) {
    Statement st(db_, "SELECT 1 FROM reservation WHERE pnr = ?");
    st.bind(1, pnr);
    bool found = st.step();
    if (!st.status().ok()) return st.status().error();
    return found;
}

Result<TripOccupancy> SqliteReservationRepository::occupancy(const std::string& trainNumber,
                                                             const Date& date, ClassCode classCode) {
    TripOccupancy occ;
    Statement counts(db_,
                     "SELECT r.status, COUNT(DISTINCT r.pnr), COUNT(t.seq) FROM reservation r "
                     "JOIN traveller t ON t.pnr = r.pnr "
                     "WHERE r.train_number = ? AND r.journey_date = ? AND r.class_code = ? "
                     "AND r.status <> 'CANCELLED' GROUP BY r.status");
    counts.bind(1, trainNumber).bind(2, date.iso()).bind(3, classCodeString(classCode));
    while (counts.step()) {
        auto status = parseTicketStatus(counts.text(0));
        int tickets = static_cast<int>(counts.integer(1));
        int travellers = static_cast<int>(counts.integer(2));
        if (status == TicketStatus::Confirmed) occ.confirmedTravellers = travellers;
        else if (status == TicketStatus::Rac) occ.racTravellers = travellers;
        else if (status == TicketStatus::Waitlisted) occ.waitlistTickets = tickets;
    }
    if (!counts.status().ok()) return counts.status().error();

    Statement seats(db_,
                    "SELECT seat_number FROM traveller WHERE train_number = ? AND journey_date = ? "
                    "AND class_code = ? AND seat_number IS NOT NULL ORDER BY seat_number");
    seats.bind(1, trainNumber).bind(2, date.iso()).bind(3, classCodeString(classCode));
    while (seats.step()) occ.occupiedSeats.push_back(seats.text(0));
    if (!seats.status().ok()) return seats.status().error();
    return occ;
}

Result<std::vector<Reservation>> SqliteReservationRepository::listActiveForTrip(
    const std::string& trainNumber, const Date& date, ClassCode classCode) {
    std::string sql = select(
        "WHERE train_number = ? AND journey_date = ? AND class_code = ? AND status <> 'CANCELLED' "
        "ORDER BY queue_seq");
    Statement st(db_, sql.c_str());
    st.bind(1, trainNumber).bind(2, date.iso()).bind(3, classCodeString(classCode));
    return query(st);
}

Result<std::vector<Reservation>> SqliteReservationRepository::listForAccount(
    const std::string& accountId) {
    std::string sql = select("WHERE account_id = ? ORDER BY journey_date, pnr");
    Statement st(db_, sql.c_str());
    st.bind(1, accountId);
    return query(st);
}

Result<std::vector<Reservation>> SqliteReservationRepository::listByJourneyDate(const Date& from,
                                                                                const Date& to) {
    std::string sql = select("WHERE journey_date BETWEEN ? AND ? ORDER BY journey_date, queue_seq");
    Statement st(db_, sql.c_str());
    st.bind(1, from.iso()).bind(2, to.iso());
    return query(st);
}

Result<int> SqliteReservationRepository::countActiveForTrain(const std::string& trainNumber,
                                                             const Date& fromDate) {
    Statement st(db_,
                 "SELECT COUNT(*) FROM reservation WHERE train_number = ? AND journey_date >= ? "
                 "AND status <> 'CANCELLED'");
    st.bind(1, trainNumber).bind(2, fromDate.iso());
    if (!st.step()) return st.status().error();
    return static_cast<int>(st.integer(0));
}

Result<std::int64_t> SqliteReservationRepository::nextQueueSeq() {
    Statement st(db_, "SELECT COALESCE(MAX(queue_seq), 0) + 1 FROM reservation");
    if (!st.step()) return st.status().error();
    return st.integer(0);
}

}  // namespace rrs
