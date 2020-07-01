
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SOURCE_FILES
        data.cpp
        lambda.cpp
        mustache.cpp
        node.cpp
        renderer.cpp
        stack.cpp
        tokenizer.cpp
        utils.cpp)

set(HEADERS
        data.hpp
        exception.hpp
        lambda.hpp
        mustache.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/mustache_config.h
        node.hpp
        renderer.hpp
        stack.hpp
        tokenizer.hpp
        utils.hpp)

add_library(mustache SHARED ${SOURCE_FILES} ${HEADERS})
add_library(mustache_static STATIC ${SOURCE_FILES} ${HEADERS})

install(TARGETS mustache mustache_static
  EXPORT mustache
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mustache)
install(EXPORT mustache DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cmake)
