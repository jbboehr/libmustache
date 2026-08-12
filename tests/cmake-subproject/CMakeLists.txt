cmake_minimum_required(VERSION 3.18)
project(mustache_subproject_consumer LANGUAGES CXX)

add_subdirectory("../.." libmustache)

if(MUSTACHE_BUILD_CLI OR MUSTACHE_ENABLE_TESTS)
    message(FATAL_ERROR
        "libmustache developer tools must default to off as a subproject")
endif()

if(TARGET mustachec OR TARGET test_spec OR TARGET test_utils OR TARGET test_misc)
    message(FATAL_ERROR
        "libmustache created developer targets while embedded as a subproject")
endif()

add_executable(mustache_subproject_consumer main.cpp)
target_link_libraries(mustache_subproject_consumer PRIVATE mustache::mustache)
