add_library(lib_uicc OBJECT usim_interface.c)
target_include_directories(lib_uicc PRIVATE .)
target_link_libraries(lib_uicc PRIVATE log_headers)
