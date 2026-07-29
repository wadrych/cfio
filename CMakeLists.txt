cmake_minimum_required(VERSION 3.28)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "vcpkg toolchain")
endif()

project(cfio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

option(CFIO_ENABLE_TUI  "Build with FTXUI TUI support"   OFF)
option(CFIO_ENABLE_QT   "Build with Qt6 GUI support"     OFF)
option(CFIO_ENABLE_SPDK "Build with SPDK engine support"  OFF)
option(CFIO_ENABLE_COVERAGE "Build with gcov instrumentation" OFF)

enable_testing()

if(CFIO_ENABLE_COVERAGE)
  set(CFIO_COVERAGE_COMPILE_OPTIONS -O0 -g --coverage -fprofile-abs-path)
  set(CFIO_COVERAGE_LINK_OPTIONS --coverage)
endif()

find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(DOXYGEN_FOUND)
  add_custom_target(docs
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating Doxygen documentation"
      VERBATIM
  )
endif()

add_subdirectory(src)
add_subdirectory(tests)

if(CFIO_ENABLE_COVERAGE)
  find_program(GCOVR_EXECUTABLE gcovr)
  if(GCOVR_EXECUTABLE)
    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
        COMMAND ${GCOVR_EXECUTABLE}
                --root ${CMAKE_SOURCE_DIR}
                --gcov-executable gcov-14
                --filter ${CMAKE_SOURCE_DIR}/src/
                --exclude-unreachable-branches
                --html-details ${CMAKE_BINARY_DIR}/coverage/index.html
                --print-summary
        DEPENDS cfio-tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating coverage report"
        VERBATIM
    )
  else()
    message(WARNING
        "CFIO_ENABLE_COVERAGE=ON but gcovr was not found.")
  endif()
endif()
