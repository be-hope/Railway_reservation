# System Validation Report

Actual verification: 2026-11-17T14:14:19.556709+00:00. Tested source commit: `55bfc5f7fb2c22c6812fea3be2b0b72ffb18c642`. Platform: Ubuntu 24.04 / GCC 13 / SQLite / OpenSSL / GoogleTest under WSL. Reports were produced from execution artifacts, not estimated from source.

The Python driver runs the compiled `rrs_app`, supplies menu inputs, opens the resulting SQLite database read-only for assertions, and restarts the app to verify persistence. Each scenario creates a fresh temporary database, avoiding dependence on earlier runs. Demo credentials are test fixtures.

| Scenario | Result | Seconds |
| --- | --- | ---: |
| `test_ADMIN_cancellation_is_authorized` | PASS | 2.204 |
| `test_AUTH_invalid_login_and_secret_free_audit` | PASS | 1.818 |
| `test_AUTH_registration_login_and_restart` | PASS | 2.591 |
| `test_FR025_ops_reports_match_stored_booking` | PASS | 2.553 |
| `test_IT_CAN_03_waitlist_promotes_after_cancellation` | PASS | 3.267 |
| `test_IT_STF_01_02_staff_book_correct_and_cancel` | PASS | 2.041 |
| `test_MNT_restart_retains_ticket` | PASS | 2.262 |
| `test_OPS_cannot_modify_trains` | PASS | 1.521 |
| `test_SEC_T08_foreign_history_and_pnr_are_denied` | PASS | 1.881 |
| `test_ST_ADM_01_admin_creates_bookable_train` | PASS | 2.033 |
| `test_ST_BKG_01_02_quote_booking_and_history` | PASS | 1.559 |
| `test_ST_CAN_01_cancel_refund_and_repeat_rejection` | PASS | 2.098 |

Coverage includes registration/login, quote/booking/history, foreign-PNR denial, refunds and repeated cancellation, waitlist promotion, staff booking/correction/cancellation, admin-created trains becoming bookable, admin cancellation, Ops reports/permission boundaries and restart persistence.

Reproduce: `python3 tools/system_validation.py build-final/rrs_app`. Evidence: [system JSON](reports/system-results.json), [system log](reports/system-validation.log), [demo smoke](reports/demo-smoke.log). This is automated system validation; subjective human usability and native Windows portability are not inferred from it.
