set(MUSTACHE_SOURCES
    compiled_template.cpp
    data.cpp
    lambda.cpp
    mustache.cpp
    node.cpp
    renderer.cpp
    stack.cpp
    tokenizer.cpp
    utils.cpp
)
if(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
    set(MUSTACHE_CISTA_ARCHIVE_SOURCE
        "${PROJECT_SOURCE_DIR}/benchmarks/cista-archive.cpp")
    set(MUSTACHE_CISTA_XXH3_SOURCE
        "${PROJECT_SOURCE_DIR}/benchmarks/cista-xxh3.cpp")
    list(APPEND MUSTACHE_SOURCES
        "${MUSTACHE_CISTA_ARCHIVE_SOURCE}"
        "${MUSTACHE_CISTA_XXH3_SOURCE}")
    if(NOT MSVC)
        set_property(SOURCE
            "${MUSTACHE_CISTA_ARCHIVE_SOURCE}"
            "${MUSTACHE_CISTA_XXH3_SOURCE}"
            APPEND PROPERTY COMPILE_OPTIONS
                -fvisibility=hidden
                -fvisibility-inlines-hidden
        )
    endif()
    set_property(SOURCE "${MUSTACHE_CISTA_ARCHIVE_SOURCE}"
        APPEND PROPERTY COMPILE_DEFINITIONS
            CISTA_XXH3=1
            MUSTACHE_CISTA_RUNTIME_VERSION_XXH3=1)
    if(MSVC)
        set_property(SOURCE "${MUSTACHE_CISTA_ARCHIVE_SOURCE}"
            APPEND PROPERTY COMPILE_OPTIONS /UCISTA_FMT)
    else()
        set_property(SOURCE "${MUSTACHE_CISTA_ARCHIVE_SOURCE}"
            APPEND PROPERTY COMPILE_OPTIONS -UCISTA_FMT)
    endif()
    if(NOT MUSTACHE_USE_SYSTEM_CISTA)
        set_property(SOURCE "${MUSTACHE_CISTA_ARCHIVE_SOURCE}"
            APPEND PROPERTY COMPILE_DEFINITIONS
                MUSTACHE_USE_VENDORED_CISTA=1)
    endif()
    if(NOT MUSTACHE_USE_SYSTEM_XXHASH)
        set_property(SOURCE "${MUSTACHE_CISTA_XXH3_SOURCE}"
            APPEND PROPERTY COMPILE_DEFINITIONS
                XXH_IMPLEMENTATION=1
                XXH_STATIC_LINKING_ONLY=1
                XXH_NAMESPACE=MUSTACHE_CISTA_XXH_)
    endif()
endif()
set(MUSTACHE_PRIVATE_HEADERS
    render_engine.hpp
)
if(MUSTACHE_HAVE_LIBJSON)
    list(APPEND MUSTACHE_SOURCES json_parser.cpp)
    set_property(SOURCE json_parser.cpp APPEND PROPERTY COMPILE_OPTIONS
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fvisibility=hidden>"
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fvisibility-inlines-hidden>"
    )
endif()

set(MUSTACHE_PRIVATE_DEPENDENCIES)
if(MUSTACHE_HAVE_LIBJSON)
    list(APPEND MUSTACHE_PRIVATE_DEPENDENCIES
        "$<BUILD_INTERFACE:${MUSTACHE_JSON_TARGET}>")
endif()
if(MUSTACHE_HAVE_LIBYAML)
    list(APPEND MUSTACHE_PRIVATE_DEPENDENCIES
        ${MUSTACHE_YAML_TARGET})
endif()
if(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
    list(APPEND MUSTACHE_PRIVATE_DEPENDENCIES ZLIB::ZLIB)
    if(MUSTACHE_USE_SYSTEM_XXHASH)
        list(APPEND MUSTACHE_PRIVATE_DEPENDENCIES ${MUSTACHE_XXHASH_TARGET})
    endif()
endif()

set(MUSTACHE_PUBLIC_HEADERS
    archived_template.hpp
    compiled_template.hpp
    data.hpp
    exception.hpp
    lambda.hpp
    mustache.hpp
    mustache_export.hpp
    node.hpp
    renderer.hpp
    stack.hpp
    tokenizer.hpp
    utils.hpp
)

function(mustache_configure_library target)
    target_compile_features(${target} PUBLIC cxx_std_${MUSTACHE_CXX_STANDARD})
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(${target}
        PRIVATE
            ${MUSTACHE_PRIVATE_DEPENDENCIES}
    )
    if(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
        target_include_directories(${target} BEFORE PRIVATE
            "${PROJECT_SOURCE_DIR}/benchmarks/cista-xxh3")
        if(MUSTACHE_USE_SYSTEM_CISTA)
            target_include_directories(${target} SYSTEM PRIVATE
                "$<TARGET_PROPERTY:${MUSTACHE_CISTA_TARGET},INTERFACE_INCLUDE_DIRECTORIES>")
        else()
            target_include_directories(${target} SYSTEM PRIVATE
                "${PROJECT_SOURCE_DIR}/vendor/cista")
        endif()
        if(NOT MUSTACHE_USE_SYSTEM_XXHASH)
            target_include_directories(${target} SYSTEM PRIVATE
                "${PROJECT_SOURCE_DIR}/vendor/xxhash")
        endif()
    endif()
    mustache_apply_build_options(${target})
endfunction()

add_library(mustache SHARED ${MUSTACHE_SOURCES} ${MUSTACHE_PRIVATE_HEADERS} ${MUSTACHE_PUBLIC_HEADERS})
add_library(mustache::mustache ALIAS mustache)
mustache_configure_library(mustache)
target_compile_definitions(mustache PRIVATE MUSTACHE_BUILDING_LIBRARY)
set_target_properties(mustache PROPERTIES
    VERSION "${MUSTACHE_ABI_VERSION}.0.0"
    SOVERSION "${MUSTACHE_ABI_VERSION}"
)
if(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
    # Shared consumers do not import private archive dependencies. Preserve
    # their non-system link directories in the installed library instead.
    set_property(TARGET mustache PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

add_library(mustache_static STATIC ${MUSTACHE_SOURCES} ${MUSTACHE_PRIVATE_HEADERS} ${MUSTACHE_PUBLIC_HEADERS})
add_library(mustache::mustache_static ALIAS mustache_static)
mustache_configure_library(mustache_static)
mustache_enable_clang_tidy(mustache_static)
target_compile_definitions(mustache_static PUBLIC MUSTACHE_STATIC_DEFINE)
if(MUSTACHE_ENABLE_FUZZING)
    # The fuzz executable links this archive, so its implementation needs
    # SanitizerCoverage instrumentation as well as the libFuzzer harness.
    target_compile_options(mustache_static PRIVATE -fsanitize=fuzzer-no-link)
endif()
if(NOT WIN32)
    set_target_properties(mustache_static PROPERTIES OUTPUT_NAME mustache)
endif()

install(TARGETS mustache
    EXPORT mustacheSharedTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(TARGETS mustache_static
    EXPORT mustacheStaticTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(FILES
    ${MUSTACHE_PUBLIC_HEADERS}
    "${CMAKE_CURRENT_BINARY_DIR}/mustache_config.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mustache"
)

install(EXPORT mustacheSharedTargets
    FILE mustacheSharedTargets.cmake
    NAMESPACE mustache::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mustache"
)

install(EXPORT mustacheStaticTargets
    FILE mustacheStaticTargets.cmake
    NAMESPACE mustache::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mustache"
)
