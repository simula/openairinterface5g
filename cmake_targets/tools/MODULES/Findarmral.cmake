#
# Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The OpenAirInterface Software Alliance licenses this file to You under
# the OAI Public License, Version 1.1  (the "License"); you may not use this file
# except in compliance with the License.
# You may obtain a copy of the License at
# 
#      http://www.openairinterface.org/?page_id=698
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ------------------------------------------------------------------------------
# For more information about the OpenAirInterface (OAI) Software Alliance:
#      contact@openairinterface.org
# 

# FindArmral
# -------
# 
# Finds the armral library.
# the version is currently hardcoded to 25.01
#
# Optional options
# ^^^^^^^^^^^^^^^^
#
# ``armral_LOCATION``
#   The location of the library.
# 
# Imported Targets
# ^^^^^^^^^^^^^^^^
# 
# This module provides the following imported targets, if found:
# 
# ``armral``
#   The armral library
# 
# Result Variables
# ^^^^^^^^^^^^^^^^
# 
# This will define the following variables:
# 
# ``armral_FOUND``
#   True if the system has the armral library.
# ``armral_VERSION``
#   The version of the armral library which was found.
# ``armral_INCLUDE_DIRS``
#   Include directories needed to use armral.
# ``armral_LIBRARIES``
#   Libraries needed to link to armral.
# 
# Cache Variables
# ^^^^^^^^^^^^^^^
# 
# The following cache variables may also be set:
# 
# ``armral_INCLUDE_DIR``
#   The directory containing ``armral.h``.
# ``armral_LIBRARY``
#   The path to the armral library.

option(armral_LOCATION "directory of ArmRAL library" "")
if (NOT armral_LOCATION)
  set(armral_LOCATION "/usr/local")
endif()
if (NOT EXISTS ${armral_LOCATION})
  message(FATAL_ERROR "no such directory: ${armral_LOCATION}")
endif()

find_path(armral_INCLUDE_DIR
  NAMES
    armral.h
  HINTS ${armral_LOCATION}
  PATH_SUFFIXES include
  NO_DEFAULT_PATH
)
if (NOT armral_INCLUDE_DIR)
  message(FATAL_ERROR "could not detect armral header at ${armral_LOCATION}/include")
endif()

find_library(armral_LIBRARY
  NAMES armral
  HINTS ${armral_LOCATION}
  PATH_SUFFIXES lib build
  NO_DEFAULT_PATH
)
if (NOT armral_LIBRARY)
  message(FATAL_ERROR "could not detect armral build artifacts at neither ${armral_LOCATION}/lib nor ${armral_LOCATION}/build")
endif()

# No way to retrieve the version from header or library
set(armral_VERSION 25.01)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(armral
  FOUND_VAR armral_FOUND
  REQUIRED_VARS
    armral_LIBRARY
    armral_INCLUDE_DIR
  VERSION_VAR armral_VERSION
)

if(armral_FOUND)
  set(armral_LIBRARIES ${armral_LIBRARY})
  set(armral_INCLUDE_DIRS ${armral_INCLUDE_DIR})
endif()

if(armral_FOUND AND NOT TARGET armral)
  add_library(armral UNKNOWN IMPORTED)
  set_target_properties(armral PROPERTIES
    IMPORTED_LOCATION "${armral_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${armral_INCLUDE_DIRS}"
  )
endif()

mark_as_advanced(
  armral_INCLUDE_DIR
  armral_LIBRARY
)
