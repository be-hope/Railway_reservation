# Code Coverage Report

Actual verification: 2026-11-17T14:14:19.556709+00:00. Tested source commit: `55bfc5f7fb2c22c6812fea3be2b0b72ffb18c642`. Platform: Ubuntu 24.04 / GCC 13 / SQLite / OpenSSL / GoogleTest under WSL. Reports were produced from execution artifacts, not estimated from source.

**Compiled production C++ line coverage: 2731/2858 (95.56%)**. Function coverage: 531/574 (92.51%). The denominator includes instrumented C++ in `src/` and `tools/`, including the application/menus; it excludes test sources, third-party libraries, Python automation and historical unlinked root prototypes.

**Business-layer line coverage: 1268/1298 (97.69%)**. The SRS NFR-005 target is >=80% on the business layer. Recorded status: **MET**. Business filters include auth/booking/cancel/admin/security and common domain/date/money/result/session/clock; SQLite, UI, seed/main and file-backed audit I/O are reported in the whole-production figure instead.

Measured using GCC `--coverage -O0`, all four C++ executables and the system scenarios, then gcovr 7.0. Counts are union coverage, not averages across files or test counts.

Evidence: [HTML with source details](reports/coverage.html), [raw gcovr JSON](reports/coverage.json), [summary JSON](reports/coverage-summary.json), [Cobertura XML](reports/coverage.xml), [business summary](reports/business-coverage-summary.json). The full command is in scripts/final_verify.sh. A green test suite does not imply every line is executed.
