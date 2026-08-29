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

# Keep pkg-config consumers on the compiler-specific C++17 spelling selected
# by CMake. Some compilers express a language mode with more than one option,
# while others need no option at all.
string(JOIN " " MUSTACHE_CXX_STANDARD_FLAG
    ${CMAKE_CXX17_STANDARD_COMPILE_OPTION})

include(CheckCXXSourceCompiles)
function(mustache_check_floating_to_chars result_variable)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    check_cxx_source_compiles([=[
        #include <charconv>
        #include <system_error>

        int main()
        {
            char buffer[64];
            const auto result = std::to_chars(buffer, buffer + sizeof(buffer), 1.25);
            return result.ec == std::errc{} ? 0 : 1;
        }
    ]=] MUSTACHE_FLOATING_TO_CHARS_PROBE_RESULT)
    set(${result_variable} "${MUSTACHE_FLOATING_TO_CHARS_PROBE_RESULT}" PARENT_SCOPE)
endfunction()

mustache_check_floating_to_chars(MUSTACHE_HAVE_FLOATING_TO_CHARS)
if(NOT MUSTACHE_HAVE_FLOATING_TO_CHARS)
    message(FATAL_ERROR
        "A C++17 standard library with floating-point std::to_chars is required")
endif()

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
option(MUSTACHE_WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" OFF)
option(MUSTACHE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MUSTACHE_ENABLE_FUZZING "Build the Clang libFuzzer targets and smoke tests" OFF)
option(MUSTACHE_ENABLE_CLANG_TIDY "Run clang-tidy on production targets" OFF)
set(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES AUTO CACHE STRING
    "Build archived-template support: AUTO, ON, or OFF")
set_property(CACHE MUSTACHE_ENABLE_ARCHIVED_TEMPLATES
    PROPERTY STRINGS AUTO ON OFF)
option(MUSTACHE_USE_SYSTEM_CISTA
    "Use a system Cista package instead of the bundled private snapshot" OFF)
option(MUSTACHE_USE_SYSTEM_XXHASH
    "Use a system xxHash package instead of the bundled private snapshot" OFF)
option(MUSTACHE_ENABLE_CISTA_BENCHMARK "Build the experimental Cista archive benchmark" OFF)
set(MUSTACHE_ENABLE_JSON AUTO CACHE STRING
    "Build JSON support: AUTO, ON, or OFF")
set_property(CACHE MUSTACHE_ENABLE_JSON PROPERTY STRINGS AUTO ON OFF)
set(MUSTACHE_ENABLE_YAML AUTO CACHE STRING
    "Build YAML support: AUTO, ON, or OFF")
set_property(CACHE MUSTACHE_ENABLE_YAML PROPERTY STRINGS AUTO ON OFF)
option(MUSTACHE_CISTA_BENCHMARK_BUILTIN_XXH3
    "Use Cista runtime versioning and built-in integrity with the supplied XXH3"
    ${MUSTACHE_ENABLE_CISTA_BENCHMARK})

if(MUSTACHE_CISTA_BENCHMARK_BUILTIN_XXH3 AND NOT MUSTACHE_ENABLE_CISTA_BENCHMARK)
    message(FATAL_ERROR
        "MUSTACHE_CISTA_BENCHMARK_BUILTIN_XXH3 requires MUSTACHE_ENABLE_CISTA_BENCHMARK")
endif()

foreach(MUSTACHE_OPTION IN ITEMS
        MUSTACHE_ENABLE_JSON
        MUSTACHE_ENABLE_YAML
        MUSTACHE_ENABLE_ARCHIVED_TEMPLATES)
    string(TOUPPER "${${MUSTACHE_OPTION}}" MUSTACHE_OPTION_VALUE)
    if(MUSTACHE_OPTION STREQUAL "MUSTACHE_ENABLE_ARCHIVED_TEMPLATES")
        # This setting was a CMake boolean before it gained AUTO. Preserve
        # existing TRUE/FALSE and other standard boolean cache values.
        if(MUSTACHE_OPTION_VALUE MATCHES "^(TRUE|YES|Y|1)$")
            set(MUSTACHE_OPTION_VALUE ON)
        elseif(MUSTACHE_OPTION_VALUE MATCHES "^(FALSE|NO|N|0)$")
            set(MUSTACHE_OPTION_VALUE OFF)
        endif()
    endif()
    if(NOT MUSTACHE_OPTION_VALUE MATCHES "^(AUTO|ON|OFF)$")
        message(FATAL_ERROR
            "${MUSTACHE_OPTION} must be AUTO, ON, or OFF")
    endif()
    set(${MUSTACHE_OPTION} "${MUSTACHE_OPTION_VALUE}" CACHE STRING
        "Build optional support: AUTO, ON, or OFF" FORCE)
    set_property(CACHE ${MUSTACHE_OPTION} PROPERTY STRINGS AUTO ON OFF)
endforeach()

set(MUSTACHE_ARCHIVE_PRIVATE_VISIBILITY_SUPPORTED OFF)
if(MSVC)
    set(MUSTACHE_ARCHIVE_PRIVATE_VISIBILITY_SUPPORTED ON)
else()
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-fvisibility=hidden
        MUSTACHE_ARCHIVE_VISIBILITY_HIDDEN_SUPPORTED)
    check_cxx_compiler_flag(-fvisibility-inlines-hidden
        MUSTACHE_ARCHIVE_VISIBILITY_INLINES_HIDDEN_SUPPORTED)
    if(MUSTACHE_ARCHIVE_VISIBILITY_HIDDEN_SUPPORTED AND
            MUSTACHE_ARCHIVE_VISIBILITY_INLINES_HIDDEN_SUPPORTED)
        set(MUSTACHE_ARCHIVE_PRIVATE_VISIBILITY_SUPPORTED ON)
    endif()
endif()

set(MUSTACHE_HAVE_ARCHIVED_TEMPLATES OFF)
if(NOT MUSTACHE_ENABLE_ARCHIVED_TEMPLATES STREQUAL "OFF")
    if(NOT MUSTACHE_ARCHIVE_PRIVATE_VISIBILITY_SUPPORTED)
        if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES STREQUAL "ON")
            message(FATAL_ERROR
                "archived-template support requires private C++ symbol visibility")
        endif()
    else()
        if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES STREQUAL "ON")
            find_package(ZLIB REQUIRED)
        else()
            find_package(ZLIB QUIET)
        endif()
        if(TARGET ZLIB::ZLIB)
            set(MUSTACHE_HAVE_ARCHIVED_TEMPLATES ON)
        endif()
    endif()
endif()
message(STATUS
    "Archived-template support: ${MUSTACHE_HAVE_ARCHIVED_TEMPLATES}")

if(MUSTACHE_ENABLE_CISTA_BENCHMARK AND NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB REQUIRED)
endif()

set(MUSTACHE_NEED_CISTA OFF)
if(MUSTACHE_HAVE_ARCHIVED_TEMPLATES OR MUSTACHE_ENABLE_CISTA_BENCHMARK)
    set(MUSTACHE_NEED_CISTA ON)
endif()

if(MUSTACHE_USE_SYSTEM_CISTA AND NOT MUSTACHE_NEED_CISTA)
    message(FATAL_ERROR
        "MUSTACHE_USE_SYSTEM_CISTA requires archived-template support "
        "or MUSTACHE_ENABLE_CISTA_BENCHMARK")
endif()

if(MUSTACHE_USE_SYSTEM_XXHASH AND NOT MUSTACHE_NEED_CISTA)
    message(FATAL_ERROR
        "MUSTACHE_USE_SYSTEM_XXHASH requires archived-template support "
        "or MUSTACHE_ENABLE_CISTA_BENCHMARK")
endif()

if(MUSTACHE_ENABLE_CLANG_TIDY)
    find_program(MUSTACHE_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set(MUSTACHE_CLANG_TIDY_COMMAND "${MUSTACHE_CLANG_TIDY_EXECUTABLE}")
endif()

if(NOT MSVC AND MUSTACHE_ENABLE_HARDENING)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS
        "-O2 -Werror -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3")
    check_cxx_source_compiles([=[
        #if !defined(__OPTIMIZE__) || __OPTIMIZE__ <= 0
        #error _FORTIFY_SOURCE requires optimization
        #endif
        #if !defined(_FORTIFY_SOURCE) || _FORTIFY_SOURCE != 3
        #error _FORTIFY_SOURCE=3 was not selected
        #endif
        #include <cstring>

        int main()
        {
            char source[] = "abc";
            char destination[sizeof(source)];
            std::memcpy(destination, source, sizeof(source));
            return destination[0] == 'a' ? 0 : 1;
        }
    ]=] MUSTACHE_FORTIFY_SOURCE_3_SUPPORTED)
    cmake_pop_check_state()
endif()

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

set(MUSTACHE_HAVE_LIBJSON OFF)
if(NOT MUSTACHE_ENABLE_JSON STREQUAL "OFF")
    find_package(nlohmann_json 3.10.5 CONFIG QUIET)
    if(TARGET nlohmann_json::nlohmann_json)
        set(MUSTACHE_HAVE_LIBJSON ON)
        set(MUSTACHE_JSON_TARGET nlohmann_json::nlohmann_json)
    elseif(MUSTACHE_ENABLE_JSON STREQUAL "ON")
        message(FATAL_ERROR
            "MUSTACHE_ENABLE_JSON=ON requires nlohmann_json >= 3.10.5")
    endif()
endif()
message(STATUS "JSON support: ${MUSTACHE_HAVE_LIBJSON}")

if(MUSTACHE_NEED_CISTA)
    if(MUSTACHE_USE_SYSTEM_XXHASH)
        find_package(xxHash 0.8 CONFIG REQUIRED)
        set(MUSTACHE_XXHASH_TARGET xxHash::xxhash)
    else()
        set(MUSTACHE_VENDORED_XXHASH_HEADER
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/xxhash/xxhash.h")
        if(NOT EXISTS "${MUSTACHE_VENDORED_XXHASH_HEADER}")
            message(FATAL_ERROR
                "Bundled xxHash header is missing: ${MUSTACHE_VENDORED_XXHASH_HEADER}")
        endif()
        add_library(mustache_xxhash_headers INTERFACE)
        target_include_directories(mustache_xxhash_headers SYSTEM INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/xxhash")
        # Keep one private implementation in the archive translation unit.
        # XXH_INLINE_ALL makes Cista 0.16 runtime type hashes unstable under
        # optimized GCC builds, while the implementation mode is deterministic.
        target_compile_definitions(mustache_xxhash_headers INTERFACE
            XXH_IMPLEMENTATION=1
            XXH_STATIC_LINKING_ONLY=1
            XXH_NAMESPACE=MUSTACHE_CISTA_XXH_)
        set(MUSTACHE_XXHASH_TARGET mustache_xxhash_headers)
    endif()

    if(MUSTACHE_USE_SYSTEM_CISTA)
        # nixpkgs packages Cista 0.16 with CMake metadata reporting version
        # 0.7, so requesting the package version would reject the available
        # dependency.
        find_package(cista CONFIG REQUIRED)
        set(MUSTACHE_CISTA_TARGET cista::cista)
    else()
        set(MUSTACHE_VENDORED_CISTA_HEADER
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cista/cista.h")
        if(NOT EXISTS "${MUSTACHE_VENDORED_CISTA_HEADER}")
            message(FATAL_ERROR
                "Bundled Cista header is missing: ${MUSTACHE_VENDORED_CISTA_HEADER}")
        endif()
        add_library(mustache_cista_headers INTERFACE)
        target_include_directories(mustache_cista_headers SYSTEM INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cista")
        target_compile_features(mustache_cista_headers INTERFACE cxx_std_17)
        set(MUSTACHE_CISTA_TARGET mustache_cista_headers)
    endif()
endif()

set(MUSTACHE_HAVE_LIBYAML OFF)
if(NOT MUSTACHE_ENABLE_YAML STREQUAL "OFF")
    if(MSVC)
        find_package(yaml CONFIG QUIET)
        if(TARGET yaml)
            set(MUSTACHE_YAML_TARGET yaml)
            set(MUSTACHE_HAVE_LIBYAML ON)
        elseif(TARGET yaml::yaml)
            set(MUSTACHE_YAML_TARGET yaml::yaml)
            set(MUSTACHE_HAVE_LIBYAML ON)
        endif()
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(YAML QUIET IMPORTED_TARGET yaml-0.1)
            if(TARGET PkgConfig::YAML)
                set(MUSTACHE_YAML_TARGET PkgConfig::YAML)
                set(MUSTACHE_HAVE_LIBYAML ON)
            endif()
        endif()
    endif()
    if(MUSTACHE_ENABLE_YAML STREQUAL "ON" AND NOT MUSTACHE_HAVE_LIBYAML)
        message(FATAL_ERROR
            "MUSTACHE_ENABLE_YAML=ON requires libyaml")
    endif()
endif()
message(STATUS "YAML support: ${MUSTACHE_HAVE_LIBYAML}")

if(MUSTACHE_HAVE_LIBYAML)
    set(MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE yaml-0.1)
else()
    set(MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE "")
endif()
if(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
    string(APPEND MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE " zlib")
    if(MUSTACHE_USE_SYSTEM_XXHASH)
        string(APPEND MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE " libxxhash")
    endif()
    string(STRIP "${MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE}"
        MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE)
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
        if(MUSTACHE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(MUSTACHE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(MUSTACHE_ENABLE_ASSERTIONS)
            target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS=1)
        endif()
        if(MUSTACHE_ENABLE_HARDENING)
            target_compile_options(${target} PRIVATE -fstack-protector-strong)
            if(MUSTACHE_FORTIFY_SOURCE_3_SUPPORTED)
                target_compile_options(${target} PRIVATE
                    $<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE>
                    $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>)
            endif()
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

function(mustache_enable_clang_tidy target)
    if(MUSTACHE_ENABLE_CLANG_TIDY)
        set_property(TARGET ${target} PROPERTY CXX_CLANG_TIDY
            "${MUSTACHE_CLANG_TIDY_COMMAND}")
    endif()
endfunction()

add_subdirectory(src)
if(MUSTACHE_NEED_CISTA)
    add_subdirectory(benchmarks)
else()
    add_subdirectory(benchmarks EXCLUDE_FROM_ALL)
endif()

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
