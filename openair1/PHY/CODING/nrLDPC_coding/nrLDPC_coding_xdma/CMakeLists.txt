##########################################################

# LDPC offload library - XDMA
##########################################################

add_boolean_option(ENABLE_LDPC_XDMA OFF "Build support for LDPC Offload to XDMA library" OFF)

if (ENABLE_LDPC_XDMA)

  add_library(ldpc_xdma MODULE
              nrLDPC_coding_xdma_offload.c
              nrLDPC_coding_xdma.c
              ../nrLDPC_coding_segment/nr_rate_matching.c
              ../nrLDPC_coding_segment/nrLDPC_coding_segment_encoder.c
              ../../nrLDPC_encoder/ldpc_encoder_optim8segmulti.c
  )
  target_include_directories(ldpc_xdma PRIVATE ../nrLDPC_coding_segment)

  #ensure that the T header files are generated before targets depending on them
  if (${T_TRACER})
    add_dependencies(ldpc_xdma generate_T)
  endif (${T_TRACER})

  target_link_libraries(ldpc_xdma PRIVATE ldpc_gen_HEADERS)
  set_target_properties(ldpc_xdma PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

  add_dependencies(nr-softmodem ldpc_xdma)
  add_dependencies(nr-uesoftmodem ldpc_xdma)
  add_dependencies(nr_ulsim ldpc_xdma)
  add_dependencies(nr_ulschsim ldpc_xdma)
  add_dependencies(nr_dlsim ldpc_xdma)
  add_dependencies(nr_dlschsim ldpc_xdma)

endif()
