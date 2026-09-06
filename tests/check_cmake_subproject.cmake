cmake_minimum_required(VERSION 3.18)

foreach(required IN ITEMS MUSTACHE_TEST_SOURCE_DIR MUSTACHE_TEST_BINARY_ROOT
        MUSTACHE_TEST_GENERATOR MUSTACHE_TEST_CXX_COMPILER)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

if(NOT DEFINED MUSTACHE_TEST_ARCHIVED_TEMPLATES)
    set(MUSTACHE_TEST_ARCHIVED_TEMPLATES OFF)
endif()

# Nested configuration must also work when the parent was built in-source.
include("${CMAKE_CURRENT_LIST_DIR}/copy_cmake_test_source.cmake")
set(source "${MUSTACHE_TEST_BINARY_ROOT}/source")
mustache_copy_cmake_test_source("${MUSTACHE_TEST_SOURCE_DIR}" "${source}")
set(build "${MUSTACHE_TEST_BINARY_ROOT}/build")

set(configure_arguments -G "${MUSTACHE_TEST_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${MUSTACHE_TEST_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${MUSTACHE_TEST_BUILD_TYPE}")
if(MUSTACHE_TEST_GENERATOR_PLATFORM)
    list(APPEND configure_arguments -A "${MUSTACHE_TEST_GENERATOR_PLATFORM}")
endif()
if(MUSTACHE_TEST_GENERATOR_TOOLSET)
    list(APPEND configure_arguments -T "${MUSTACHE_TEST_GENERATOR_TOOLSET}")
endif()
if(MUSTACHE_TEST_TOOLCHAIN_FILE)
    list(APPEND configure_arguments
        "-DCMAKE_TOOLCHAIN_FILE=${MUSTACHE_TEST_TOOLCHAIN_FILE}")
endif()
set(build_arguments)
set(test_arguments)
if(MUSTACHE_TEST_CONFIG)
    list(APPEND build_arguments --config "${MUSTACHE_TEST_CONFIG}")
    list(APPEND test_arguments --build-config "${MUSTACHE_TEST_CONFIG}")
endif()

function(check_command label)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${label} failed (${result}):\n${output}${error}")
    endif()
endfunction()

check_command("Subproject configure"
    "${CMAKE_COMMAND}" -S "${source}/tests/cmake-subproject" -B "${build}"
    ${configure_arguments}
    -DMUSTACHE_WARNINGS_AS_ERRORS=ON
    -DMUSTACHE_ENABLE_JSON=OFF -DMUSTACHE_ENABLE_YAML=OFF
    "-DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=${MUSTACHE_TEST_ARCHIVED_TEMPLATES}")
check_command("Subproject build"
    "${CMAKE_COMMAND}" --build "${build}" ${build_arguments} --parallel 2)
check_command("Subproject consumers"
    "${CMAKE_COMMAND}" -E chdir "${build}" "${CMAKE_CTEST_COMMAND}"
    ${test_arguments} --output-on-failure)
message(STATUS "Independent public headers and shared/static subproject consumers passed")
