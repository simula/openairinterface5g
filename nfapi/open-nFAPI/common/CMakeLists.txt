add_library(nfapi_common
            src/debug.c
            src/nfapi.c)
target_include_directories(nfapi_common PUBLIC public_inc/)
if (OAI_AERIAL)
    target_compile_definitions(nfapi_common PRIVATE FAPI_BYTE_ORDERING_BIG_ENDIAN)
endif()
