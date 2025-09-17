add_executable(test_signal_energy test_signal_energy.cpp ../signal_energy.c)
target_link_libraries(test_signal_energy PRIVATE GTest::gtest UTIL)
add_dependencies(tests test_signal_energy)
add_test(NAME test_signal_energy
  COMMAND ./test_signal_energy)

add_executable(benchmark_rotate_vector benchmark_rotate_vector.cpp)
target_link_libraries(benchmark_rotate_vector PRIVATE benchmark::benchmark UTIL PHY_NR)
add_dependencies(tests benchmark_rotate_vector)
add_test(NAME benchmark_rotate_vector
  COMMAND ./benchmark_rotate_vector)

add_executable(test_log2_approx test_log2_approx.cpp ../log2_approx.c)
target_link_libraries(test_log2_approx PRIVATE GTest::gtest LOG minimal_lib)
add_dependencies(tests test_log2_approx)
add_test(NAME test_log2_approx
  COMMAND ./test_log2_approx --gtest_filter=-log2_approx.complete)
