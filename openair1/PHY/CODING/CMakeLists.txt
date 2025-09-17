add_library(coding MODULE
            3gpplte_sse.c
            3gpplte.c
            3gpplte_turbo_decoder_sse_8bit.c
            3gpplte_turbo_decoder_sse_16bit.c
            3gpplte_turbo_decoder_avx2_16bit.c
            3gpplte_turbo_decoder.c
)
set_target_properties(coding PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
#ensure that the T header files are generated before targets depending on them
target_link_libraries(coding PRIVATE log_headers)

add_library(ldpc_orig MODULE
            nrLDPC_decoder/nrLDPC_decoder.c
            nrLDPC_encoder/ldpc_encoder.c
)
set_target_properties(ldpc_orig PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(ldpc_orig PRIVATE ldpc_segment ldpc_gen_HEADERS)

add_library(ldpc MODULE
            nrLDPC_decoder/nrLDPC_decoder.c
            nrLDPC_encoder/ldpc_encoder_optim8segmulti.c
)
set_target_properties(ldpc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(ldpc PRIVATE ldpc_segment ldpc_gen_HEADERS)

add_dependencies(ldpctest ldpc ldpc_orig)
add_dependencies(nr-softmodem ldpc ldpc_orig)
add_dependencies(nr-uesoftmodem ldpc ldpc_orig)
add_dependencies(nr_ulsim ldpc ldpc_orig)
add_dependencies(nr_ulschsim ldpc ldpc_orig)
add_dependencies(nr_dlsim ldpc ldpc_orig)
add_dependencies(nr_dlschsim ldpc ldpc_orig)

##############################################
# Base CUDA setting
##############################################

add_boolean_option(ENABLE_LDPC_CUDA OFF "Build support for CUDA" OFF)
if (ENABLE_LDPC_CUDA)
  find_package(CUDA REQUIRED)
  if (NOT CUDA_FOUND)
    message(FATAL_ERROR "no CUDA found")
  endif()
  SET(CUDA_NVCC_FLAG "${CUDA_NVCC_FLAGS};-arch=sm_60;")
  SET(CUDA_VERBOSE_BUILD ON)
  cuda_add_library(ldpc_cuda MODULE
                   nrLDPC_decoder_LYC/nrLDPC_decoder_LYC.cu
                   nrLDPC_encoder/ldpc_encoder_optim8segmulti.c
                   # The slot coding layer cannot be linked with
                   # target_link_libraries like above
                   # because of cuda_add_library
                   # which already uses target_link_libraries
                   nrLDPC_coding/nrLDPC_coding_segment/nrLDPC_coding_segment_decoder.c
                   nrLDPC_coding/nrLDPC_coding_segment/nrLDPC_coding_segment_encoder.c
                   nrLDPC_coding/nrLDPC_coding_segment/nr_rate_matching.c
  )
  set_target_properties(ldpc_cuda PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
  set_target_properties(ldpc_cuda PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

  add_dependencies(ldpctest ldpc_cuda)
  add_dependencies(nr-softmodem ldpc_cuda)
  add_dependencies(nr-uesoftmodem ldpc_cuda)
  add_dependencies(nr_ulsim ldpc_cuda)
  add_dependencies(nr_ulschsim ldpc_cuda)
  add_dependencies(nr_dlsim ldpc_cuda)
  add_dependencies(nr_dlschsim ldpc_cuda)
endif()

add_subdirectory(nrLDPC_coding)
