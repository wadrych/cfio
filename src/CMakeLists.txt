# libcfio-core static library
add_library(cfio-core STATIC
  cfio_core.cpp
)

target_include_directories(cfio-core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# cfio executable
add_executable(cfio
  main.cpp
)

target_link_libraries(cfio
  PRIVATE cfio-core
)
