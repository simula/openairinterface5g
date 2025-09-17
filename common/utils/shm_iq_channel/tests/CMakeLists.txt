add_executable(test_shm_td_iq_channel test_shm_td_iq_channel.c)
target_link_libraries(test_shm_td_iq_channel shm_td_iq_channel minimal_lib pthread)
add_dependencies(tests test_shm_td_iq_channel)
# Commented out due to issues with ASAN in the unit test docker container
#add_test(test_shm_td_iq_channel ./test_shm_td_iq_channel)
