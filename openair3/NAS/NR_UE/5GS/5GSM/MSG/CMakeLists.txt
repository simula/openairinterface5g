add_library(fgs_5gsm_lib OBJECT PduSessionEstablishRequest.c
                            PduSessionEstablishmentAccept.c)

target_link_libraries(fgs_5gsm_lib PUBLIC nas_common_ies_lib log_headers)

target_include_directories(fgs_5gsm_lib PUBLIC . ../../)
