# Booking module (M3) and data-access layer

Owner: Catherine D. Covers SRS FR-009 to FR-014, NFR-003, NFR-004, NFR-006,
the shared data-access layer (component C7), and from Sprint 3 seat release
and waitlist promotion (FR-016, FR-019), schedule/coach management (FR-022),
operational reports (FR-025) and the performance/reliability tests.

## Build and test

Needs CMake 3.16+, a C++17 compiler (GCC 11+ or MinGW-w64) and SQLite 3
(`libsqlite3-dev`, or put `sqlite3.c`/`sqlite3.h` in `third_party/sqlite/`).
GoogleTest is used from the system if installed, otherwise downloaded by CMake.

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Options: `-DRRS_SANITIZE=ON` (ASan + UBSan), `-DRRS_COVERAGE=ON` (gcov),
`-DRRS_WERROR=OFF`. Test labels: `ctest -L unit`, `ctest -L integration`,
`ctest -L perf` (see `docs/performance-test-results.md`).

Passenger menu demo on the seed data (logged in as P1, Asha Rao):

```bash
./build/rrs_passenger_demo rrs_demo.db
```

## Layout

| Path | Contents |
|------|----------|
| `src/common/` | `Result<T>` and error codes, `Money` (paise), `Date`, injectable clock, domain records, session role guard, audit port |
| `src/dal/` | Repository interfaces, SQLite schema and repositories, unit of work |
| `src/booking/` | Availability and status decision, `SeatAllocator`, `PnrGenerator`, fare arithmetic, request validation, `BookingService` |
| `src/cancel/` | `WaitlistManager` (strict-order promotion), `SeatReleaseService` |
| `src/admin/` | `ScheduleCoachService` (schedules, coaches, RAC, waitlist), `ReportService` |
| `src/ui/` | Console I/O, ticket view, passenger menu, report layout |
| `tools/` | Seed data (STP §6) and the passenger demo with interim port adapters |
| `tests/unit/` | Pure business logic, linked without the data layer (TST-01) |
| `tests/integration/` | Services and repositories on both in-memory fakes and SQLite |
| `tests/perf/` | Large-data generator, PERF-01/02, REL-01/02 |

## Design decisions

- **Layering.** Services get a `DataAccess` bundle of repository interfaces
  through the constructor. The UI only talks to service ports. The same
  service tests run against the in-memory fakes and SQLite.
- **Integrity in the database.** `UNIQUE(pnr)`, `UNIQUE(request_id)` and the
  partial unique index `ux_seat` (one active traveller per seat per
  train/date/class) back up the application logic. Cancelling sets the
  seat number to `NULL`, which releases the seat.
- **One transaction per booking.** Status decision, seat allocation, PNR and
  insert happen inside `BEGIN IMMEDIATE`; any failure rolls everything back.
- **Idempotency (NFR-004).** The request token is looked up before and
  inside the transaction, so a retry returns the original PNR.
- **Queue fairness (BR-07).** A new booking is CONFIRMED only when nobody is
  waiting (RAC or waitlist), and RAC only when nobody is waitlisted, so a
  newcomer never takes a seat that a waiting ticket is queued for.
- **Strict-order promotion (FR-019, ADR-005).** Cancelling releases the seats
  and re-plans the queue in the same transaction: RAC tickets (booking
  order) then the waitlist (position order) are confirmed from the head
  while the whole party fits; the first that does not fit stops the walk.
  Then the waitlist head moves into free RAC slots, and positions are
  renumbered 1..k. A capacity increase (FR-022) runs the same promotion.
- **Capacity changes (FR-022).** Rejected with E5002 if any date from today
  would hold fewer seats than are confirmed, lose an occupied seat, or fall
  below the RAC travellers / waitlisted tickets already held.
- **Reports (FR-025)** are computed from the stored reservations through the
  repositories, and a test compares every figure with direct SQL.
- **Money** is integer paise; there is no floating point in fares.

## Status decision table

| Condition (n travellers) | Result |
|--------------------------|--------|
| n ≤ confirmed seats free and no RAC/WL tickets waiting | CONFIRMED, seats allocated |
| n ≤ RAC slots free and nobody waitlisted | RAC (no seat number) |
| waitlisted tickets < waitlist capacity | WAITLISTED, position = count + 1 |
| otherwise | ERR-NO-CAPACITY (E3001), nothing stored |

## Interfaces owned by other modules

Some pieces this module needs belong to teammates' issues. They are kept
small and behind interfaces so the owning module can replace them:

| Here | Owner issue | Replace with |
|------|-------------|--------------|
| `common/result.h` error codes | #17 | Keep codes stable; extend as needed |
| `booking/booking_request_validator` | #18, #29 | `InputValidator` |
| `common/session.h` `requireRole` | #19, #59 | `AccessControl::requirePermission` |
| `common/audit.h` file logger | #19 | `AuditLogger` |
| `booking/fare_policy` | #37, #38 | Shared `FareCalculator` |
| `tools/demo_adapters` (search, history, profile, cancel) | #10, #33, #6, #46-#48 | Real services behind the menu ports |
| `SeatReleaseService::cancelAndRelease` caller | #47, #48 | `CancellationService` checks owner, departure time and refund, then calls it |
| `tools/seed_data` | #22 | Official seed script |

## Test traceability

| Test case | Where |
|-----------|-------|
| UT-BKG-01 | `tests/unit/test_fare_policy.cpp` |
| UT-BKG-02 | `tests/unit/test_booking_validator.cpp` |
| UT-BKG-03 | `tests/unit/test_pnr_generator.cpp` |
| UT-BKG-04 | `tests/unit/test_seat_allocator.cpp` |
| UT-BKG-05 | `tests/unit/test_availability.cpp` |
| IT-BKG-01 to IT-BKG-06, ST-BKG-02 | `tests/integration/test_booking_service.cpp` |
| ST-BKG-01, USB-01 (scripted) | `tests/integration/test_passenger_menu.cpp` |
| Schema constraints | `tests/integration/test_sqlite_schema.cpp` |
| Repository contract | `tests/integration/test_repositories.cpp` |
| UT-CAN-04 and promotion rules | `tests/unit/test_waitlist_plan.cpp` |
| IT-CAN-01 (seat), IT-CAN-03, IT-CAN-04, IT-CAN-05, randomised queue order | `tests/integration/test_seat_release.cpp` |
| IT-ADM-04 | `tests/integration/test_schedule_coach_service.cpp` |
| IT-RPT-01, IT-RPT-02 | `tests/integration/test_report_service.cpp` |
| PERF-01, PERF-02 | `tests/perf/test_performance.cpp` |
| REL-01, REL-02 | `tests/perf/test_reliability.cpp` |
