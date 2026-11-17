# Updated Test Case Document

Actual verification: 2026-11-17T14:14:19.556709+00:00. Tested source commit: `55bfc5f7fb2c22c6812fea3be2b0b72ffb18c642`. Platform: Ubuntu 24.04 / GCC 13 / SQLite / OpenSSL / GoogleTest under WSL. Reports were produced from execution artifacts, not estimated from source.

This updates the earlier SRS/SAD/STP cases for the integrated sprint result. There are **267 C++ executions** and **12 system scenarios**; the exhaustive execution catalogue is [CSV](reports/test-case-catalogue.csv). IDs below group related cases and are not additional execution counts.

| Group | Requirement | Preconditions / action | Expected result | Implementation evidence |
| --- | --- | --- | --- | --- |
| AUTH-01 | FR-001/004, SEC-001 | Register unique mobile/email; retry duplicate; verify salted hash, never plaintext | Accepted unique account, duplicate E1002, distinct salts/hashes | test_security.cpp / SEC_T01 and UT_AUTH_02 |
| AUTH-02 | FR-002, SEC-004 | Try wrong credentials five times; advance fixed clock 299 then 300 seconds | Lockout rejects at 299s and accepts at 300s; success resets count | test_security.cpp / SEC_T06 |
| AUTH-03 | SEC-002/004 | Change session role/account/token/expiry, use empty/expired/revoked copy | No privilege escalation; E1005/E1006; idle renewals validated | test_security.cpp / SEC_T04 |
| PROFILE-01 | FR-003, SEC-005 | Update allowed profile fields; try foreign account or changing role/credentials | Valid fields persist, ID masked, ownership denied, credentials preserved | test_security.cpp / IT_AUTH_03; AuthPersistence |
| TRN-01 | FR-005/006 | Search valid/unknown/same/reverse stations and past/out-of-window dates | Only active matching service returned; invalid inputs rejected | test_final_integration.cpp / SearchRejectsUnknownSameAndOutOfWindowInputs |
| TRN-02 | FR-007/008 | Display ordered stops and per-class fare | Route/time/distance and fare agree with stored configuration | test_passenger_menu.cpp |
| BKG-01 | FR-009/011 | Quote 350km 3A for two travellers and compare persisted fare | Rs1750 total; stored as175000 paise | test_fare_policy.cpp; system ST_BKG_01_02 |
| BKG-02 | FR-010/012 | Validate 1-6 travellers, ages1-120, gender/ID/journey; decline quote | Invalid/refused data creates no reservation | test_booking_validator.cpp; test_booking_service.cpp |
| BKG-03 | FR-013/014, NFR-004 | Allocate seats, generate PNR, retry same request ID and inject storage fault | Unique active seats/PNR; retry returns original; failed transaction rolls back | test_booking_service.cpp; test_reliability.cpp |
| BKG-04 | FR-015/020, SEC-002 | Read own history/PNR then another passenger history/PNR | Sorted own history; foreign access E1006; invalid PNR E1001 | test_security.cpp / IT_BKG_07_SEC_T08; system SEC_T08 |
| CAN-01 | FR-016/017 | Cancel before/at/after departure and twice | Eligible cancellation succeeds once; departed/repeat rejected without mutation | test_cancellation_service.cpp; test_cancellation_eligibility.cpp |
| CAN-02 | FR-017, BR-06 | Refund boundaries48h,47h59m,12h,11h59m; waiting ticket | 90/75/50 percent before departure; waiting100percent | test_refund_calculator.cpp |
| CAN-03 | FR-018/019 | Fill capacity, waitlist next ticket, cancel confirmed ticket | Seat released and head promoted atomically; queue order preserved | test_seat_release.cpp; test_waitlist_plan.cpp; system IT_CAN_03 |
| ADM-01 | FR-021/022 | Add/rename/activate/deactivate train; validate route/schedule/class capacity | Admin only; invalid route rejected; booked load protects train/route/schedule | test_final_integration.cpp; test_schedule_coach_service.cpp |
| STF-01 | FR-023/024 | Staff books for passenger, corrects name and cancels; Ops attempts same | Authorized role succeeds, owner/seat/fare invariants preserved, Ops denied | test_security.cpp; test_final_integration.cpp; system IT_STF_01_02 |
| RPT-01 | FR-025 | View booking/occupancy/train-wise statistics and compare SQL totals | Admin/Ops figures match persistence; passenger denied | test_report_service.cpp; system FR025 |
| SEC-05 | SEC-003/005/006 | Hostile/long/control input, audit injection/tamper, masking and90/91-day retention | Safe validation, sanitized events, chain verification and retention boundary | test_security.cpp / SEC_T03, T05, T07 |
| PERF-01 | NFR-001/002 | Generated200 trains/40 stations/20000reservations;1000 mixed operations | Latency/reliability assertions in executed suite; no duplicated seats/tickets | test_performance.cpp; test_reliability.cpp |
| MNT-01 | NFR-005/006/007 | Clean build, normal/sanitizer tests, static checks and coverage | Recorded platform passes; coverage compared with80/70business thresholds | reports/ evidence; CI; platform limits recorded |

Changes since earlier sprints: real authentication replaces constructed sessions in the app; secure seed flags preserve deterministic test data; complete cancellation replaces the interim release-only adapter; admin sources use shared interfaces; final regression cases cover inactive availability, same-station fare, route protection, staff cancellation and search validation; system checks cover all four roles and process restart.

Executed outcomes are stored per case in the CSV/XML/JSON; they should be regenerated after any code change. Historical reports retain their original context and are superseded by this final run for release totals.
