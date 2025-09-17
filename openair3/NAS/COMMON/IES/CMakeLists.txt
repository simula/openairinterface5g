add_library(nas_common_ies_lib OBJECT
    AuthenticationResponseParameter.c
    NasKeySetIdentifier.c
)

target_include_directories(nas_common_ies_lib PUBLIC .)
target_link_libraries(nas_common_ies_lib PUBLIC log_headers)
