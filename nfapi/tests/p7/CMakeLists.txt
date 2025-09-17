set(Test_Labels fapi p7)
set(_fapi_p7_messages "dci_inversion")
foreach (fapi_p7_message IN LISTS _fapi_p7_messages)
    add_executable(nr_fapi_${fapi_p7_message}_test nr_fapi_${fapi_p7_message}_test.c)
    target_link_libraries(nr_fapi_${fapi_p7_message}_test PUBLIC nr_fapi_p5)
    target_link_libraries(nr_fapi_${fapi_p7_message}_test PRIVATE pthread UTIL ${T_LIB} minimal_lib)
    add_dependencies(tests nr_fapi_${fapi_p7_message}_test)

    add_test(nr_fapi_${fapi_p7_message}_test nr_fapi_${fapi_p7_message}_test)
    set_tests_properties(nr_fapi_${fapi_p7_message}_test PROPERTIES LABELS "${Test_Labels}")
endforeach ()
# Add the dci label for the dci payload test
set(dci_labels dci ${Test_Labels})
set_tests_properties(nr_fapi_dci_inversion_test PROPERTIES LABELS "dci ${Test_Labels}")
# Add the FAPI_BYTE_ORDERING_BIG_ENDIAN preprocessor define in order to test the invert/uninvert dci payload functions
target_compile_definitions(nfapi_common PRIVATE FAPI_BYTE_ORDERING_BIG_ENDIAN)
