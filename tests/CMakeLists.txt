find_package(GTest REQUIRED)
include(GoogleTest)

enable_testing()

add_executable(cfio-tests
  test_smoke.cpp
  test_size_parser.cpp
  test_job_config.cpp
)

target_link_libraries(cfio-tests
  PRIVATE GTest::gtest_main
  PRIVATE cfio-core
)

gtest_discover_tests(cfio-tests)
