add_subdirectory(utils)
add_subdirectory(config/yaml)

configure_file(oai_version.h.in oai_version.h @ONLY)

add_library(instrumentation INTERFACE)
target_include_directories(instrumentation INTERFACE .)
if (TRACY_ENABLE)
  target_link_libraries(instrumentation INTERFACE Tracy::TracyClient)
endif()
