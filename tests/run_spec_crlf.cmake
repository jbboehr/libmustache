if(NOT DEFINED TEST_SPEC_EXECUTABLE OR
   NOT DEFINED MUSTACHE_SPEC_DIR OR
   NOT DEFINED CRLF_SPEC_DIR)
    message(FATAL_ERROR "The CRLF specification test is missing an input")
endif()

file(REMOVE_RECURSE "${CRLF_SPEC_DIR}")
file(MAKE_DIRECTORY "${CRLF_SPEC_DIR}")
file(GLOB specification_files LIST_DIRECTORIES FALSE
    "${MUSTACHE_SPEC_DIR}/*.yml")

foreach(specification_file IN LISTS specification_files)
    get_filename_component(filename "${specification_file}" NAME)
    file(READ "${specification_file}" contents)
    string(REPLACE "\r\n" "\n" contents "${contents}")
    string(REPLACE "\n" "\r\n" contents "${contents}")
    file(WRITE "${CRLF_SPEC_DIR}/${filename}" "${contents}")
endforeach()

execute_process(
    COMMAND "${TEST_SPEC_EXECUTABLE}" "${CRLF_SPEC_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR
        "CRLF specification suite failed with ${result}\n${output}${error}")
endif()
