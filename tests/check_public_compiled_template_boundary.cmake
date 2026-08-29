foreach(required_variable IN ITEMS
        MUSTACHE_PUBLIC_HEADER
        MUSTACHE_CONFIG_INCLUDE_DIR
        MUSTACHE_SOURCE_INCLUDE_DIR
        MUSTACHE_TEST_BINARY_DIR
        MUSTACHE_TEST_GENERATOR
        MUSTACHE_TEST_CXX_COMPILER)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT EXISTS "${MUSTACHE_PUBLIC_HEADER}")
    message(FATAL_ERROR "The installable compiled-template header is missing: ${MUSTACHE_PUBLIC_HEADER}")
endif()
file(READ "${MUSTACHE_PUBLIC_HEADER}" header_contents)
if(header_contents MATCHES "CompiledTemplateAccess" OR
        header_contents MATCHES "namespace[ \t]+detail" OR
        header_contents MATCHES "friend[^\n]*detail::")
    message(FATAL_ERROR
        "The installable compiled-template header exposes an internal detail access bridge")
endif()

set(consumer_source_dir "${MUSTACHE_TEST_BINARY_DIR}/boundary-consumer")
set(consumer_build_dir "${MUSTACHE_TEST_BINARY_DIR}/boundary-consumer-build")
file(MAKE_DIRECTORY "${consumer_source_dir}")
file(WRITE "${consumer_source_dir}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.15)
project(compiled_template_boundary_consumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(boundary_control OBJECT control.cpp)
target_include_directories(boundary_control PRIVATE
    "${MUSTACHE_CONFIG_INCLUDE_DIR}"
    "${MUSTACHE_SOURCE_INCLUDE_DIR}")
add_library(boundary_exploit OBJECT exploit.cpp)
target_include_directories(boundary_exploit PRIVATE
    "${MUSTACHE_CONFIG_INCLUDE_DIR}"
    "${MUSTACHE_SOURCE_INCLUDE_DIR}")
]=])
file(WRITE "${consumer_source_dir}/control.cpp" [=[
#include "compiled_template.hpp"

mustache::CompiledTemplate * compiledTemplatePointer()
{
  return nullptr;
}
]=])
file(WRITE "${consumer_source_dir}/exploit.cpp" [=[
#include "compiled_template.hpp"

const void * archivedTemplateRoot(const mustache::CompiledTemplate& compiled)
{
  return compiled.archivedTemplateRoot();
}
]=])

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${consumer_source_dir}"
    -B "${consumer_build_dir}"
    -G "${MUSTACHE_TEST_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${MUSTACHE_TEST_CXX_COMPILER}"
    "-DMUSTACHE_CONFIG_INCLUDE_DIR=${MUSTACHE_CONFIG_INCLUDE_DIR}"
    "-DMUSTACHE_SOURCE_INCLUDE_DIR=${MUSTACHE_SOURCE_INCLUDE_DIR}")
if(DEFINED MUSTACHE_TEST_GENERATOR_PLATFORM AND
        NOT MUSTACHE_TEST_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND configure_command -A "${MUSTACHE_TEST_GENERATOR_PLATFORM}")
endif()
if(DEFINED MUSTACHE_TEST_GENERATOR_TOOLSET AND
        NOT MUSTACHE_TEST_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND configure_command -T "${MUSTACHE_TEST_GENERATOR_TOOLSET}")
endif()
execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Configuring the installed-header consumer failed:\n${configure_output}${configure_error}")
endif()

set(build_config_arguments)
if(DEFINED MUSTACHE_TEST_CONFIG AND NOT MUSTACHE_TEST_CONFIG STREQUAL "")
    list(APPEND build_config_arguments --config "${MUSTACHE_TEST_CONFIG}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build_dir}"
        --target boundary_control ${build_config_arguments}
    RESULT_VARIABLE control_result
    OUTPUT_VARIABLE control_output
    ERROR_VARIABLE control_error)
if(NOT control_result EQUAL 0)
    message(FATAL_ERROR
        "The installed-header control consumer failed to compile:\n${control_output}${control_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build_dir}"
        --target boundary_exploit ${build_config_arguments}
    RESULT_VARIABLE exploit_result
    OUTPUT_VARIABLE exploit_output
    ERROR_VARIABLE exploit_error)
if(exploit_result EQUAL 0)
    message(FATAL_ERROR
        "A downstream consumer could call the private compiled-template archive accessor")
endif()
string(TOLOWER "${exploit_output}\n${exploit_error}" exploit_diagnostics)
if(MUSTACHE_ARCHIVED_TEMPLATES)
    set(expected_access_diagnostic "(private|cannot access)")
else()
    set(expected_access_diagnostic "(no member|not a member|has no member)")
endif()
if(NOT exploit_diagnostics MATCHES "archivedtemplateroot" OR
        NOT exploit_diagnostics MATCHES "${expected_access_diagnostic}")
    message(FATAL_ERROR
        "The exploit compilation failed for an unexpected reason:\n${exploit_output}${exploit_error}")
endif()
