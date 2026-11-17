# Branch Coverage Report

Actual verification: 2026-11-17T14:14:19.556709+00:00. Tested source commit: `55bfc5f7fb2c22c6812fea3be2b0b72ffb18c642`. Platform: Ubuntu 24.04 / GCC 13 / SQLite / OpenSSL / GoogleTest under WSL. Reports were produced from execution artifacts, not estimated from source.

**Raw compiled production branch coverage: 3976/7737 (51.39%)**. This raw GCC figure includes compiler-generated exception/error paths.

**Business decision branch coverage: 1771/2245 (78.89%)**. SRS NFR-005 target: >=70% business-layer branches. Recorded status: **MET**. This second view uses the same gcov data restricted to the documented business-layer files, with only branches explicitly marked `throw=true` by GCC/gcovr removed by `tools/extract_business_coverage.py`. This explicit exception-edge exclusion does not delete untested business decisions. The derived business-coverage.json preserves every retained branch for audit. The unfiltered result remains above and in coverage.json.

Both outcomes of validation, authorization, capacity/status decisions, refund 48h/12h boundaries, ownership, expiry/lockout and error/rollback paths are exercised. Remaining uncovered branches appear in the HTML; coverage is evidence of execution, not a proof that every requirement is correct.

Evidence: [source HTML](reports/coverage.html), [raw summary](reports/coverage-summary.json), [business summary](reports/business-coverage-summary.json), [business XML](reports/business-coverage.xml).
