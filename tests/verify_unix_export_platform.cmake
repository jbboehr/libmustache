include("${CMAKE_CURRENT_LIST_DIR}/select_unix_export_format.cmake")

function(expect_export_format system_name is_apple is_unix expected)
    mustache_select_unix_export_format(
        actual "${system_name}" "${is_apple}" "${is_unix}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "${system_name} export format: expected '${expected}', got '${actual}'")
    endif()
endfunction()

expect_export_format(Linux FALSE TRUE ELF)
expect_export_format(Darwin TRUE TRUE MACHO)
expect_export_format(CYGWIN FALSE TRUE "")
expect_export_format(SunOS FALSE TRUE "")
expect_export_format(AIX FALSE TRUE "")
expect_export_format(Windows FALSE FALSE "")
