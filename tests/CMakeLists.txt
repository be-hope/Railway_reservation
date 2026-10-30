# GoogleTest: system package if installed, otherwise fetched at configure time.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)

# Unit tests: business logic only, linked without the data-access layer, so
# no file or database can be opened (NFR-006 / TST-01).
add_executable(rrs_unit_tests
  unit/test_money_date.cpp
  unit/test_fare_policy.cpp
  unit/test_seat_allocator.cpp
  unit/test_pnr_generator.cpp
  unit/test_availability.cpp
  unit/test_booking_validator.cpp
  unit/test_waitlist_plan.cpp
)
target_link_libraries(rrs_unit_tests PRIVATE rrs_core GTest::gtest_main)
gtest_discover_tests(rrs_unit_tests PROPERTIES LABELS unit)

# Shared helpers: in-memory fakes, storage backends, fault injection.
add_library(rrs_test_support STATIC
  fakes/in_memory_store.cpp
  support/test_support.cpp
)
target_include_directories(rrs_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rrs_test_support PUBLIC rrs_dal rrs_tools GTest::gtest)

# Integration tests: services against both the fakes and real SQLite.
add_executable(rrs_integration_tests
  integration/test_sqlite_schema.cpp
  integration/test_repositories.cpp
  integration/test_booking_service.cpp
  integration/test_passenger_menu.cpp
  integration/test_seat_release.cpp
  integration/test_schedule_coach_service.cpp
  integration/test_report_service.cpp
)
target_link_libraries(rrs_integration_tests PRIVATE rrs_test_support GTest::gtest_main)
gtest_discover_tests(rrs_integration_tests PROPERTIES LABELS integration)

# Performance, soak and retry tests (PERF-01/02, REL-01/02) on a generated
# large database: 200 trains, 40 stations, 20,000 reservations.
add_executable(rrs_perf_tests
  perf/large_data.cpp
  perf/test_performance.cpp
  perf/test_reliability.cpp
)
target_link_libraries(rrs_perf_tests PRIVATE rrs_test_support GTest::gtest_main)
gtest_discover_tests(rrs_perf_tests PROPERTIES LABELS perf DISCOVERY_TIMEOUT 60)
