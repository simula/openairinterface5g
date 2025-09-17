add_subdirectory(MESSAGES)
add_subdirectory(lib)

add_library(e1ap e1ap.c e1ap_common.c)
target_link_libraries(e1ap
                      PUBLIC asn1_e1ap e1ap_lib
                      PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs asn1_f1ap UTIL e1_if)
target_include_directories(e1ap PUBLIC ${CMAKE_CURRENT_DIR})

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()

if(ENABLE_TESTS)
  target_compile_definitions(e1ap PRIVATE E1AP_LOG_ERRORS)
endif()
