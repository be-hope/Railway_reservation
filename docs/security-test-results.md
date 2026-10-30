# Security and integration verification

Verified on 30 October 2026 using Ubuntu 24.04 under WSL, GCC 13.3, CMake 3.28, SQLite 3.45, OpenSSL 3.0, GoogleTest 1.14 and cppcheck 2.13.

## Results

- Debug build: strict compiler warnings treated as errors; passed.
- Sequential CTest: **217/217 passed**, with no skips (51 unit, 143 integration, 8 performance/reliability, 15 security).
- Interactive executable smoke check: passed. Covers login, two-traveller AC3 fare of Rs 1,750, ownership-restricted history/PNR, database restart, staff correction, and secret-free audit output. Reproduce with `python3 tools/verify_demo.py` after building.
- PBKDF2 compatibility: passed against a known hash independently generated using Python `hashlib.pbkdf2_hmac`, with a fixed 16-byte salt and 600,000 iterations.
- ASan/UBSan: **217/217 passed**, running all four test executables sequentially with leak detection and halt-on-error enabled; no sanitizer findings. The earlier 214-test CTest sanitizer run also passed before the last three regressions were added.
- cppcheck: warning, performance and portability checks passed across all 37 production/demo translation units, with no findings.

An initial parallel CTest run skipped three performance cases because the existing performance fixture used a shared temporary filename. The performance and reliability fixtures now create unique databases per process, preventing interference between concurrent runs. GoogleTest discovery uses a 60-second timeout to accommodate sanitizer startup on WSL-mounted drives. The complete sequential run executes all eight performance/reliability cases successfully. Use sequential CTest for stable timing measurements as documented in README and CI.

## Security test traceability

| Test-plan ID | Automated evidence |
| --- | --- |
| SEC-T01 | Different random salts/hashes for identical passwords; hash-only SQLite storage; constant-time verification; redacted profile output; independent PBKDF2 vector |
| SEC-T02 | Four roles across all nine permission categories; ownership restrictions; passenger denied staff operations |
| SEC-T03 | Mobile/email/password/name/age/gender/ID/station/PNR boundaries; long/control/SQL-meta/format-string inputs; existing booking validation suite |
| SEC-T04 | Empty session, changed account/role/token/expiry, 900-second idle boundary, activity renewal and logout-copy revocation |
| SEC-T05 | Security audit events; append-only records; control/separator sanitization; edited-record detection |
| SEC-T06 | Five failures; correct password rejected at 299 seconds; accepted at 300; failure counter reset; lockout survives reopen |
| SEC-T07 | Masked profile/history IDs; 90/91-day retention boundary; idempotent cleanup |
| SEC-T08 | Foreign PNR rejected, SQL-meta PNR rejected, own history only, profile identity/role escalation rejected |

## Assigned-issue scope

The GitHub assignee is `be-hope`. There are 14 open assigned issues, including umbrella issues; several foundation pieces already existed in main and are extended rather than replaced here.

| Issues | Implementation and evidence | State in this branch |
| --- | --- | --- |
| #1, #58, #59 | AuthService, OpenSSL PasswordHasher, durable uniqueness/lockout, role-bound renewable/revocable sessions; auth/security tests | Implemented |
| #6 | ProfileService with validation, field whitelist, durable updates, masked IDs and no exposed hashes/salts; reopen test | Implemented |
| #16, #17 | Existing CMake/src/Result/error catalogue extended with security sources, strict warnings, stack protection and documented dependency setup; existing Result/error tests retained | Implemented/verified |
| #18 | Shared InputValidator for required fields; booking traveller/station/date validation now uses it | Implemented |
| #19 | AccessControl permission matrix; existing service guards now verify private session state; chained append-only AuditLogger with explicit failure and POSIX file protection | Implemented |
| #29 | Booking validation enforces 1-6 travellers, valid journey data and shared field rules before mutations | Implemented/verified |
| #33 | Production BookingHistoryService restricts account, sorts date/PNR, and masks IDs | Implemented |
| #57 | Staff booking delegates to transactional BookingService; corrections validate role, PNR, status, departure and traveller count, preserve seats/fare/owner, and audit success | Implemented |
| #8 | SEC-T01-T08 executable, sanitizer configuration, cppcheck and GitHub Actions regression workflow | Implemented; verification results above |
| #45 | README clean-machine build/run instructions, reproducible CLI smoke check, this report, and final-demo.md | Implemented |
| #40 | Auth/profile/history/staff integration and security validation delivered; final release depends on other members' features | **Keep open** |

## Remaining release work and limits

- PR #60 (train), #62 (seat/fare), and #63 (train administration/application integration) are other assigned work and were not merged or modified here. Their integration still needs review against the current shared domain/repository contracts.
- Final cancellation validation/refund work (#47/#48 and related #46/#44) is not implemented by the current seat-release helper. The login-based executable does not wire that interim cancellation adapter, so it cannot offer an incorrect cancellation/refund result. This blocks a complete final-release claim for #40.
- The demo exposes passenger flows and limited staff/administrator reservation actions. Reports/configuration exist as services; the team's final role-menu integration remains separate.
- Audit chains detect record edits/reordering; detecting truncation/full replacement requires an externally anchored final hash. Existing unchained logs need to be archived before use. Audit I/O failure is surfaced, but a previously committed mutation may need reconciliation.
- The app supports one process per database/audit pair. POSIX file modes do not replace Windows host ACLs. Native Windows/MSVC and external SonarQube were not run. SHA-256 and PBKDF2 use the installed OpenSSL implementation rather than custom cryptography.

Issues remain open on GitHub until reviewed changes are merged. Only the implemented issues above should be closed by the change; #40 must remain open with these explicit release dependencies.
