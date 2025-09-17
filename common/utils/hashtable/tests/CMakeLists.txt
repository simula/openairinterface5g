add_executable(test_hashtable test_hashtable.cpp)
add_dependencies(tests test_hashtable)
target_link_libraries(test_hashtable PRIVATE HASHTABLE GTest::gtest)
add_test(NAME test_hashtable
  COMMAND ./test_hashtable)
