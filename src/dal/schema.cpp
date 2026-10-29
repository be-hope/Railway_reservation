// schema.cpp
// Tables, constraints and indexes of rrs.db.
//
// Integrity rules enforced by the database itself (SRS §5.2):
//  * account login id / contact number / e-mail are UNIQUE (FR-004)
//  * reservation.pnr is the primary key and must be 10 digits, first 1-9 (FR-014)
//  * reservation.request_id is UNIQUE - idempotent booking (NFR-004, ADR-004)
//  * ux_seat: one seat can belong to only one active traveller per
//    train/date/class (FR-013). Cancelling sets seat_number to NULL.
//  * a WAITLISTED ticket always has a position; other statuses never do.

#include "dal/schema.h"

namespace rrs {

const char* const kSchemaSql = R"SQL(
CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS station (
    station_code TEXT PRIMARY KEY CHECK (length(station_code) BETWEEN 2 AND 5),
    station_name TEXT NOT NULL CHECK (length(station_name) BETWEEN 1 AND 50)
);

CREATE TABLE IF NOT EXISTS account (
    account_id      TEXT PRIMARY KEY CHECK (length(account_id) <= 36),
    role            TEXT NOT NULL CHECK (role IN ('PASSENGER','STAFF','ADMIN','OPS')),
    login_id        TEXT NOT NULL UNIQUE CHECK (length(login_id) <= 100),
    name            TEXT NOT NULL CHECK (length(name) BETWEEN 1 AND 100),
    age             INTEGER CHECK (age IS NULL OR age BETWEEN 1 AND 120),
    gender          TEXT CHECK (gender IS NULL OR gender IN ('M','F','O')),
    contact_number  TEXT UNIQUE CHECK (contact_number IS NULL OR length(contact_number) <= 15),
    email           TEXT UNIQUE CHECK (email IS NULL OR length(email) <= 100),
    password_hash   TEXT NOT NULL,
    salt            TEXT NOT NULL,
    id_proof_type   TEXT CHECK (id_proof_type IS NULL OR length(id_proof_type) <= 20),
    id_proof_number TEXT CHECK (id_proof_number IS NULL OR length(id_proof_number) <= 30),
    failed_attempts INTEGER NOT NULL DEFAULT 0 CHECK (failed_attempts >= 0),
    locked_until    INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS train (
    train_number TEXT PRIMARY KEY CHECK (length(train_number) BETWEEN 1 AND 10),
    train_name   TEXT NOT NULL CHECK (length(train_name) BETWEEN 1 AND 60),
    is_active    INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1))
);

CREATE TABLE IF NOT EXISTS route_stop (
    train_number      TEXT NOT NULL REFERENCES train(train_number) ON DELETE CASCADE,
    stop_seq          INTEGER NOT NULL CHECK (stop_seq >= 1),
    station_code      TEXT NOT NULL REFERENCES station(station_code),
    arrival_minutes   INTEGER CHECK (arrival_minutes IS NULL OR arrival_minutes BETWEEN 0 AND 1439),
    departure_minutes INTEGER CHECK (departure_minutes IS NULL OR departure_minutes BETWEEN 0 AND 1439),
    day_offset        INTEGER NOT NULL DEFAULT 0 CHECK (day_offset >= 0),
    distance_km       INTEGER NOT NULL CHECK (distance_km >= 0),
    PRIMARY KEY (train_number, stop_seq),
    UNIQUE (train_number, station_code)
);
CREATE INDEX IF NOT EXISTS ix_route_stop_station ON route_stop(station_code);

CREATE TABLE IF NOT EXISTS class_config (
    train_number      TEXT NOT NULL REFERENCES train(train_number) ON DELETE CASCADE,
    class_code        TEXT NOT NULL CHECK (class_code IN ('SL','3A')),
    coaches           INTEGER NOT NULL CHECK (coaches >= 1),
    seats_per_coach   INTEGER NOT NULL CHECK (seats_per_coach >= 1),
    rac_quota         INTEGER NOT NULL DEFAULT 0 CHECK (rac_quota >= 0),
    wl_capacity       INTEGER NOT NULL DEFAULT 20 CHECK (wl_capacity >= 0),
    fare_multiplier_pct INTEGER NOT NULL CHECK (fare_multiplier_pct > 0),
    PRIMARY KEY (train_number, class_code)
);

CREATE TABLE IF NOT EXISTS schedule (
    schedule_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    train_number TEXT NOT NULL REFERENCES train(train_number) ON DELETE CASCADE,
    journey_date TEXT NOT NULL CHECK (length(journey_date) = 10),
    is_active    INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    UNIQUE (train_number, journey_date)
);
CREATE INDEX IF NOT EXISTS ix_schedule_date ON schedule(journey_date);

CREATE TABLE IF NOT EXISTS reservation (
    pnr              TEXT PRIMARY KEY
                     CHECK (length(pnr) = 10 AND pnr GLOB '[1-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]'),
    request_id       TEXT NOT NULL UNIQUE CHECK (length(request_id) BETWEEN 1 AND 36),
    account_id       TEXT NOT NULL REFERENCES account(account_id),
    booked_by        TEXT NOT NULL REFERENCES account(account_id),
    train_number     TEXT NOT NULL,
    journey_date     TEXT NOT NULL,
    class_code       TEXT NOT NULL CHECK (class_code IN ('SL','3A')),
    source_code      TEXT NOT NULL REFERENCES station(station_code),
    destination_code TEXT NOT NULL REFERENCES station(station_code),
    status           TEXT NOT NULL CHECK (status IN ('CONFIRMED','RAC','WAITLISTED','CANCELLED')),
    wl_position      INTEGER CHECK (wl_position IS NULL OR wl_position >= 1),
    fare_paise       INTEGER NOT NULL CHECK (fare_paise >= 0),
    refund_paise     INTEGER CHECK (refund_paise IS NULL OR refund_paise >= 0),
    queue_seq        INTEGER NOT NULL UNIQUE,
    booked_at        INTEGER NOT NULL,
    cancelled_at     INTEGER,
    CHECK ((status = 'WAITLISTED') = (wl_position IS NOT NULL)),
    CHECK (source_code <> destination_code),
    FOREIGN KEY (train_number, journey_date) REFERENCES schedule(train_number, journey_date)
);
CREATE INDEX IF NOT EXISTS ix_reservation_trip    ON reservation(train_number, journey_date, class_code, status);
CREATE INDEX IF NOT EXISTS ix_reservation_account ON reservation(account_id, journey_date);
CREATE INDEX IF NOT EXISTS ix_reservation_date    ON reservation(journey_date);

CREATE TABLE IF NOT EXISTS traveller (
    pnr             TEXT NOT NULL REFERENCES reservation(pnr) ON DELETE CASCADE,
    seq             INTEGER NOT NULL CHECK (seq BETWEEN 1 AND 6),
    name            TEXT NOT NULL CHECK (length(name) BETWEEN 1 AND 100),
    age             INTEGER NOT NULL CHECK (age BETWEEN 1 AND 120),
    gender          TEXT NOT NULL CHECK (gender IN ('M','F','O')),
    id_proof_number TEXT CHECK (id_proof_number IS NULL OR length(id_proof_number) <= 30),
    seat_number     TEXT CHECK (seat_number IS NULL OR length(seat_number) <= 10),
    -- trip columns copied from the reservation so ux_seat can be enforced
    train_number    TEXT NOT NULL,
    journey_date    TEXT NOT NULL,
    class_code      TEXT NOT NULL,
    PRIMARY KEY (pnr, seq)
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_seat
    ON traveller(train_number, journey_date, class_code, seat_number)
    WHERE seat_number IS NOT NULL;
)SQL";

}  // namespace rrs
