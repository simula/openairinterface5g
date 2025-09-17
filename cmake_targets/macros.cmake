###########################################
# macros to define options as there is numerous options in oai
################################################
macro(add_option name val helpstr)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  add_definitions("-D${name}=${value}")
endmacro(add_option)

macro(add_boolean_option name val helpstr adddef)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  set_property(CACHE ${name} PROPERTY TYPE BOOL)
  if (${value} AND ${adddef})
    add_definitions("-D${name}")
  endif (${value} AND ${adddef})
endmacro(add_boolean_option)

macro(add_integer_option name val helpstr)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  add_definitions("-D${name}=${value}")
endmacro(add_integer_option)

macro(add_list1_option name val helpstr)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  set_property(CACHE ${name} PROPERTY STRINGS ${ARGN})
  if(NOT "${value}" STREQUAL "False")
    add_definitions("-D${name}=${value}")
  endif()
endmacro(add_list1_option)

macro(add_list2_option name val helpstr)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  set_property(CACHE ${name} PROPERTY STRINGS ${ARGN})
  if(NOT "${value}" STREQUAL "None")
    add_definitions("-D${value}=1")
  endif()
endmacro(add_list2_option)

macro(add_list_string_option name val helpstr)
  if(DEFINED ${name})
    set(value ${${name}})
  else(DEFINED ${name})
    set(value ${val})
  endif()
  set(${name} ${value} CACHE STRING "${helpstr}")
  set_property(CACHE ${name} PROPERTY STRINGS ${ARGN})
  if(NOT "${value}" STREQUAL "False")
    add_definitions("-D${name}=\"${value}\"")
  endif()
endmacro(add_list_string_option)

# this function should produce the same value as the macro MAKE_VERSION defined in the C code (file types.h)
function(make_version VERSION_VALUE)
  math(EXPR RESULT "0")
  foreach (ARG ${ARGN})
    math(EXPR RESULT "${RESULT} * 256 + ${ARG}")
  endforeach()
  set(${VERSION_VALUE} "${RESULT}" PARENT_SCOPE)
endfunction()

macro(eval_boolean VARIABLE)
  if(${ARGN})
    set(${VARIABLE} ON)
  else()
    set(${VARIABLE} OFF)
  endif()
endmacro()

function(check_option EXEC TEST_OPTION EXEC_HINT)
  message(STATUS "Check if ${EXEC} supports ${TEST_OPTION}")
  execute_process(COMMAND ${EXEC} ${TEST_OPTION}
                  RESULT_VARIABLE CHECK_STATUS
                  OUTPUT_VARIABLE CHECK_OUTPUT
                  ERROR_VARIABLE CHECK_OUTPUT)
  if(NOT ${CHECK_STATUS} EQUAL 1)
    get_filename_component(EXEC_FILE ${EXEC} NAME)
    message(FATAL_ERROR "Error message: ${CHECK_OUTPUT}\
You might want to re-run ./build_oai -I
Or provide a path to ${EXEC_FILE} using
  ./build_oai ... --cmake-opt -D${EXEC_HINT}=/path/to/${EXEC_FILE}
or directly with
  cmake .. -D${EXEC_HINT}=/path/to/${EXEC_FILE}
")
  endif()
endfunction()

function(run_asn1c ASN1C_GRAMMAR ASN1C_PREFIX)
  set(options "")
  set(oneValueArgs COMMENT)
  set(multiValueArgs OUTPUT OPTIONS)
  cmake_parse_arguments(ASN1C "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  get_filename_component(GRAMMAR_FILE ${ASN1C_GRAMMAR} NAME)
  set(LOGFILE "${CMAKE_CURRENT_BINARY_DIR}/${GRAMMAR_FILE}.log")
  add_custom_command(OUTPUT ${ASN1C_OUTPUT}
    COMMAND ASN1C_PREFIX=${ASN1C_PREFIX} ${ASN1C_EXEC} ${ASN1C_OPTIONS} -D ${CMAKE_CURRENT_BINARY_DIR} ${ASN1C_GRAMMAR} > ${LOGFILE} 2>&1 || cat ${LOGFILE}
    DEPENDS ${ASN1C_GRAMMAR}
    COMMENT "Generating ${ASN1C_COMMENT} from ${GRAMMAR_FILE}"
  )
endfunction()
