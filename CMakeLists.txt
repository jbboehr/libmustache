cmake_minimum_required(VERSION 3.5)
project(mustache CXX)

set(MUSTACHE_VERSION_MAJOR "0")
set(MUSTACHE_VERSION_MINOR "4")
set(MUSTACHE_VERSION_PATCH "3")
set(MUSTACHE_PACKAGE_VERSION ${MUSTACHE_VERSION_MAJOR}.${MUSTACHE_VERSION_MINOR}.${MUSTACHE_VERSION_PATCH})

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include_directories(../output/include)

add_definitions(-DMUSTACHE_VERSION_MAJOR=${MUSTACHE_VERSION_MAJOR})
add_definitions(-DMUSTACHE_VERSION_MINOR=${MUSTACHE_VERSION_MINOR})
add_definitions(-DMUSTACHE_VERSION_PATCH=${MUSTACHE_VERSION_PATCH})

option(MUSTACHE_ENABLE_TESTS
        "Enable the compilation and running of unit tests" ON)

find_library(LIBJSON NAME json-c HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../output/lib ENV LIBJSON_PATH)

if(WIN32)
add_definitions(-DYAML_DECLARE_STATIC=1)
find_library(LIBYAML NAME yaml_static HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../output/lib ENV LIBYAML_PATH)
else()
find_library(LIBYAML NAME yaml HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../output/lib ENV LIBYAML_PATH)
endif(WIN32)

# generate config.h and mustache_config.h
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mustache_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/mustache_config.h)
add_definitions(-DHAVE_CONFIG_H)

add_subdirectory(src)
add_subdirectory(bin)

# Unit tests
if (MUSTACHE_ENABLE_TESTS)
    add_subdirectory(tests)
    enable_testing()
    add_test(NAME test_utils COMMAND tests/test_utils)
    add_test(NAME test_spec COMMAND tests/spec/test_spec ${CMAKE_CURRENT_SOURCE_DIR}/spec/specs)
endif()
