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

add_executable(dft_test test_dft.c ../dfts_load.c)
target_link_libraries(dft_test minimal_lib shlib_loader SIMU m)
add_dependencies(tests dft_test)
add_dependencies(dft_test dfts) # trigger build of dfts (shared lib for DFT)
add_test(NAME dft_test COMMAND ./dft_test)

add_executable(test_vector_op test_vector_op.cpp)
target_link_libraries(test_vector_op PRIVATE LOG minimal_lib)
add_dependencies(tests test_vector_op)
add_test(NAME test_vector_op
  COMMAND test_vector_op)

add_executable(test_oai_arith_operations test_oai_arith_operations.cpp ../oai_arith_operations.c)
target_link_libraries(test_oai_arith_operations PRIVATE GTest::gtest LOG minimal_lib)
add_dependencies(tests test_oai_arith_operations)
add_test(NAME test_oai_arith_operations
  COMMAND ./test_oai_arith_operations)

add_executable(test_sse_intrinsics test_sse_intrinsics.cpp)
target_link_libraries(test_sse_intrinsics PRIVATE GTest::gtest LOG minimal_lib)
add_dependencies(tests test_sse_intrinsics)
add_test(NAME test_sse_intrinsics
  COMMAND ./test_sse_intrinsics)