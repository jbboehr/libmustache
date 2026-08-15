set(MUSTACHE_SOURCES
    compiled_template.cpp
    data.cpp
    json_parser.cpp
    lambda.cpp
    mustache.cpp
    node.cpp
    renderer.cpp
    stack.cpp
    tokenizer.cpp
    utils.cpp
)
set_property(SOURCE json_parser.cpp APPEND PROPERTY COMPILE_OPTIONS
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fvisibility=hidden>"
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-fvisibility-inlines-hidden>"
)

set(MUSTACHE_PUBLIC_HEADERS
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
            $<BUILD_INTERFACE:${MUSTACHE_JSON_TARGET}>
            ${MUSTACHE_YAML_TARGET}
    )
    mustache_apply_build_options(${target})
endfunction()

add_library(mustache SHARED ${MUSTACHE_SOURCES} ${MUSTACHE_PUBLIC_HEADERS})
add_library(mustache::mustache ALIAS mustache)
mustache_configure_library(mustache)
target_compile_definitions(mustache PRIVATE MUSTACHE_BUILDING_LIBRARY)
set_target_properties(mustache PROPERTIES
    VERSION "${MUSTACHE_ABI_VERSION}.0.0"
    SOVERSION "${MUSTACHE_ABI_VERSION}"
)

add_library(mustache_static STATIC ${MUSTACHE_SOURCES} ${MUSTACHE_PUBLIC_HEADERS})
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
