find_package(GTest REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
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
  test_display_factory.cpp
  test_terminal_display.cpp
  test_results_exporter.cpp
)

target_link_libraries(cfio-tests
  PRIVATE GTest::gtest
  PRIVATE cfio-core
  PRIVATE nlohmann_json::nlohmann_json
)

gtest_discover_tests(cfio-tests)
