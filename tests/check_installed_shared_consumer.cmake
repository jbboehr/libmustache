foreach(required_variable IN ITEMS
        MUSTACHE_BUILD_DIRECTORY
        MUSTACHE_CONSUMER_SOURCE_DIRECTORY
        MUSTACHE_INSTALL_LIBDIR
        MUSTACHE_SHARED_LIBRARY_FILE_NAME
        MUSTACHE_TEST_ROOT
        MUSTACHE_ZLIB_LIBRARY
        MUSTACHE_ZLIB_LIBRARY_DIRECTORY)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT IS_DIRECTORY "${MUSTACHE_BUILD_DIRECTORY}")
    message(FATAL_ERROR
        "MUSTACHE_BUILD_DIRECTORY is not a directory: ${MUSTACHE_BUILD_DIRECTORY}")
endif()

get_filename_component(build_directory
    "${MUSTACHE_BUILD_DIRECTORY}" REALPATH)
get_filename_component(test_root "${MUSTACHE_TEST_ROOT}" REALPATH)
file(RELATIVE_PATH test_root_relative "${build_directory}" "${test_root}")
if(test_root_relative STREQUAL "" OR test_root_relative STREQUAL "." OR
        IS_ABSOLUTE "${test_root_relative}" OR
        test_root_relative MATCHES "^\\.\\.([/\\\\]|$)")
    message(FATAL_ERROR
        "MUSTACHE_TEST_ROOT must be safely beneath MUSTACHE_BUILD_DIRECTORY\n"
        "  build directory: ${build_directory}\n"
        "  test root: ${test_root}")
endif()

set(install_directory "${test_root}/install")
set(consumer_build_directory "${test_root}/consumer")
file(REMOVE_RECURSE
    "${install_directory}"
    "${consumer_build_directory}")

set(install_command
    "${CMAKE_COMMAND}"
    --install "${MUSTACHE_BUILD_DIRECTORY}"
    --prefix "${install_directory}")
if(DEFINED MUSTACHE_BUILD_CONFIG AND
        NOT "${MUSTACHE_BUILD_CONFIG}" STREQUAL "")
    list(APPEND install_command --config "${MUSTACHE_BUILD_CONFIG}")
endif()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "Installing libmustache failed (${install_result})\n"
        "${install_output}${install_error}")
endif()

get_filename_component(expected_zlib_library
    "${MUSTACHE_ZLIB_LIBRARY}" REALPATH)
get_filename_component(expected_zlib_directory
    "${MUSTACHE_ZLIB_LIBRARY_DIRECTORY}" REALPATH)
if(NOT EXISTS "${expected_zlib_library}" OR
        NOT IS_DIRECTORY "${expected_zlib_directory}")
    message(FATAL_ERROR
        "The selected zlib library or dependency directory is missing\n"
        "  library: ${expected_zlib_library}\n"
        "  directory: ${expected_zlib_directory}")
endif()

set(installed_shared_library
    "${install_directory}/${MUSTACHE_INSTALL_LIBDIR}/${MUSTACHE_SHARED_LIBRARY_FILE_NAME}")
if(NOT EXISTS "${installed_shared_library}")
    message(FATAL_ERROR
        "The installed shared library is missing: ${installed_shared_library}")
endif()

file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${installed_shared_library}"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies)
set(selected_zlib_resolved FALSE)
foreach(resolved_dependency IN LISTS resolved_dependencies)
    get_filename_component(resolved_dependency
        "${resolved_dependency}" REALPATH)
    if(resolved_dependency STREQUAL expected_zlib_library)
        set(selected_zlib_resolved TRUE)
        break()
    endif()
endforeach()
if(NOT selected_zlib_resolved)
    message(FATAL_ERROR
        "The installed shared library does not resolve the selected zlib\n"
        "  expected: ${expected_zlib_library}\n"
        "  resolved: ${resolved_dependencies}\n"
        "  unresolved: ${unresolved_dependencies}")
endif()

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${MUSTACHE_CONSUMER_SOURCE_DIRECTORY}"
    -B "${consumer_build_directory}"
    "-DCMAKE_PREFIX_PATH=${install_directory}"
    -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE
    -DMUSTACHE_CONSUMER_LINKAGE=shared)
if(DEFINED MUSTACHE_BUILD_CONFIG AND
        NOT "${MUSTACHE_BUILD_CONFIG}" STREQUAL "")
    list(APPEND configure_command
        "-DCMAKE_BUILD_TYPE=${MUSTACHE_BUILD_CONFIG}")
endif()
if(MUSTACHE_ENABLE_SANITIZERS)
    list(APPEND configure_command
        "-DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all")
endif()

execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Configuring the installed shared consumer failed (${configure_result})\n"
        "${configure_output}${configure_error}")
endif()

set(build_command
    "${CMAKE_COMMAND}"
    --build "${consumer_build_directory}")
if(DEFINED MUSTACHE_BUILD_CONFIG AND
        NOT "${MUSTACHE_BUILD_CONFIG}" STREQUAL "")
    list(APPEND build_command --config "${MUSTACHE_BUILD_CONFIG}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        --unset=LD_LIBRARY_PATH
        --unset=LIBRARY_PATH
        ${build_command}
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Linking the installed shared consumer failed (${build_result})\n"
        "${build_output}${build_error}")
endif()

set(consumer_executable
    "${consumer_build_directory}/mustache_consumer${CMAKE_EXECUTABLE_SUFFIX}")
if(DEFINED MUSTACHE_BUILD_CONFIG AND
        EXISTS "${consumer_build_directory}/${MUSTACHE_BUILD_CONFIG}/mustache_consumer${CMAKE_EXECUTABLE_SUFFIX}")
    set(consumer_executable
        "${consumer_build_directory}/${MUSTACHE_BUILD_CONFIG}/mustache_consumer${CMAKE_EXECUTABLE_SUFFIX}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        --unset=LD_LIBRARY_PATH
        "${consumer_executable}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR
        "Running the installed shared consumer failed (${run_result})\n"
        "${run_output}${run_error}")
endif()
