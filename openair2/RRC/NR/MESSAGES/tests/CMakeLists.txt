add_executable(test_asn1_msg test_asn1_msg.cpp)
target_link_libraries(test_asn1_msg PRIVATE nr_rrc SECURITY minimal_lib GTest::gtest UTIL)
add_dependencies(tests test_asn1_msg)
add_test(NAME test_asn1_msg
    COMMAND ./test_asn1_msg)
