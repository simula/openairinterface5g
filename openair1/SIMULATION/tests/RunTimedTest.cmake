# this script expects
# - TEST_CMD (program + options)
# - CHECK_SCRIPT (path to analyze-timing.sh)
# further, it analyzes the (test-specific) environment for CHECK_0, CHECK_1,
# etc. to construct the input to the check script.

# the calling process writes env vars CHECK_0, CHECK_1, ...
# each has "condition;threshold" as it's content
# combine all necessary CHECK0, CHECK1, ... into a single array CHECKS
# to be passed to the check script
foreach(count RANGE 9)
  set(CHECK "CHECK_${count}")
  if (NOT DEFINED ENV{${CHECK}})
    break()
  endif()
  list(APPEND CHECKS "$ENV{${CHECK}}")
endforeach()

# execute the actual test command TEST_CMD and pipe its output into the
# check script. Afterwards, check both commands return codes.
execute_process(COMMAND ${TEST_CMD}
                COMMAND ${CHECK_SCRIPT} ${CHECKS}
                COMMAND_ECHO STDOUT
                RESULTS_VARIABLE RET_CODES
)
list(LENGTH RET_CODES LEN_RET_CODES)
if(NOT LEN_RET_CODES EQUAL 2)
  message(SEND_ERROR "execute_process() did not run both commands!")
endif()

list(GET RET_CODES 0 TEST_RET_CODE)
message(STATUS "test command finished with ${TEST_RET_CODE}")
if(NOT ${TEST_RET_CODE} MATCHES "0")
  message(SEND_ERROR " => test failed!")
endif()

list(GET RET_CODES 1 CHECK_RET_CODE)
message(STATUS "check command finished with ${CHECK_RET_CODE}")
if(NOT ${CHECK_RET_CODE} MATCHES "0")
  message(SEND_ERROR " => check failed!")
endif()
