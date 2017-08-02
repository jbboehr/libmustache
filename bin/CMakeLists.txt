
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

set(SOURCE_FILES
        bin/mustache.cpp)

add_executable(mustachec mustache.cpp)
target_link_libraries(mustachec mustache_static)

install(TARGETS mustachec
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
