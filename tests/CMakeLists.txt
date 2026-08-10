find_package(GTest REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
include(GoogleTest)

enable_testing()

add_executable(cfio-tests
  test_main.cpp
  test_smoke.cpp
  test_logger.cpp
  test_engine_utils.cpp
  test_size_parser.cpp
  test_job_config.cpp
  test_config_validator.cpp
  test_config_parser.cpp
  test_aligned_buffer.cpp
  test_buffer_pool.cpp
  test_engine_interface.cpp
  test_log2_histogram.cpp
  test_offset_generator.cpp
  test_io_direction_decider.cpp
  test_worker_thread.cpp
  test_metrics_aggregator.cpp
  test_file_preparator.cpp
  test_benchmark_orchestrator.cpp
  test_display_factory.cpp
  test_metric_format.cpp
  test_terminal_display.cpp
  test_results_exporter.cpp
  test_run_mailbox.cpp
  test_qt_display.cpp
  test_qt_chart_geometry.cpp
  test_qt_job_table_model.cpp
)

target_link_libraries(cfio-tests
  PRIVATE GTest::gtest
  PRIVATE cfio-core
  PRIVATE nlohmann_json::nlohmann_json
)

if(CFIO_ENABLE_COVERAGE)
  target_compile_options(cfio-tests PRIVATE ${CFIO_COVERAGE_COMPILE_OPTIONS})
endif()

gtest_discover_tests(cfio-tests)
