# dependencies
find_package(spdlog REQUIRED)
find_package(CLI11 REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-vincentlaucsb-csv-parser CONFIG REQUIRED)

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
        telemetry/aligned_buffer.cpp
)

target_include_directories(cfio-core
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cfio-core
        PUBLIC spdlog::spdlog
        PRIVATE nlohmann_json::nlohmann_json
        PRIVATE unofficial::vincentlaucsb-csv-parser::csv
)

# cfio executable
add_executable(cfio
        main.cpp
)

target_link_libraries(cfio
        PRIVATE cfio-core
        PRIVATE CLI11::CLI11
)
