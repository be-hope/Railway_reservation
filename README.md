# Railway Reservation System

C++17 console application with SQLite storage, passenger booking, role-based services, PBKDF2 authentication, and security regression tests.

## Build on Ubuntu 24.04 / WSL

Requires GCC 11 or newer, CMake 3.16+, OpenSSL Crypto, SQLite3, and GoogleTest. The Windows MinGW 6 compiler is too old; use Ubuntu in WSL. From a Windows terminal, run `wsl -d Ubuntu`, then change to this repository's Linux path.

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build libsqlite3-dev libssl-dev libgtest-dev cppcheck
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

GoogleTest is fetched from its official repository only if a system package is unavailable. Builds otherwise use installed dependencies. Production code compiles with strict warnings, warnings as errors, and stack protection. Native MSVC warning flags are supported, but the verified platform is GCC 13 on Ubuntu 24.04.

## Run

```sh
./build/rrs_passenger_demo
```

Choose Register to create a passenger account, then Login using its mobile number or email. Registration requires a name, an unused mobile or email, and an 8-128 character password containing a letter and a digit. Password entry is hidden on a terminal. Profile details can be completed after login. Data persists in `rrs_demo.db`; security events append to `audit.log` in the current directory.

For sample trains and explicitly enabled test accounts, use a **new database file**:

```sh
./build/rrs_passenger_demo demo.db --seed-demo
```

| Role | Login | Password |
| --- | --- | --- |
| Passenger | 9876500001 | Test@1234 |
| Passenger | 9876500002 | Test@1234 |
| Staff | staff01 | Test@1234 |
| Administrator | admin01 | Test@1234 |
| Operations | ops01 | Test@1234 |

These credentials are development fixtures only. Seeding is opt-in and does not overwrite an existing database or enable disabled accounts in an older demo database. Every enabled account has its own randomly salted hash. Normal registration always creates a passenger, never a privileged account.

Passenger screens support train search, schedules, fare quotes, availability, 1-6 traveller booking, own booking history, PNR lookup, and profile updates. Staff can book one traveller on behalf of a passenger and correct an existing traveller's name through the demo; the service accepts corrections for all travellers. Staff seed passenger IDs are `acc-p1` and `acc-p2`. Role checks also apply to direct service calls.

## Security

- Passwords use OpenSSL PBKDF2-HMAC-SHA256 with 600,000 iterations, a 16-byte OS-random salt, and constant-time digest comparison. Only 64-character hash and 32-character salt hex strings persist. The work factor is currently fixed by the storage contract; changing it needs a migration.
- Five consecutive wrong passwords lock an account for five minutes. Success resets the counter. Unknown users and wrong passwords return the same invalid-credentials error and perform the same PBKDF2 work.
- Sessions carry private process-local state binding token, account, role, and expiry. Changing public fields cannot grant access. Successful authorized operations extend the idle deadline by 15 minutes. Logout revokes all copies; restarting the process invalidates sessions.
- Profile and history services restrict passengers to their own account. Profile output removes hashes/salts and masks ID numbers. Updates cannot change role or credentials. SQL uses bound parameters.
- Audit records have UTC timestamps, sanitized fields, and a SHA-256 chain. Edited or reordered records are detected before another append. Archive any legacy unchained audit log before upgrading. File-open/write errors are explicit exceptions; callers must treat those as operational failures. Mutations committed before an audit failure may require reconciliation.
- On POSIX filesystems, database and audit files are owner-only (0600). Windows/WSL mounted-drive access still depends on host ACLs. Use a protected directory and one application process per database/audit pair.
- Startup clears reservation traveller IDs strictly more than 90 days after the journey, without deleting tickets or audit records. Account profile IDs remain until the passenger updates them. Schedule dates/times use the machine's local timezone; audit timestamps use UTC.

A hash chain needs an independently stored final hash to detect truncation or complete replacement. It does not protect against a privileged attacker rewriting the entire log. Production account provisioning, distributed sessions, and multi-process audit coordination are outside this console project's scope.

## Verification

```sh
./build/tests/rrs_security_tests
python3 tools/verify_demo.py
cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRRS_SANITIZE=ON
cmake --build build-sanitize --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure
cppcheck --enable=warning,performance,portability --error-exitcode=1 --suppress=missingIncludeSystem --inline-suppr -Isrc src tools
```

Run CTest sequentially for stable performance measurements. Performance fixtures use a unique temporary database for each process. See [test results](docs/security-test-results.md) and the [final demo script](docs/final-demo.md). GitHub Actions runs normal and sanitizer builds plus static analysis on pushes and pull requests.

## Module integration

`AuthService`, `ProfileService`, `BookingHistoryService`, `InputValidator`, `AccessControl`, and `ReservationAdminService` extend the existing `DataAccess`, `Session`, and `Result<T>` contracts. The login-based executable wires the production profile/history services, replacing the temporary adapters in its runtime path. Test code uses a dedicated session fixture friend; application code can issue sessions only through authentication.

Train administration/integration PR #63, train module PR #60, and seat/fare PR #62 belong to other assigned work and are not merged by this change. The demo uses the existing repository train adapter. The cancellation module's final departure/refund implementation is still separate work; cancellation is deliberately unavailable in this executable until that service is integrated. See [scope and issue traceability](docs/security-test-results.md).
