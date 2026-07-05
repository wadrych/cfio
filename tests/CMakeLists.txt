find_package(GTest REQUIRED)
include(GoogleTest)

enable_testing()

add_executable(cfio-tests
  test_smoke.cpp
  test_size_parser.cpp
  test_job_config.cpp
  test_config_validator.cpp
  test_config_parser.cpp
  test_aligned_buffer.cpp
  test_engine_interface.cpp
  test_log2_histogram.cpp
  test_offset_generator.cpp
  test_io_direction_decider.cpp
  test_worker_thread.cpp
  test_metrics_aggregator.cpp
  test_file_preparator.cpp
)

target_link_libraries(cfio-tests
  PRIVATE GTest::gtest
  PRIVATE cfio-core
)

gtest_discover_tests(cfio-tests)
