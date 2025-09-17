add_executable(test_actor test_actor.cpp)
target_link_libraries(test_actor PRIVATE actor GTest::gtest thread-pool LOG)
add_dependencies(tests test_actor)
add_test(NAME test_actor
  COMMAND ./test_actor)
