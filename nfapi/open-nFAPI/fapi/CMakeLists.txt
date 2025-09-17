add_library(nr_fapi_p5
        src/nr_fapi_p5.c
        src/nr_fapi_p5_utils.c
)
target_include_directories(nr_fapi_p5 PUBLIC inc)
target_link_libraries(nr_fapi_p5 PUBLIC nfapi_common)
if(OAI_AERIAL)
    target_compile_definitions(nr_fapi_p5 PRIVATE ENABLE_AERIAL)
endif()
