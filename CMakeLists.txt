cmake_minimum_required(VERSION 3.16)
project(RailwayReservationSystem LANGUAGES C CXX)

# Build settings from SAD §3.6 / NFR-007 (PORT-01).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RRS_WERROR "Treat warnings as errors" ON)
option(RRS_SANITIZE "Build with AddressSanitizer + UBSan" OFF)
option(RRS_COVERAGE "Build with gcov instrumentation" OFF)
option(RRS_BUILD_TESTS "Build the GoogleTest suites" ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# SQLite (ADR-002). Use a vendored amalgamation in third_party/sqlite if it is
# present (sqlite3.c + sqlite3.h from sqlite.org), otherwise the system library
# (e.g. apt install libsqlite3-dev).
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
  set(RRS_SQLITE_DIR ${CMAKE_SOURCE_DIR}/third_party/sqlite)
else()
  find_package(SQLite3 REQUIRED)
endif()

if(RRS_SQLITE_DIR)
  add_library(sqlite3_vendored STATIC ${RRS_SQLITE_DIR}/sqlite3.c)
  target_include_directories(sqlite3_vendored PUBLIC ${RRS_SQLITE_DIR})
  target_compile_definitions(sqlite3_vendored PRIVATE SQLITE_THREADSAFE=0 SQLITE_DEFAULT_FOREIGN_KEYS=1)
  set(RRS_SQLITE_TARGET sqlite3_vendored)
else()
  set(RRS_SQLITE_TARGET SQLite::SQLite3)
endif()

# ---------------------------------------------------------------------------
# Common compile options for our own code only (not third-party sources).
# ---------------------------------------------------------------------------
add_library(rrs_options INTERFACE)
target_compile_options(rrs_options INTERFACE -Wall -Wextra -Wpedantic)
if(RRS_WERROR)
  target_compile_options(rrs_options INTERFACE -Werror)
endif()
if(RRS_SANITIZE)
  target_compile_options(rrs_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(rrs_options INTERFACE -fsanitize=address,undefined)
endif()
if(RRS_COVERAGE)
  target_compile_options(rrs_options INTERFACE --coverage -O0)
  target_link_options(rrs_options INTERFACE --coverage)
endif()

# Business layer: no file or database access (NFR-006 / TST-01).
add_library(rrs_core STATIC
  src/common/date.cpp
  src/common/clock.cpp
  src/common/domain.cpp
  src/common/money.cpp
  src/common/result.cpp
  src/common/session.cpp
)
target_include_directories(rrs_core PUBLIC src)
target_link_libraries(rrs_core PUBLIC rrs_options)

# Data access layer (C7) and file-backed audit log.
add_library(rrs_dal STATIC
  src/common/audit.cpp
  src/dal/schema.cpp
  src/dal/sqlite_database.cpp
  src/dal/sqlite_train_repository.cpp
  src/dal/sqlite_config_repository.cpp
  src/dal/sqlite_reservation_repository.cpp
  src/dal/sqlite_user_repository.cpp
)
target_link_libraries(rrs_dal PUBLIC rrs_core ${RRS_SQLITE_TARGET})

# Seed data shared by the tests (and later the demo).
add_library(rrs_tools STATIC
  tools/seed_data.cpp
)
target_include_directories(rrs_tools PUBLIC tools)
target_link_libraries(rrs_tools PUBLIC rrs_core)

if(RRS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
