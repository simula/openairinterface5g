add_library(barrier barrier.c)
target_include_directories(barrier PUBLIC ./)
target_link_libraries(barrier PUBLIC pthread)

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
