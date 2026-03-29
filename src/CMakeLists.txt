# dependencies
find_package(spdlog REQUIRED)
find_package(CLI11 REQUIRED)

# libcfio-core static library
add_library(cfio-core STATIC
        cfio_core.cpp
        logging/logger.cpp
        config/size_parser.cpp
        config/job_config.cpp
        config/config_validator.cpp
)

target_include_directories(cfio-core
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cfio-core
        PUBLIC spdlog::spdlog
)

# cfio executable
add_executable(cfio
        main.cpp
)

target_link_libraries(cfio
        PRIVATE cfio-core
        PRIVATE CLI11::CLI11
)
