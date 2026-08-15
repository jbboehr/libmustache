cmake_minimum_required(VERSION 3.18)

project(mustache VERSION 0.6.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(MUSTACHE_ABI_VERSION 6)
set(MUSTACHE_PACKAGE_VERSION "${PROJECT_VERSION}")
set(MUSTACHE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(MUSTACHE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(MUSTACHE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(MUSTACHE_CXX_STANDARD 17)

# Keep pkg-config consumers on the compiler-specific spelling selected by
# CMake (for example, -std=c++1z on GCC 7). Some compilers express a language
# mode with more than one option, while others need no option at all.
string(JOIN " " MUSTACHE_CXX_STANDARD_FLAG
    ${CMAKE_CXX17_STANDARD_COMPILE_OPTION})

set(MUSTACHE_SPEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spec/specs" CACHE PATH
    "Path to the mustache specification suites")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MUSTACHE_IS_TOP_LEVEL ON)
else()
    set(MUSTACHE_IS_TOP_LEVEL OFF)
endif()

option(MUSTACHE_BUILD_CLI "Build the mustache command-line program" ${MUSTACHE_IS_TOP_LEVEL})
option(MUSTACHE_ENABLE_TESTS "Build and register the test suite" ${MUSTACHE_IS_TOP_LEVEL})
option(MUSTACHE_ENABLE_HARDENING "Enable supported compiler and linker hardening" ON)
option(MUSTACHE_ENABLE_ASSERTIONS "Enable standard-library runtime assertions" ON)
option(MUSTACHE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MUSTACHE_ENABLE_FUZZING "Build the Clang libFuzzer targets and smoke tests" OFF)

if(MUSTACHE_ENABLE_FUZZING)
    if(NOT MUSTACHE_ENABLE_TESTS)
        message(FATAL_ERROR "MUSTACHE_ENABLE_FUZZING requires MUSTACHE_ENABLE_TESTS")
    endif()
    if(NOT MUSTACHE_ENABLE_SANITIZERS)
        message(FATAL_ERROR "MUSTACHE_ENABLE_FUZZING requires MUSTACHE_ENABLE_SANITIZERS")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "MUSTACHE_ENABLE_FUZZING requires a Clang compiler")
    endif()
endif()

find_package(nlohmann_json 3.10.5 CONFIG REQUIRED)
set(MUSTACHE_JSON_TARGET nlohmann_json::nlohmann_json)

if(MSVC)
    find_package(yaml CONFIG REQUIRED)
    if(TARGET yaml)
        set(MUSTACHE_YAML_TARGET yaml)
    elseif(TARGET yaml::yaml)
        set(MUSTACHE_YAML_TARGET yaml::yaml)
    else()
        message(FATAL_ERROR "The yaml package did not provide a supported CMake target")
    endif()

else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(YAML REQUIRED IMPORTED_TARGET yaml-0.1)
    set(MUSTACHE_YAML_TARGET PkgConfig::YAML)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mustache_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/src/mustache_config.h"
    @ONLY
)

# Resolve the prefix relative to the installed pkg-config file so that
# `cmake --install --prefix` and relocated install trees remain usable.
set(prefix "\${pcfiledir}/../..")
set(exec_prefix "\${prefix}")
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(libdir "${CMAKE_INSTALL_LIBDIR}")
else()
    set(libdir "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
set(VERSION "${PROJECT_VERSION}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/mustache.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mustache.pc"
    @ONLY
)

function(mustache_apply_build_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(MUSTACHE_ENABLE_ASSERTIONS)
            target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS=1)
        endif()
        if(MUSTACHE_ENABLE_HARDENING)
            target_compile_options(${target} PRIVATE -fstack-protector-strong)
            target_compile_definitions(${target} PRIVATE
                $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=3>)
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                target_link_options(${target} PRIVATE -Wl,-z,relro -Wl,-z,now)
            endif()
        endif()
        if(MUSTACHE_ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE
                -fno-omit-frame-pointer
                -fsanitize=address,undefined
                -fno-sanitize-recover=all)
            target_link_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-sanitize-recover=all)
        endif()
    endif()
endfunction()

add_subdirectory(src)

if(MUSTACHE_BUILD_CLI)
    add_subdirectory(bin)
endif()

if(MUSTACHE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mustacheConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mustacheConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mustache"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mustacheConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMinorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mustacheConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mustacheConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mustache"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mustache.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
