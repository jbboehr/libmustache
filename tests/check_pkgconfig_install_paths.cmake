cmake_minimum_required(VERSION 3.18)

foreach(required IN ITEMS MUSTACHE_TEST_SOURCE_DIR MUSTACHE_TEST_BINARY_ROOT
        MUSTACHE_TEST_GENERATOR MUSTACHE_TEST_CXX_COMPILER MUSTACHE_TEST_PKG_CONFIG)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/copy_cmake_test_source.cmake")
set(source "${MUSTACHE_TEST_BINARY_ROOT}/source")
mustache_copy_cmake_test_source("${MUSTACHE_TEST_SOURCE_DIR}" "${source}")
set(build "${MUSTACHE_TEST_BINARY_ROOT}/build")
set(consumer_source "${MUSTACHE_TEST_BINARY_ROOT}/consumer")
file(MAKE_DIRECTORY "${consumer_source}")
file(WRITE "${consumer_source}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.18)
project(pkgconfig_consumer LANGUAGES CXX)
separate_arguments(cflags UNIX_COMMAND "${MUSTACHE_PKGCONFIG_CFLAGS}")
add_executable(pkgconfig_consumer main.cpp)
target_compile_options(pkgconfig_consumer PRIVATE ${cflags})
# CMake forwards link flags verbatim, so retain pkg-config's shell escaping.
target_link_libraries(pkgconfig_consumer PRIVATE "${MUSTACHE_PKGCONFIG_LIBS}")
set_target_properties(pkgconfig_consumer PROPERTIES
    BUILD_RPATH "${MUSTACHE_LIBRARY_DIR}")
enable_testing()
add_test(NAME consumer COMMAND pkgconfig_consumer)
]=])
file(WRITE "${consumer_source}/main.cpp" [=[
#include <mustache/mustache.hpp>

int main()
{
    const auto data = mustache::Data::string("pkg-config");
    return mustache::render(mustache::compile("{{.}}"), data) == "pkg-config" ? 0 : 1;
}
]=])

set(configure_arguments -G "${MUSTACHE_TEST_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${MUSTACHE_TEST_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${MUSTACHE_TEST_BUILD_TYPE}")
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

function(check_install name phase prefix library_dir include_dir)
    set(pkg_environment "${CMAKE_COMMAND}" -E env
        "PKG_CONFIG_PATH=${library_dir}/pkgconfig"
        "PKG_CONFIG_LIBDIR=${library_dir}/pkgconfig"
        "PKG_CONFIG_SYSROOT_DIR=")
    set(pkg_arguments --dont-define-prefix)
    if(phase STREQUAL "relocated")
        # Moving an installed tree requires an explicit prefix override.
        list(APPEND pkg_arguments "--define-variable=prefix=${prefix}")
    endif()
    foreach(variable IN ITEMS prefix libdir includedir)
        execute_process(
            COMMAND ${pkg_environment} "${MUSTACHE_TEST_PKG_CONFIG}"
                ${pkg_arguments} "--variable=${variable}" mustache
            RESULT_VARIABLE result OUTPUT_VARIABLE value ERROR_VARIABLE error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${name}/${phase}: pkg-config query failed: ${error}")
        endif()
        # pkgconf retains shell escapes in variable output; freedesktop
        # pkg-config returns literal values. Decode before normalizing.
        string(REGEX REPLACE "\\\\([ \t\"'\\\\])" "\\1" value "${value}")
        get_filename_component(actual "${value}" ABSOLUTE)
        if(variable STREQUAL "prefix")
            set(expected "${prefix}")
        elseif(variable STREQUAL "libdir")
            set(expected "${library_dir}")
        else()
            set(expected "${include_dir}")
        endif()
        if(NOT actual STREQUAL expected)
            message(FATAL_ERROR
                "${name}/${phase}: wrong ${variable}\nExpected: ${expected}\nActual: ${actual}")
        endif()
    endforeach()
    foreach(flags IN ITEMS cflags libs)
        execute_process(
            COMMAND ${pkg_environment} "${MUSTACHE_TEST_PKG_CONFIG}"
                ${pkg_arguments} "--${flags}" mustache
            RESULT_VARIABLE result OUTPUT_VARIABLE ${flags} ERROR_VARIABLE error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${name}/${phase}: pkg-config flags failed: ${error}")
        endif()
    endforeach()
    set(consumer_build "${MUSTACHE_TEST_BINARY_ROOT}/consumer-${name}-${phase}")
    check_command("${name}/${phase} consumer configure"
        "${CMAKE_COMMAND}" -S "${consumer_source}" -B "${consumer_build}"
        ${configure_arguments}
        "-DMUSTACHE_PKGCONFIG_CFLAGS=${cflags}"
        "-DMUSTACHE_PKGCONFIG_LIBS=${libs}"
        "-DMUSTACHE_LIBRARY_DIR=${library_dir}")
    check_command("${name}/${phase} consumer build"
        "${CMAKE_COMMAND}" --build "${consumer_build}" ${build_arguments})
    check_command("${name}/${phase} consumer run"
        "${CMAKE_COMMAND}" -E chdir "${consumer_build}" "${CMAKE_CTEST_COMMAND}"
        ${test_arguments} --output-on-failure)
    message(STATUS "${name}/${phase}: paths, compilation, linking, and rendering passed")
endfunction()

# Fresh install locations allow real renames on repeat runs while reusing the
# library build, whose source and feature configuration stay the same.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(install_root "${MUSTACHE_TEST_BINARY_ROOT}/install-${run_id}")
foreach(name IN ITEMS nested lib lib64 absolute apostrophe absolute-apostrophe "with spaces")
    set(case_root "${install_root}/${name}")
    if(name STREQUAL "apostrophe")
        set(case_root "${case_root}/O'Brien")
    endif()
    set(configured_prefix "${case_root}/configured")
    set(include_dir include)
    if(name STREQUAL "nested")
        set(library_dir lib/x86_64-linux-gnu)
    elseif(name MATCHES "^absolute")
        set(configured_prefix "${configured_prefix} prefix")
        set(library_dir "${install_root}/fixed libraries")
        set(include_dir "${install_root}/fixed headers")
        if(name STREQUAL "absolute-apostrophe")
            string(APPEND configured_prefix " O'Brien")
            string(APPEND library_dir " O'Brien")
            string(APPEND include_dir " O'Brien")
        endif()
    elseif(name STREQUAL "apostrophe")
        set(library_dir lib)
    elseif(name STREQUAL "with spaces")
        set(library_dir "lib/nested dir")
        set(include_dir "sdk/header dir")
    else()
        set(library_dir "${name}")
        if(name STREQUAL "lib64")
            set(include_dir sdk/include)
        endif()
    endif()
    check_command("${name} configure"
        "${CMAKE_COMMAND}" -S "${source}" -B "${build}"
        ${configure_arguments}
        "-DCMAKE_INSTALL_PREFIX=${configured_prefix}"
        "-DCMAKE_INSTALL_LIBDIR=${library_dir}"
        "-DCMAKE_INSTALL_INCLUDEDIR=${include_dir}"
        -DMUSTACHE_BUILD_CLI=OFF -DMUSTACHE_ENABLE_TESTS=OFF
        -DMUSTACHE_ENABLE_HARDENING=OFF -DMUSTACHE_WARNINGS_AS_ERRORS=ON
        -DMUSTACHE_ENABLE_JSON=OFF -DMUSTACHE_ENABLE_YAML=OFF
        -DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=OFF)
    check_command("${name} build"
        "${CMAKE_COMMAND}" --build "${build}" ${build_arguments} --parallel 2)
    check_command("${name} install"
        "${CMAKE_COMMAND}" --install "${build}" ${build_arguments})
    if(name MATCHES "^absolute")
        check_install("${name}" configured "${configured_prefix}" "${library_dir}" "${include_dir}")
    else()
        check_install("${name}" configured "${configured_prefix}"
            "${configured_prefix}/${library_dir}" "${configured_prefix}/${include_dir}")
        file(RENAME "${configured_prefix}" "${configured_prefix}-saved")
        set(installed_prefix "${case_root}/installed")
        check_command("${name} install with prefix override"
            "${CMAKE_COMMAND}" --install "${build}" ${build_arguments}
            --prefix "${installed_prefix}")
        check_install("${name}" overridden "${installed_prefix}"
            "${installed_prefix}/${library_dir}" "${installed_prefix}/${include_dir}")
        set(relocated_prefix "${case_root}/relocated")
        file(RENAME "${installed_prefix}" "${relocated_prefix}")
        check_install("${name}" relocated "${relocated_prefix}"
            "${relocated_prefix}/${library_dir}" "${relocated_prefix}/${include_dir}")
    endif()
endforeach()
