if (ENABLE_IMSCOPE)
  CPMAddPackage("gh:ocornut/imgui#v1.91.3-docking")
  add_library(imgui
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
  )
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

  add_library(imgui_opengl_renderer ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(imgui_opengl_renderer PUBLIC ${imgui_SOURCE_DIR}/backends/)
  target_link_libraries(imgui_opengl_renderer PUBLIC imgui)

  add_library(imgui_glfw_backend ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
  target_include_directories(imgui_glfw_backend PUBLIC ${imgui_SOURCE_DIR}/backends/)
  target_link_libraries(imgui_glfw_backend PUBLIC imgui)

  find_package(OpenGL REQUIRED)
  find_package(glfw3 3.3 REQUIRED)

  CPMAddPackage("gh:epezent/implot#v0.16")
  add_library(implot
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
  )
  target_link_libraries(implot PUBLIC imgui)
  target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})

  add_library(imscope MODULE imscope.cpp ../phy_scope_interface.c imscope_common.cpp imscope_init.cpp)
  target_link_libraries(imscope PUBLIC imgui_glfw_backend glfw imgui_opengl_renderer OpenGL::OpenGL implot UTIL)
  set_target_properties(imscope PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  configure_file(imgui.ini ${CMAKE_BINARY_DIR}/imscope-init.ini COPYONLY)

  add_executable(imscope_iq_file_viewer imscope.cpp imscope_iq_file_viewer.cpp ../phy_scope_interface.c imscope_common.cpp)
  target_link_libraries(imscope_iq_file_viewer PRIVATE imgui_glfw_backend glfw imgui_opengl_renderer OpenGL::OpenGL implot UTIL minimal_lib shlib_loader)
  set_target_properties(imscope_iq_file_viewer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if (ENABLE_IMSCOPE_RECORD)
  add_library(imscope_record MODULE ../phy_scope_interface.c imscope_common.cpp imscope_record.cpp)
  target_link_libraries(imscope_record PRIVATE UTIL atomic)
  set_target_properties(imscope_record PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

