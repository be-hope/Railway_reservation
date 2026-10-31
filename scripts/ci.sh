#!/usr/bin/env bash

set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-ci}"
ARTIFACTS_DIR="${ARTIFACTS_DIR:-$ROOT_DIR/artifacts}"
BUILD_JOBS="${BUILD_JOBS:-2}"

mkdir -p "$BUILD_DIR" "$ARTIFACTS_DIR"

echo "===== Railway Reservation Docker CI ====="

for tool in cmake ctest cppcheck gcovr; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: Required tool not found: $tool" >&2
        exit 1
    fi
done

echo "[1/5] Configuring C++17 project with coverage..."

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DRRS_BUILD_TESTS=ON \
    -DRRS_WERROR=ON \
    -DRRS_COVERAGE=ON

echo "[2/5] Building all targets..."

cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS"

echo "[3/5] Running unit, integration and performance tests..."

ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "[4/5] Running Cppcheck static analysis..."

cppcheck \
    --enable=warning,performance,portability \
    --error-exitcode=1 \
    --suppress=missingIncludeSystem \
    --std=c++17 \
    -I "$ROOT_DIR/src" \
    -I "$ROOT_DIR/tools" \
    -I "$ROOT_DIR/tests" \
    "$ROOT_DIR/src" \
    "$ROOT_DIR/tools" \
    "$ROOT_DIR/tests/fakes" \
    "$ROOT_DIR/tests/support"

echo "[5/5] Generating code coverage reports..."

gcovr \
    --root "$ROOT_DIR" \
    --filter 'src/.*' \
    --filter 'tools/.*' \
    --print-summary \
    --xml-pretty \
    --xml "$ARTIFACTS_DIR/coverage.xml" \
    --html-details "$ARTIFACTS_DIR/coverage.html" \
    "$BUILD_DIR"

echo ""
echo "===== CI COMPLETED SUCCESSFULLY ====="
echo "Coverage XML:  $ARTIFACTS_DIR/coverage.xml"
echo "Coverage HTML: $ARTIFACTS_DIR/coverage.html"
