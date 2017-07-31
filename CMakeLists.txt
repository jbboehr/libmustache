cmake_minimum_required(VERSION 3.5)
project(mustache CXX)

set(MUSTACHE_VERSION_MAJOR 0)
set(MUSTACHE_VERSION_MINOR 6)
set(MUSTACHE_VERSION_PATCH 4)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_definitions(-DMUSTACHE_VERSION_MAJOR=${MUSTACHE_VERSION_MAJOR})
add_definitions(-DMUSTACHE_VERSION_MINOR=${MUSTACHE_VERSION_MINOR})
add_definitions(-DMUSTACHE_VERSION_PATCH=${MUSTACHE_VERSION_PATCH})

option(MUSTACHE_ENABLE_TESTS
        "Enable the compilation and running of unit tests" ON)

find_library(LIBJSON json-c)

find_library(LIBYAML yaml)

add_subdirectory(src)
add_subdirectory(bin)

# Unit tests
if (MUSTACHE_ENABLE_TESTS)
    add_subdirectory(test)
    enable_testing()
    add_test(NAME test_utils COMMAND test/test_utils)
    add_test(NAME test_spec COMMAND test/spec/test_spec ${CMAKE_CURRENT_SOURCE_DIR}/spec/specs)
endif()
