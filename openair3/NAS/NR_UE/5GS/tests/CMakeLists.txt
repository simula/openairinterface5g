message(STATUS "Add test nas_lib_test")

add_executable(nas_lib_test nas_lib_test.c)

add_dependencies(tests nas_lib_test)
add_test(NAME nas_lib_test COMMAND nas_lib_test)

target_link_libraries(nas_lib_test PRIVATE
    nr_nas
)

target_include_directories(nas_lib_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
