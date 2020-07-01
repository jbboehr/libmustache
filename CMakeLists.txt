cmake_minimum_required(VERSION 3.5)
project(mustache CXX)

set(MUSTACHE_VERSION_MAJOR "0")
set(MUSTACHE_VERSION_MINOR "5")
set(MUSTACHE_VERSION_PATCH "0")
set(MUSTACHE_PACKAGE_VERSION ${MUSTACHE_VERSION_MAJOR}.${MUSTACHE_VERSION_MINOR}.${MUSTACHE_VERSION_PATCH})

if(NOT MUSTACHE_SPEC_DIR)
    set(MUSTACHE_SPEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spec/specs")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
if (NOT MSVC)
    include(FindPkgConfig)
endif()

add_definitions(-DMUSTACHE_VERSION_MAJOR=${MUSTACHE_VERSION_MAJOR})
add_definitions(-DMUSTACHE_VERSION_MINOR=${MUSTACHE_VERSION_MINOR})
add_definitions(-DMUSTACHE_VERSION_PATCH=${MUSTACHE_VERSION_PATCH})
add_definitions(-DMUSTACHE_HAVE_LIBJSON=1)
add_definitions(-DMUSTACHE_HAVE_LIBYAML=1)

option(MUSTACHE_ENABLE_TESTS
        "Enable the compilation and running of unit tests" ON)

if(MSVC)
    find_path(GETOPT_INCLUDE_DIR getopt.h)
    find_library(GETOPT_LIBRARY NAMES getopt)
    include_directories(${GETOPT_INCLUDE_DIR})
    link_libraries(${GETOPT_LIBRARY})

    find_package(json-c CONFIG REQUIRED)

    find_path(JSON_INCLUDE_DIR json-c/json.h)
    find_library(JSON_LIBRARY json-c)
    include_directories(${JSON_INCLUDE_DIR} ${JSON_INCLUDE_DIR}/json-c)
    link_libraries(${JSON_LIBRARY})

    find_package(yaml CONFIG REQUIRED)

    find_path(YAML_INCLUDE_DIR yaml.h)
    find_library(YAML_LIBRARY yaml)
    include_directories(${YAML_INCLUDE_DIR})
    link_libraries(${YAML_LIBRARY})
else()
    pkg_check_modules(JSONC REQUIRED json-c)
    pkg_check_modules(YAML REQUIRED yaml-0.1)
    include_directories(${JSONC_INCLUDE_DIRS} ${YAML_INCLUDE_DIRS})
    link_libraries(${JSONC_LIBRARIES} ${YAML_LIBRARIES})
endif()

# generate mustache_config.h
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mustache_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/mustache_config.h)
add_definitions(-DHAVE_CONFIG_H)

add_subdirectory(src)
add_subdirectory(bin)

# Unit tests
if (MUSTACHE_ENABLE_TESTS)
    add_subdirectory(tests)
    enable_testing()
    add_test(NAME test_utils COMMAND tests/test_utils)
    add_test(NAME test_spec COMMAND tests/test_spec ${MUSTACHE_SPEC_DIR})
endif()
