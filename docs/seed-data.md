# Railway Reservation System - Seed Data

## Purpose

The seed utility creates a predictable SQLite database containing stations,
trains, class configurations, schedules, test accounts and sample reservations.

## Build

From the project root:

    cmake -S . -B build -DRRS_BUILD_TESTS=ON -DRRS_WERROR=ON
    cmake --build build --target rrs_seed_database -j

## Run

Use an explicit date when reproducible output is required:

    ./build/rrs_seed_database railway_seed.db 2026-11-02

Arguments:

- First argument: SQLite database path (optional).
- Second argument: seed start date in YYYY-MM-DD format (optional).

Defaults:

- Database file: rrs_seed.db
- Seed date: current system date
- Schedule length: 121 days

## Seed contents

The dataset contains:

- 5 stations
- 4 trains
- 8 train/class configurations
- 5 test accounts
- 4 sample reservations

The sample reservations include confirmed and waitlisted bookings.
One sample reservation is created by staff on behalf of a passenger.

## Test accounts

| Account ID | Login ID | Role |
|---|---|---|
| acc-p1 | 9876500001 | Passenger |
| acc-p2 | 9876500002 | Passenger |
| acc-s1 | staff01 | Staff |
| acc-a1 | admin01 | Admin |
| acc-o1 | ops01 | Operations |

### Authentication note

These seed accounts use a disabled password hash (`!`).
They are suitable for integration tests and development data, but cannot
authenticate using a password until the authentication module stores real
password hashes.

Do not use these accounts or their sample identity details in production.

## Repeatability

The complete seed operation is transactional.

- Existing matching seed accounts are retained.
- Existing matching sample reservations are retained.
- Conflicting records with the same seed identifiers cause an error.
- Re-running the utility does not duplicate the seed reservations.

For the same database state and explicit seed date, the same seed records
are produced.

## Test fixture usage

`seed::loadSeedData()` creates the base stations, trains, configurations,
schedules and accounts without sample reservations.

`seed::loadDemoSeedData()` creates the complete demo dataset, including
sample reservations.

Integration tests that require an empty booking history should continue
using `loadSeedData()`.
