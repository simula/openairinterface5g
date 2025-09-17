# we need at least gcc-11 to build xran, so let's enforce it here
# (because xran might not check it, and we have control here)
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0
   OR CMAKE_C_COMPILER_VERSION VERSION_LESS 11.0)
 message(FATAL_ERROR "you need to have at least gcc/g++-11 to use 7.2 FHI through xran")
endif()

# use env var PKG_CONFIG_PATH to override paths to libdpdk.pc
pkg_check_modules(dpdk REQUIRED libdpdk)
pkg_check_modules(numa REQUIRED numa)

add_library(oran_fhlib_5g MODULE
            oran_isolate.c
            oaioran.c
            oran-config.c
            oran-init.c
)

set(E_VERSION 5.1.6)
set(F_VERSION 6.1.4)

find_package(xran REQUIRED)
if(xran_VERSION VERSION_EQUAL E_VERSION)
  target_compile_definitions(oran_fhlib_5g PRIVATE E_RELEASE)
elseif(xran_VERSION VERSION_EQUAL F_VERSION)
  target_compile_definitions(oran_fhlib_5g PRIVATE F_RELEASE)
else()
  message(FATAL_ERROR "Found xran version ${xran_VERSION} but needed ${E_VERSION} (E release) or ${F_VERSION} (F release)")
endif()

# Ignore xran-specific warning: we don't care/can't change the following warning, so suppress
# alignment 1 of 'struct XYZ' is less than 2
add_compile_options(-Wno-packed-not-aligned)

set_target_properties(oran_fhlib_5g PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -march=native")
target_link_libraries(oran_fhlib_5g PRIVATE xran::xran ${dpdk_LINK_LIBRARIES} ${T_LIB} pthread dl rt m numa)
target_include_directories(oran_fhlib_5g PRIVATE ${dpdk_INCLUDE_DIRS})
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  find_package(armral REQUIRED)
  target_sources(oran_fhlib_5g PRIVATE armral_bfp_compression.c)
  target_link_libraries(oran_fhlib_5g PRIVATE armral)
endif()

add_boolean_option(OAI_FHI72_USE_POLLING OFF "Enable polling in FHI72 driver" ON)

message(STATUS "Building FHI72 CUS library")

set_target_properties(oran_fhlib_5g PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(TARGET oran_fhlib_5g POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink liboran_fhlib_5g.so liboai_transpro.so
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_boolean_option(OAI_FHI72_MPLANE OFF "Activate OAI's FHI 7.2 M-plane support" OFF)
if(OAI_FHI72_MPLANE)
  add_subdirectory(mplane)
endif()
