# Integration Defect and Regression Report

Actual verification: 2026-11-17T14:14:19.556709+00:00. Tested source commit: `55bfc5f7fb2c22c6812fea3be2b0b72ffb18c642`. Platform: Ubuntu 24.04 / GCC 13 / SQLite / OpenSSL / GoogleTest under WSL. Reports were produced from execution artifacts, not estimated from source.

| Defect | Before | Correction | Evidence |
| --- | --- | --- | --- |
| Missing/incompatible train admin interface | Integrated build failed on missing admin/train_admin_service.h and incompatible repository/result calls | Added shared-interface header/service and replaced placeholder main with real auth/role menus | build-before.log; final build.log |
| Inactive train still exposed availability | Configuration/schedule lookup succeeded after deactivation | Verify active train before availability; reject invalid same-station quotes | regression-before/after.log |
| Staff cancellation incorrectly forbidden | Cancellation allowed only Passenger despite role matrix | Permit authenticated Staff/Admin; preserve passenger ownership and deny Ops | regression-before/after.log; system staff/admin cases |
| Seed/auth merge conflict | Different seed signatures and disabled credentials conflicted with runnable demo | Preserve idempotent sample seeding, optional hash-backed demo logins and existing accounts | seed unit/integration cases; executable auth cases |
| Administrator route editing accepted no signed sentinel | Numeric input parser could not read -1 origin/terminus times | Bounded signed integer input when field range permits negative values | system ST_ADM_01 |
| Shared performance/reliability DB paths | Concurrent test runs interfered with temporary files | Unique per-process random filenames | final performance/reliability pass logs |

The before/fix comparison uses archived pre-fix service fixtures with the current new regression tests in an isolated source copy. Four failing executions represent two parameterized scenario names on memory and SQLite; they are intentionally failing evidence, not failures in the current release. Reproduce with `python3 tools/reproduce_regressions.py`. The current regressions pass and are part of the full suite.

Known limits: the verified deployment is one local process with SQLite and file audit, WSL/GCC is the recorded platform, a separately hosted Jenkins/SonarQube service and native Windows build were not exercised, and there is no real railway/payment integration. Audit truncation needs an external anchor; Windows ACLs remain relevant on mounted drives.
