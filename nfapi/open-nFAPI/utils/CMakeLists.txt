add_executable(nfapi_hex_parser nfapi_hex_parser.c)
target_link_libraries(nfapi_hex_parser PUBLIC nr_fapi_p5 nr_fapi_p7)
# This makes it so nfapi_hex_parser is always compiled whenever NFAPI_LIB is compiled
add_dependencies(NFAPI_LIB nfapi_hex_parser )
