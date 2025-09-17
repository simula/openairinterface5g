find_package(yaml-cpp REQUIRED PATHS /usr/lib/aarch64-linux-gnu/pkgconfig/)
# include(FetchContent)

# FetchContent_Declare(
#   yaml-cpp
#   GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
#   GIT_TAG 0.8.0
# )
# FetchContent_GetProperties(yaml-cpp)

# if(NOT yaml-cpp_POPULATED)
#   message(STATUS "Fetching yaml-cpp...")
#   FetchContent_Populate(yaml-cpp)
#   add_subdirectory(${yaml-cpp_SOURCE_DIR} ${yaml-cpp_BINARY_DIR})
# endif()

add_library(params_yaml_static config_yaml.cpp)
target_link_libraries(params_yaml_static PUBLIC UTIL yaml-cpp)
if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()

add_library(params_yaml MODULE config_yaml.cpp)
target_link_libraries(params_yaml PUBLIC UTIL yaml-cpp)
set_target_properties(params_yaml PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
