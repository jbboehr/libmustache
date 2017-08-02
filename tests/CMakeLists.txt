
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

link_libraries(mustache_static)

add_executable(test_spec test_spec.cpp test_spec.hpp ./fixtures/lambdas.cpp ./fixtures/lambdas.hpp)
add_executable(test_utils test_utils.cpp)
