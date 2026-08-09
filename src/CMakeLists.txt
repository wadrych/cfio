# dependencies
find_package(spdlog REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CLI11 REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-vincentlaucsb-csv-parser CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)
find_library(LIBAIO_LIBRARY NAMES aio REQUIRED)
find_path(LIBAIO_INCLUDE_DIR NAMES libaio.h REQUIRED)

# libcfio-core static library
add_library(cfio-core STATIC
        cfio_core.cpp
        logging/logger.cpp
        config/size_parser.cpp
        config/job_config.cpp
        config/config_validator.cpp
        config/json_parser.cpp
        config/csv_parser.cpp
        config/parser_factory.cpp
        engine/engine_factory.cpp
        engine/psync_engine.cpp
        engine/io_uring_engine.cpp
        engine/libaio_engine.cpp
        engine/sync_engine.cpp
        telemetry/aligned_buffer.cpp
        telemetry/buffer_pool.cpp
        telemetry/offset_generator.cpp
        telemetry/io_direction_decider.cpp
        telemetry/worker_thread.cpp
        telemetry/metrics_aggregator.cpp
        orchestrator/file_preparator.cpp
        orchestrator/benchmark_orchestrator.cpp
        display/display_factory.cpp
        display/terminal_display.cpp
        reporting/results_exporter.cpp
)

target_include_directories(cfio-core
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${LIBURING_INCLUDE_DIRS}
        PRIVATE ${LIBAIO_INCLUDE_DIR}
)

target_link_libraries(cfio-core
        PUBLIC spdlog::spdlog
        PRIVATE fmt::fmt
        PRIVATE nlohmann_json::nlohmann_json
        PRIVATE unofficial::vincentlaucsb-csv-parser::csv
        PRIVATE ${LIBURING_LIBRARIES}
        PRIVATE ${LIBAIO_LIBRARY}
)

if(CFIO_ENABLE_COVERAGE)
  target_compile_options(cfio-core PRIVATE ${CFIO_COVERAGE_COMPILE_OPTIONS})
  target_link_options(cfio-core PUBLIC ${CFIO_COVERAGE_LINK_OPTIONS})
endif()

# cfio executable
add_executable(cfio
        main.cpp
)

target_link_libraries(cfio
        PRIVATE cfio-core
        PRIVATE CLI11::CLI11
)
