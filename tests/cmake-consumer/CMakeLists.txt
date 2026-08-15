cmake_minimum_required(VERSION 3.15)
project(mustache_consumer LANGUAGES CXX)

# Start below libmustache's requirement to verify that the imported target
# raises consumers to the public C++17 baseline.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MUSTACHE_CONSUMER_LINKAGE shared CACHE STRING
    "Installed mustache component to consume (shared or static)")
set_property(CACHE MUSTACHE_CONSUMER_LINKAGE PROPERTY STRINGS shared static)

if(MUSTACHE_CONSUMER_LINKAGE STREQUAL "shared")
    set(MUSTACHE_CONSUMER_TARGET mustache::mustache)
elseif(MUSTACHE_CONSUMER_LINKAGE STREQUAL "static")
    set(MUSTACHE_CONSUMER_TARGET mustache::mustache_static)
else()
    message(FATAL_ERROR
        "MUSTACHE_CONSUMER_LINKAGE must be either shared or static")
endif()

find_package(mustache 0.6 CONFIG REQUIRED
    COMPONENTS ${MUSTACHE_CONSUMER_LINKAGE})

add_executable(mustache_consumer main.cpp)
target_compile_definitions(mustache_consumer PRIVATE
    MUSTACHE_EXPECTED_VERSION="${mustache_VERSION}")
if(MUSTACHE_CONSUMER_LINKAGE STREQUAL "static")
    target_compile_definitions(mustache_consumer PRIVATE
        MUSTACHE_EXPECT_STATIC_DEFINE)
endif()
target_link_libraries(mustache_consumer PRIVATE ${MUSTACHE_CONSUMER_TARGET})
