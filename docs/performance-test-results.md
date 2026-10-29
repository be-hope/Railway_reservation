# Performance, soak and retry test results

Issue #7 (part of #42). Test cases PERF-01, PERF-02, REL-01 and REL-02 from
the Software Test Plan, plus extra retry and long-run checks.

## How to run

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build -L perf --output-on-failure
./build/tests/rrs_perf_tests        # prints P50 / P95 / max
```

`tests/perf/large_data.cpp` generates the STP §6 large database from a fixed
seed: the seed data plus 40 stations, 200 trains (4 to 9 stops each, SL 4×72
and 3A 2×64, RAC 4, WL 20), 30 days of schedules and 20,000 confirmed
reservations with allocated seats. The tests use a real SQLite file.

## Results (29-10-2026)

Environment: Docker `gcc:13` image (Debian, GCC 13, SQLite 3.40) on a
Windows 11 laptop; Debug build; database file on the container's file system.

| Test | Requirement | Measured | Target | Result |
|------|-------------|----------|--------|--------|
| Large-data generation | STP §6 | 200 trains, 40 stations, 20,000 reservations in 4.3 s | - | Done |
| PERF-01 search (100 sequential queries, varied legs and dates) | NFR-001 | P50 0.57 ms, P95 0.91 ms, max 1.15 ms; 100 of 100 within 2.0 s | ≥ 95 of 100 within 2.0 s | Pass |
| PERF-02 availability (50 queries) | NFR-001, FR-009 | P50 0.14 ms, P95 0.31 ms, max 0.39 ms | P95 ≤ 2.0 s | Pass |
| PERF-02 booking (50 bookings) | NFR-001 | P50 7.06 ms, P95 20.96 ms, max 36.38 ms | P95 ≤ 2.0 s | Pass |
| Repeated search (500 runs of one leg) | NFR-001 | P95 0.51 ms first 50, 0.53 ms last 450 | No slowdown | Pass |
| REL-01 soak, 1,000 mixed operations (search, availability, booking, ticket, cancel) | NFR-002 | 1,000 of 1,000 completed normally; no crash; trip invariants and DB integrity check pass | ≥ 99 % normal | Pass |
| REL-02 lost confirmation + process restart, 3 retries with the same token | NFR-004 | Same PNR every time; 1 reservation; 1 seat used | No duplicate | Pass |
| 3 storage faults before COMMIT, then success, then 20 duplicate submissions | NFR-004 | 4 attempts; 1 reservation; first seat B1-01 (no seat leaked) | No duplicate | Pass |
| Fill 20001 SL with 320 single bookings | FR-013, FR-018 | 288 confirmed, 20 waitlisted, 12 rejected (E3001); no duplicate PNR or seat | - | Pass |

Timings change from machine to machine; the margin to the 2-second target is
more than three orders of magnitude for search and two for booking.

## Other quality checks run on the same code

| Check | Result |
|-------|--------|
| Full suite (unit, integration, perf) | 202 tests pass |
| AddressSanitizer + UBSan build (`-DRRS_SANITIZE=ON`) | All tests pass, 0 sanitizer reports |
| `-Wall -Wextra -Wpedantic -Werror` | 0 warnings |
| gcov line coverage, business layer (`booking/`, `cancel/`, `admin/`, `common/`) | 94.8 % lines |
| gcov branch coverage, same files | 58 % as counted by gcov 13, which includes the hidden exception-unwinding branches of C++ calls; lcov 1.16 in the image cannot exclude them. Most remaining branches are storage-error (E9001) return paths. NFR-005 asks for 70 % - recheck with `gcovr --exclude-throw-branches` in CI (#23). |
