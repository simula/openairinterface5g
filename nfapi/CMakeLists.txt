add_subdirectory(oai_integration/aerial)
if (OAI_WLS)
    add_subdirectory(oai_integration/wls_integration)
else () # Not WLS, means its SOCKET-based comms, maybe add a flag specific for UDP
    message(STATUS "No Support for WLS")
    add_subdirectory(oai_integration/socket)
endif ()
add_subdirectory(open-nFAPI)
add_subdirectory(tests)
