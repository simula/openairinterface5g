add_library(f1ap_lib OBJECT
            f1ap_lib_common.c
            f1ap_rrc_message_transfer.c
            f1ap_interface_management.c
            f1ap_ue_context.c
)
target_link_libraries(f1ap_lib PRIVATE asn1_f1ap ds)
target_include_directories(f1ap_lib PUBLIC
                           ${CMAKE_SOURCE_DIR}
                           ${CMAKE_CURRENT_SOURCE_DIR}
)

if(ENABLE_TESTS)
  target_compile_definitions(f1ap_lib PRIVATE ENABLE_TESTS)
endif()
