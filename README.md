# Railway Reservation System

A C++17 application with a minimal passenger browser frontend and full role-based console menus for passenger registration/login, train search, booking, PNR lookup, cancellation/refunds, waitlist promotion, staff reservation corrections, train administration and operational reports. SQLite persists data locally. This is a coursework reservation simulation; it does not connect to a live railway or process payments.

## Start on this Windows computer

Double-click **Start-Railway.cmd**. It uses the installed Ubuntu WSL distribution, builds the executable, and starts the login screen with an opt-in demonstration database. The first run creates `demo.db` and `audit.log` in the repository directory. Reusing the same database preserves bookings.

Or open Ubuntu/WSL in this repository and run:

```sh
cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-final -j 4
./build-final/rrs_app demo.db --seed-demo
```

For a new, unseeded database: `./build-final/rrs_app private.db`. The `rrs_passenger_demo` executable is a compatibility alias built from the same application entry point.

## Build on a clean Ubuntu 24.04 machine

Requires GCC 11+, CMake 3.16+, SQLite3, OpenSSL Crypto, Python3 and GoogleTest. Install:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build libsqlite3-dev libssl-dev libgtest-dev python3 cppcheck gcovr
cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-final -j 4
ctest --test-dir build-final --output-on-failure
python3 tools/system_validation.py build-final/rrs_app
```

Verified platform: Ubuntu 24.04, GCC 13 under WSL. Native MSVC/macOS are not part of the recorded verification. Use WSL rather than the old Windows MinGW 6 compiler. When GoogleTest is installed, configuration needs no dependency download; otherwise CMake fetches GoogleTest from its official repository.

## Demonstration accounts

Enabled only when creating a new database with `--seed-demo`:

| Role | Login | Demo password |
| --- | --- | --- |
| Passenger | 9876500001 | Test@1234 |
| Passenger | 9876500002 | Test@1234 |
| Staff | staff01 | Test@1234 |
| Administrator | admin01 | Test@1234 |
| Operations | ops01 | Test@1234 |

The accounts use separate random salts; passwords are not stored as plaintext. Seed IDs for bookings on behalf of passengers are `acc-p1` and `acc-p2`. Normal registration always creates a passenger. Existing user credentials are never overwritten by seeding. The standalone `rrs_seed_database` utility creates deterministic sample data with disabled logins by default; its optional fourth argument `--enable-demo-logins` explicitly enables test credentials.

## What to demonstrate

- Passenger: search SBC to MAS, select train 20001, choose 3A and two travellers; fare is Rs 1,750.00. Save the PNR. View My bookings and complete My profile. IDs are masked.
- Another passenger cannot see the first passenger's history or ticket. Staff can book on behalf, correct names without changing owner/seats/fare, and cancel eligible tickets.
- Cancellation before departure calculates the refund, cancels the ticket, releases seats and promotes the waitlist. Confirmed/RAC refund slabs: 90% at >=48h, 75% at >=12h and <48h, 50% before departure below 12h. Waitlisted tickets receive 100%. Cancellation at/after departure is rejected.
- Administrator: add/rename/activate/deactivate trains, configure ordered routes, add/remove schedule dates, configure class capacity and inspect reports. Active future bookings protect route/schedule/train integrity. Class capacity reductions below booked load are rejected.
- Operations: booking statistics, train-wise counts and occupancy; no administrator modifications.

See [the application walkthrough](docs/final-demo.md) and [the test-case document](docs/test-cases.md).

## Architecture and backend

`tools/main.cpp` wires UI, services and repositories. `src/ui/` handles passenger screens; `tools/management_menu.cpp` handles admin/operations screens. `src/auth`, `booking`, `cancel`, `admin`, `security` contain business services. `src/dal` implements SQLite repositories and transactions; `src/common` contains shared records, fixed-point money, dates, sessions, audit and typed errors.

The root `passenger.cpp`, `train.cpp`, `route.cpp`, `seat.cpp`, and `fare.cpp` are earlier standalone sprint prototypes, retained as history. They are not part of the production target or production coverage denominator. Their functionality is implemented through the shared domain/services in `src/`; do not compile all root .cpp files together.

Key tables: `account`, `station`, `train`, `route_stop`, `schedule`, `class_config`, `reservation`, `traveller`. Booking transactions, unique request IDs/PNRs and a partial unique seat index prevent partial bookings and duplicate seat assignment. Business logic depends on repository interfaces, allowing the same tests to run with in-memory fakes and real SQLite.

Inspect a demo database without exposing password hashes:

```sh
python3 tools/inspect_database.py demo.db
```

## Tests, reports and reproducibility

```sh
ctest --test-dir build-final -L unit --output-on-failure
ctest --test-dir build-final -L integration --output-on-failure
./build-final/tests/rrs_security_tests
./build-final/tests/rrs_integration_tests --gtest_filter='*FinalIntegration*'
python3 tools/system_validation.py build-final/rrs_app
bash scripts/final_verify.sh
```

The last command runs normal regression, real-executable system tests, static analysis, ASan/UBSan and GCC/gcovr coverage, then writes raw evidence under `docs/reports/`. It uses separate `build-final-sanitize` and `build-final-coverage` directories for those runs. Run `python3 tools/generate_test_reports.py` afterward to regenerate the human-readable reports and test catalogue from that evidence.

Reports: [Unit tests](docs/unit-test-report.md), [System validation](docs/system-validation-report.md), [Code coverage](docs/code-coverage-report.md), [Branch coverage](docs/branch-coverage-report.md), [Coverage browser](docs/reports/coverage.html), [Defects and fixes](docs/defect-report.md). Actual execution dates and tested source revision appear in the reports; earlier sprint reports are historical snapshots rather than final release totals.

GitHub Actions runs normal and sanitizer tests and a Docker build/test/static-analysis/coverage pipeline. Jenkinsfile provides the same Docker pipeline; a separately hosted Jenkins server is not required to run the application and was not exercised in the local verification.

## Security and operational limits

Passwords use PBKDF2-HMAC-SHA256 (OpenSSL), 600,000 iterations, random 16-byte salts and constant-time comparison. Five failed logins lock an account for five minutes. Private process-local session state binds role/account/token and provides a renewable 15-minute idle deadline; logout revokes copied sessions. Profile views redact credentials and mask IDs. Reservation traveller IDs are cleared more than 90 days after the journey.

SQL values use bound parameters. UTC audit entries are sanitized, append-only and hash-chained; externally anchor a final hash to detect complete replacement/truncation. Audit I/O errors are explicit operational failures; mutations committed immediately before an audit failure may need reconciliation. POSIX database/audit modes are 0600, but Windows mounted drives also depend on host ACLs. One process per database/audit pair is supported. Schedule times use the machine's local timezone. This is not a distributed or live ticketing deployment.

## Minimal browser frontend

Run `Start-Railway-Web.cmd` and open http://localhost:8765. The passenger frontend supports registration, login, train search, fare review, booking, tickets and cancellation using the existing C++ services. See [frontend setup](docs/web-frontend.md).
