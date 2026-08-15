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
if(NOT MSVC)
    list(APPEND MUSTACHE_SOURCES json_parser.cpp)
endif()
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

if(MSVC)
    # CMake's legacy export-all mode scans this DLL target's object files. Keep
    # the header-only parser in a private archive so its template machinery is
    # linked into the DLL without becoming part of the Windows export table.
    add_library(mustache_json_adapter STATIC json_parser.cpp)
    target_compile_features(mustache_json_adapter PRIVATE
        cxx_std_${MUSTACHE_CXX_STANDARD})
    set_target_properties(mustache_json_adapter PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(mustache_json_adapter PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(mustache_json_adapter PRIVATE
        ${MUSTACHE_JSON_TARGET})
    mustache_apply_build_options(mustache_json_adapter)
endif()

add_library(mustache SHARED ${MUSTACHE_SOURCES} ${MUSTACHE_PUBLIC_HEADERS})
add_library(mustache::mustache ALIAS mustache)
mustache_configure_library(mustache)
if(MSVC)
    target_link_libraries(mustache PRIVATE mustache_json_adapter)
endif()
set_target_properties(mustache PROPERTIES
    VERSION "${MUSTACHE_ABI_VERSION}.0.0"
    SOVERSION "${MUSTACHE_ABI_VERSION}"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_library(mustache_static STATIC ${MUSTACHE_SOURCES} ${MUSTACHE_PUBLIC_HEADERS})
add_library(mustache::mustache_static ALIAS mustache_static)
if(MSVC)
    target_sources(mustache_static PRIVATE json_parser.cpp)
endif()
mustache_configure_library(mustache_static)
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
