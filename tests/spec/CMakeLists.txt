
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

link_libraries(${LIBYAML} ${LIBJSON} mustache)

add_executable(test_spec test_spec.cpp test_spec.hpp ../fixtures/lambdas.cpp ../fixtures/lambdas.hpp)
