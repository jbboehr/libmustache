if(DEFINED MUSTACHE_TEST_SPEC_EXECUTABLE)
    foreach(invalid_value "+1" " 1" "1x" "0" "-1" "999999999999999999999999")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "EXEC_NUM=${invalid_value}"
                "${MUSTACHE_TEST_SPEC_EXECUTABLE}" "${MUSTACHE_SPEC_DIR}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error)
        if(NOT result EQUAL 1)
            message(FATAL_ERROR
                "Specification test accepted EXEC_NUM=${invalid_value} (result ${result}):\n${output}${error}")
        endif()
        string(FIND "${output}${error}"
            "Invalid EXEC_NUM: expected a positive decimal integer"
            diagnostic_position)
        if(diagnostic_position EQUAL -1)
            message(FATAL_ERROR
                "Specification test did not diagnose EXEC_NUM=${invalid_value}:\n${output}${error}")
        endif()
    endforeach()
endif()

if(DEFINED MUSTACHE_TEST_ALLOCATION_FAILURE_EXECUTABLE)
    foreach(invalid_value "+1" " 1" "1x" "-1" "999999999999999999999999")
        execute_process(
            COMMAND "${MUSTACHE_TEST_ALLOCATION_FAILURE_EXECUTABLE}"
                --archive-allocation-probe serialize "${invalid_value}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error)
        if(NOT result EQUAL 12)
            message(FATAL_ERROR
                "Archive allocation probe accepted ${invalid_value} (result ${result}):\n${output}${error}")
        endif()
        string(FIND "${output}${error}"
            "invalid archive allocation failure index"
            diagnostic_position)
        if(diagnostic_position EQUAL -1)
            message(FATAL_ERROR
                "Archive allocation probe did not diagnose ${invalid_value}:\n${output}${error}")
        endif()
    endforeach()
endif()
