add_executable(test_nr_modulation test_nr_modulation.cpp)
target_link_libraries(test_nr_modulation PRIVATE PHY_NR GTest::gtest minimal_lib)
add_dependencies(tests test_nr_modulation)
add_test(NAME test_nr_modulation
  COMMAND ./test_nr_modulation)
