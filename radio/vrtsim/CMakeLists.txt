add_library(noise_device STATIC noise_device.c)

add_library(vrtsim MODULE vrtsim.c)
set_target_properties(vrtsim PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(vrtsim PRIVATE SIMU shm_td_iq_channel actor noise_device)
add_dependencies(vrtsim generate_T)

add_boolean_option(OAI_VRTSIM_TAPS_CLIENT OFF "Enable taps client" ON)
if (OAI_VRTSIM_TAPS_CLIENT)
  target_link_libraries(vrtsim PRIVATE taps_client)
  file(DOWNLOAD
    "https://gitlab.eurecom.fr/oai/raytracing-channel-emulator/-/raw/59589ad2a534881810b2aa7dd81e63066245bb8d/server/api/taps.fbs?inline=false"
    ${CMAKE_SOURCE_DIR}/taps.fbs
  )

  set(TAPS_IF_PATH ${CMAKE_SOURCE_DIR}/taps.fbs)
  set(TAPS_API_HEADER taps_generated.h)

  add_custom_command(OUTPUT ${TAPS_API_HEADER}
    COMMAND flatc -c ${TAPS_IF_PATH}
    DEPENDS ${TAPS_IF_PATH}
    COMMENT "Generating flatbuffers API from ${TAPS_IF_PATH}"
  )
  add_library(taps_api INTERFACE)
  target_include_directories(taps_api INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

  add_library(taps_client taps_client.cpp ${TAPS_API_HEADER})
  target_include_directories(taps_client PUBLIC .)
  find_package(Flatbuffers REQUIRED)
  target_link_libraries(taps_client PUBLIC flatbuffers taps_api SIMU nanomsg)
endif()
