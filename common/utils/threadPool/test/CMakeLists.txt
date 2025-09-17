add_executable(test_thread-pool
  test_thread-pool.c)
add_dependencies(tests test_thread-pool)
target_link_libraries(test_thread-pool PRIVATE thread-pool pthread LOG minimal_lib)
add_test(NAME test_thread-pool
  COMMAND ./test_thread-pool)
