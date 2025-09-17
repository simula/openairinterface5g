add_library(nfapi_socket_lib
        socket_pnf.c
        socket_vnf.c
        socket_common.c
)

target_include_directories(nfapi_socket_lib PRIVATE include/)

target_compile_definitions(nfapi_socket_lib PUBLIC ENABLE_SOCKET)
target_link_libraries(nfapi_socket_lib PRIVATE NFAPI_LIB NFAPI_USER_LIB)
target_link_libraries(nfapi_socket_lib PRIVATE LOG)
add_dependencies(nfapi_socket_lib generate_T)
target_link_libraries(nfapi_socket_lib PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs)
target_link_libraries(nfapi_socket_lib PRIVATE nr_fapi_p5 nr_fapi_p7)
