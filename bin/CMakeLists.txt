
set(SOURCE_FILES
        bin/mustache.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

link_libraries(mustache)

add_executable(mustachec mustache.cpp)

install(TARGETS mustachec
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
