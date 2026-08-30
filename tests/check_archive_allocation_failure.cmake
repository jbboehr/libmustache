cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED MUSTACHE_TEST_ALLOCATION_FAILURE_EXECUTABLE OR
        MUSTACHE_TEST_ALLOCATION_FAILURE_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR
        "MUSTACHE_TEST_ALLOCATION_FAILURE_EXECUTABLE is required")
endif()

foreach(MUSTACHE_OPERATION IN ITEMS serialize load load-view)
    set(MUSTACHE_OBSERVED_ALLOCATION_FAILURE FALSE)
    set(MUSTACHE_COMPLETED_ALLOCATION_SWEEP FALSE)
    foreach(MUSTACHE_FAIL_AT RANGE 0 4095)
        execute_process(
            COMMAND "${MUSTACHE_TEST_ALLOCATION_FAILURE_EXECUTABLE}"
                --archive-allocation-probe "${MUSTACHE_OPERATION}"
                "${MUSTACHE_FAIL_AT}"
            RESULT_VARIABLE MUSTACHE_PROBE_RESULT
            OUTPUT_VARIABLE MUSTACHE_PROBE_OUTPUT
            ERROR_VARIABLE MUSTACHE_PROBE_ERROR)
        if(MUSTACHE_PROBE_RESULT EQUAL 10)
            set(MUSTACHE_OBSERVED_ALLOCATION_FAILURE TRUE)
        elseif(MUSTACHE_PROBE_RESULT EQUAL 0)
            if(NOT MUSTACHE_OBSERVED_ALLOCATION_FAILURE)
                message(FATAL_ERROR
                    "Archive ${MUSTACHE_OPERATION} succeeded without exercising an allocation failure")
            endif()
            set(MUSTACHE_COMPLETED_ALLOCATION_SWEEP TRUE)
            break()
        else()
            message(FATAL_ERROR
                "Archive ${MUSTACHE_OPERATION} did not propagate allocation failure ${MUSTACHE_FAIL_AT} "
                "(result ${MUSTACHE_PROBE_RESULT}):\n${MUSTACHE_PROBE_OUTPUT}${MUSTACHE_PROBE_ERROR}")
        endif()
    endforeach()
    if(NOT MUSTACHE_COMPLETED_ALLOCATION_SWEEP)
        message(FATAL_ERROR
            "Archive ${MUSTACHE_OPERATION} did not complete within 4096 allocation attempts")
    endif()
endforeach()
