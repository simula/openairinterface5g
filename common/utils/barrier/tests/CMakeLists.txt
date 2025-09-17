add_executable(test_barrier test_barrier.cpp)
target_link_libraries(test_barrier PRIVATE barrier GTest::gtest)
add_dependencies(tests test_barrier)
add_test(NAME test_barrier
  COMMAND ./test_barrier)
