
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SOURCE_FILES
        compiler.cpp
        data.cpp
        lambda.cpp
        mustache.cpp
        node.cpp
        renderer.cpp
        stack.cpp
        tokenizer.cpp
        utils.cpp
        vm.cpp)

set(HEADERS
        compiler.hpp
        data.hpp
        exception.hpp
        lambda.hpp
        mustache.hpp
        ${CMAKE_CURRENT_BINARY_DIR}/mustache_config.h
        node.hpp
        renderer.hpp
        stack.hpp
        tokenizer.hpp
        utils.hpp
        vm.hpp)

link_libraries(${LIBYAML} ${LIBJSON})

add_library(mustache SHARED ${SOURCE_FILES} ${HEADERS})
add_library(mustache_static STATIC ${SOURCE_FILES} ${HEADERS})

install(TARGETS mustache mustache_static
  EXPORT mustache
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

install(FILES ${HEADERS} DESTINATION include/mustache)
install(EXPORT mustache DESTINATION cmake)
