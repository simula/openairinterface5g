# - Try to find SCTP library and headers
#
# Imported Targets
# ^^^^^^^^^^^^^^^^
#
# This module provides the following imported targets, if found:
#
# ``sctp::sctp``
#   The SCTP library
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# this will define
#
#  sctp_FOUND - system has SCTP
#  sctp_INCLUDE_DIRS - the SCTP include directories
#  sctp_LIBRARIES - link these to use SCTP

# Include dir
find_path(sctp_INCLUDE_DIR
  NAMES netinet/sctp.h
)

# Library
find_library(sctp_LIBRARY
  NAMES sctp
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
#set(sctp_PROCESS_INCLUDES sctp_INCLUDE_DIR)
#set(sctp_PROCESS_LIBS sctp_LIBRARY)
#libfind_process(sctp)


# handle the QUIETLY and REQUIRED arguments and set sctp_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(sctp
  FOUND_VAR sctp_FOUND
  REQUIRED_VARS
    sctp_LIBRARY
    sctp_INCLUDE_DIR
)

# If we successfully found the sctp library then add the library to the
# sctp_LIBRARIES cmake variable otherwise set sctp_LIBRARIES to nothing.
if(sctp_FOUND)
   set(sctp_LIBRARIES ${sctp_LIBRARY})
   set(sctp_INCLUDE_DIRS ${sctp_INCLUDE_DIR})
endif()

if(sctp_FOUND AND NOT TARGET sctp::sctp)
  add_library(sctp::sctp UNKNOWN IMPORTED)
  set_target_properties(sctp::sctp PROPERTIES
    IMPORTED_LOCATION "${sctp_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${sctp_INCLUDE_DIRS}"
  )
endif()

# Lastly make it so that the SCTP_LIBRARY and SCTP_INCLUDE_DIR variables
# only show up under the advanced options in the gui cmake applications.
mark_as_advanced(SCTP_LIBRARY SCTP_INCLUDE_DIR)
