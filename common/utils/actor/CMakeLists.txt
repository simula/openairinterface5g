add_library(actor actor.c)
# Only include header files due to issues with ODR in threadPool binaries
target_include_directories(actor PUBLIC ./ ../threadPool/)
if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
