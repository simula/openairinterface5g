add_subdirectory(5GMM/MSG)
add_subdirectory(5GMM/IES)
add_subdirectory(5GSM/MSG)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
add_library(fgs_lib fgs_nas_lib.c)

target_link_libraries(fgs_lib PUBLIC
    fgs_5gmm_lib
    fgs_5gsm_lib
    fgs_5gmm_ies_lib
    nas_common_ies_lib
    ds
)

target_include_directories(fgs_lib PUBLIC . 5GMM/MSG 5GMM/IES)
