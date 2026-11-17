#!/usr/bin/env bash
set -Eeuo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
REPORTS="$ROOT_DIR/docs/reports"
mkdir -p "$REPORTS"
printf '%s\n' 'Verification running; inspect logs until the completion marker is replaced.' > "$REPORTS/verification-complete.txt"
REVISION="$(git rev-parse --short HEAD)"
SAN_BUILD="$ROOT_DIR/build-final-sanitize"
COV_BUILD="$ROOT_DIR/build-final-coverage"
printf '%s\n' "Testing source revision $REVISION" > "$REPORTS/execution-environment.txt"
git rev-parse HEAD >> "$REPORTS/execution-environment.txt"
date -u +'%Y-%m-%dT%H:%M:%SZ' >> "$REPORTS/execution-environment.txt"
g++ --version | head -1 >> "$REPORTS/execution-environment.txt"
cmake --version | head -1 >> "$REPORTS/execution-environment.txt"
gcovr --version | head -1 >> "$REPORTS/execution-environment.txt"
cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Debug > "$REPORTS/build.log" 2>&1
cmake --build build-final -j 4 >> "$REPORTS/build.log" 2>&1
for suite in unit integration perf security; do
    "$ROOT_DIR/build-final/tests/rrs_${suite}_tests" --gtest_output="xml:$REPORTS/normal-$suite.xml" > "$REPORTS/normal-$suite.log" 2>&1
done
python3 tools/collect_test_xml.py
python3 tools/system_validation.py build-final/rrs_app "$REPORTS/system-results.json" > "$REPORTS/system-validation.log" 2>&1
python3 tools/verify_demo.py build-final/rrs_app > "$REPORTS/demo-smoke.log" 2>&1
cppcheck --enable=warning,performance,portability --error-exitcode=1 --suppress=missingIncludeSystem --inline-suppr --std=c++17 -Isrc src tools > "$REPORTS/static-analysis.log" 2>&1
cmake -S . -B "$SAN_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRRS_SANITIZE=ON > "$REPORTS/sanitizer-build.log" 2>&1
cmake --build "$SAN_BUILD" -j 4 >> "$REPORTS/sanitizer-build.log" 2>&1
for suite in unit integration perf security; do
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$SAN_BUILD/tests/rrs_${suite}_tests" --gtest_output="xml:$REPORTS/sanitizer-$suite.xml" > "$REPORTS/sanitizer-$suite.log" 2>&1
done
cmake -S . -B "$COV_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRRS_COVERAGE=ON > "$REPORTS/coverage-build.log" 2>&1
cmake --build "$COV_BUILD" -j 4 >> "$REPORTS/coverage-build.log" 2>&1
for suite in unit integration perf security; do
    "$COV_BUILD/tests/rrs_${suite}_tests" --gtest_output="xml:$REPORTS/coverage-$suite.xml" > "$REPORTS/coverage-$suite.log" 2>&1
done
python3 tools/system_validation.py "$COV_BUILD/rrs_app" "$REPORTS/coverage-system-results.json" > "$REPORTS/coverage-system.log" 2>&1
gcovr --root "$ROOT_DIR" --filter 'src/.*' --filter 'tools/.*' --json "$REPORTS/coverage.json" --json-summary "$REPORTS/coverage-summary.json" --xml-pretty --xml "$REPORTS/coverage.xml" --html-details "$REPORTS/coverage.html" --html-self-contained --print-summary "$COV_BUILD" > "$REPORTS/coverage-summary.log" 2>&1
"$ROOT_DIR/build-final/tests/rrs_integration_tests" --gtest_filter='*FinalIntegration.InactiveTrainHasNoAvailabilityAndSameStationHasNoFare*:*FinalIntegration.StaffCancelsWithRefundAndOpsIsDenied*' --gtest_output="xml:$REPORTS/regression-after.xml" > "$REPORTS/regression-after.log" 2>&1
python3 tools/extract_business_coverage.py > "$REPORTS/business-coverage-summary.log" 2>&1
python3 -c 'import datetime,json,pathlib,subprocess; pathlib.Path("docs/reports/run-metadata.json").write_text(json.dumps({"tested_commit":subprocess.check_output(["git","rev-parse","HEAD"],text=True).strip(),"executed_at":datetime.datetime.now(datetime.timezone.utc).isoformat()},indent=2)+"\n")'
printf '%s\n' 'FINAL VERIFICATION PASSED' > "$REPORTS/verification-complete.txt"
