add_library(e1ap_lib OBJECT
            e1ap_bearer_context_management.c
            e1ap_interface_management.c
            e1ap_lib_common.c
)

target_link_libraries(e1ap_lib PRIVATE asn1_e1ap)
target_include_directories(e1ap_lib PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}
)

