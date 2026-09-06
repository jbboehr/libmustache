cmake_minimum_required(VERSION 3.18)
project(mustache_subproject_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Keep the shared library beside the consumers on DLL platforms.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory("../.." libmustache)

if(MUSTACHE_BUILD_CLI OR MUSTACHE_ENABLE_TESTS)
    message(FATAL_ERROR
        "libmustache developer tools must default to off as a subproject")
endif()

if(TARGET mustachec OR TARGET test_spec OR TARGET test_utils OR TARGET test_misc)
    message(FATAL_ERROR
        "libmustache created developer targets while embedded as a subproject")
endif()

# Consumer headers with common names must not replace libmustache's siblings.
set(consumer_headers "${CMAKE_CURRENT_BINARY_DIR}/consumer-headers")
set(header_checks)
foreach(header IN ITEMS archived_template.hpp compiled_template.hpp data.hpp
        exception.hpp lambda.hpp mustache.hpp mustache_export.hpp node.hpp
        renderer.hpp stack.hpp tokenizer.hpp utils.hpp mustache_config.h)
    file(WRITE "${consumer_headers}/${header}"
        "#error Consumer header ${header} was selected instead of the library header\n")
    set(check "${CMAKE_CURRENT_BINARY_DIR}/check-${header}.cpp")
    file(WRITE "${check}" "#include <mustache/${header}>\n")
    list(APPEND header_checks "${check}")
endforeach()

add_library(mustache_public_header_checks OBJECT ${header_checks})
target_include_directories(mustache_public_header_checks BEFORE PRIVATE
    "${consumer_headers}")
target_link_libraries(mustache_public_header_checks PRIVATE mustache::mustache)

enable_testing()
foreach(linkage IN ITEMS shared static)
    if(linkage STREQUAL "shared")
        set(consumer mustache_subproject_consumer)
        set(library mustache::mustache)
    else()
        set(consumer mustache_subproject_consumer_static)
        set(library mustache::mustache_static)
    endif()
    # Use exactly the same API exercise as the installed-package consumer.
    add_executable(${consumer} "../cmake-consumer/main.cpp")
    target_compile_definitions(${consumer} PRIVATE
        MUSTACHE_EXPECTED_VERSION=MUSTACHE_PACKAGE_VERSION)
    if(linkage STREQUAL "static")
        target_compile_definitions(${consumer} PRIVATE MUSTACHE_EXPECT_STATIC_DEFINE)
    endif()
    target_include_directories(${consumer} BEFORE PRIVATE "${consumer_headers}")
    target_link_libraries(${consumer} PRIVATE ${library})
    add_test(NAME ${linkage} COMMAND ${consumer})
endforeach()
