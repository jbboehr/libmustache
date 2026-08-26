cmake_minimum_required(VERSION 3.18)
project(mustache_find_quiet LANGUAGES CXX)

set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)

find_package(mustache CONFIG REQUIRED COMPONENTS shared)
if(NOT TARGET mustache::mustache)
    message(FATAL_ERROR
        "The shared component unnecessarily required build dependencies")
endif()

find_package(mustache QUIET CONFIG COMPONENTS static)

if(MUSTACHE_EXPECT_STATIC_WITHOUT_DEPENDENCIES)
    if(NOT mustache_FOUND OR NOT TARGET mustache::mustache_static)
        message(FATAL_ERROR
            "The dependency-free static component was not found")
    endif()
elseif(mustache_FOUND)
    message(FATAL_ERROR
        "The static component was found without its required dependencies")
endif()
