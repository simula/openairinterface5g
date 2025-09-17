add_library(nas_utils OBJECT
    TLVDecoder.c
    OctetString.c
)
target_link_libraries(nas_utils PUBLIC log_headers)

target_include_directories(nas_utils PRIVATE .)
