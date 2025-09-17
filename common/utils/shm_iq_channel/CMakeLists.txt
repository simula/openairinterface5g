add_library(shm_td_iq_channel shm_td_iq_channel.c)
target_include_directories(shm_td_iq_channel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
