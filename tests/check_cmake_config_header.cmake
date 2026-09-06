cmake_minimum_required(VERSION 3.18)

foreach(required IN ITEMS MUSTACHE_TEST_SOURCE_DIR MUSTACHE_TEST_BINARY_ROOT
        MUSTACHE_TEST_GENERATOR MUSTACHE_TEST_CXX_COMPILER)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# Use a private source copy: this test creates and removes a generated header.
include("${CMAKE_CURRENT_LIST_DIR}/copy_cmake_test_source.cmake")
set(MUSTACHE_SOURCE_COPY "${MUSTACHE_TEST_BINARY_ROOT}/source")
mustache_copy_cmake_test_source("${MUSTACHE_TEST_SOURCE_DIR}" "${MUSTACHE_SOURCE_COPY}")

set(MUSTACHE_CONFIGURE_ARGUMENTS -G "${MUSTACHE_TEST_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${MUSTACHE_TEST_CXX_COMPILER}"
    -DMUSTACHE_BUILD_CLI=OFF
    -DMUSTACHE_ENABLE_TESTS=OFF
    -DMUSTACHE_ENABLE_HARDENING=OFF
    -DMUSTACHE_ENABLE_JSON=OFF
    -DMUSTACHE_ENABLE_YAML=OFF
    -DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=OFF)
if(MUSTACHE_TEST_GENERATOR_PLATFORM)
    list(APPEND MUSTACHE_CONFIGURE_ARGUMENTS
        -A "${MUSTACHE_TEST_GENERATOR_PLATFORM}")
endif()
if(MUSTACHE_TEST_GENERATOR_TOOLSET)
    list(APPEND MUSTACHE_CONFIGURE_ARGUMENTS
        -T "${MUSTACHE_TEST_GENERATOR_TOOLSET}")
endif()
if(MUSTACHE_TEST_TOOLCHAIN_FILE)
    list(APPEND MUSTACHE_CONFIGURE_ARGUMENTS
        "-DCMAKE_TOOLCHAIN_FILE=${MUSTACHE_TEST_TOOLCHAIN_FILE}")
endif()

function(mustache_check_configure label source binary expect_conflict)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${binary}"
            ${MUSTACHE_CONFIGURE_ARGUMENTS}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(expect_conflict)
        if(result EQUAL 0 OR NOT "${output}${error}" MATCHES
                "source-tree mustache_config.h")
            message(FATAL_ERROR
                "${label}: expected a configuration-header conflict, got ${result}:\n${output}${error}")
        endif()
    elseif(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${label}: configuration failed (${result}):\n${output}${error}")
    endif()
    message(STATUS "${label}: passed")
endfunction()

set(MUSTACHE_STALE_HEADER "${MUSTACHE_SOURCE_COPY}/src/mustache_config.h")
set(MUSTACHE_STALE_CONTENTS
    "#define MUSTACHE_HAVE_LIBJSON 1\n#define MUSTACHE_HAVE_LIBYAML 1\n")
file(WRITE "${MUSTACHE_STALE_HEADER}" "${MUSTACHE_STALE_CONTENTS}")
mustache_check_configure("Conflicting Autotools header"
    "${MUSTACHE_SOURCE_COPY}" "${MUSTACHE_TEST_BINARY_ROOT}/build" TRUE)

# The diagnostic must also apply when libmustache is a subproject.
set(MUSTACHE_PARENT_SOURCE "${MUSTACHE_TEST_BINARY_ROOT}/parent")
file(MAKE_DIRECTORY "${MUSTACHE_PARENT_SOURCE}")
file(WRITE "${MUSTACHE_PARENT_SOURCE}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.18)\n"
    "project(config_header_parent LANGUAGES CXX)\n"
    "add_subdirectory(\"${MUSTACHE_SOURCE_COPY}\" libmustache)\n")
mustache_check_configure("Conflicting subproject header"
    "${MUSTACHE_PARENT_SOURCE}" "${MUSTACHE_TEST_BINARY_ROOT}/parent-build" TRUE)
file(READ "${MUSTACHE_STALE_HEADER}" MUSTACHE_HEADER_AFTER_REJECTION)
if(NOT MUSTACHE_HEADER_AFTER_REJECTION STREQUAL MUSTACHE_STALE_CONTENTS)
    message(FATAL_ERROR "Configuration modified the conflicting source header")
endif()

file(REMOVE "${MUSTACHE_STALE_HEADER}")
mustache_check_configure("Retry after removing the header"
    "${MUSTACHE_SOURCE_COPY}" "${MUSTACHE_TEST_BINARY_ROOT}/build" FALSE)
mustache_check_configure("Subproject retry after removing the header"
    "${MUSTACHE_PARENT_SOURCE}" "${MUSTACHE_TEST_BINARY_ROOT}/parent-build" FALSE)

# In-source CMake builds generate this same header and must remain reconfigurable.
mustache_check_configure("In-source configuration"
    "${MUSTACHE_SOURCE_COPY}" "${MUSTACHE_SOURCE_COPY}" FALSE)
mustache_check_configure("In-source reconfiguration"
    "${MUSTACHE_SOURCE_COPY}" "${MUSTACHE_SOURCE_COPY}" FALSE)
mustache_check_configure("Conflicting in-source CMake header"
    "${MUSTACHE_SOURCE_COPY}" "${MUSTACHE_TEST_BINARY_ROOT}/build" TRUE)

mustache_copy_cmake_test_source("${MUSTACHE_SOURCE_COPY}"
    "${MUSTACHE_TEST_BINARY_ROOT}/clean-source")
mustache_check_configure("Nested test source copied from an in-source build"
    "${MUSTACHE_TEST_BINARY_ROOT}/clean-source"
    "${MUSTACHE_TEST_BINARY_ROOT}/clean-build" FALSE)
