#!/usr/bin/env bash

set -e

echo "===== Railway Reservation CI ====="

echo "[1/4] Configuring project..."

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DRRS_BUILD_TESTS=ON \
    -DRRS_WERROR=ON

echo "[2/4] Building project..."

cmake --build build --parallel 2

echo "[3/4] Running automated tests..."

ctest --test-dir build --output-on-failure

echo "[4/4] Running static analysis..."

cppcheck \
    --enable=warning,performance,portability \
    --error-exitcode=1 \
    --suppress=missingIncludeSystem \
    --std=c++17 \
    -I src -I tools -I tests \
    src tools tests/fakes tests/support

echo "===== CI COMPLETED SUCCESSFULLY ====="