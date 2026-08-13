add_executable(test_spec
    test_spec.cpp
    test_spec.hpp
    spec_expectations.hpp
    fixtures/lambdas.cpp
    fixtures/lambdas.hpp
)
target_link_libraries(test_spec PRIVATE mustache_static ${MUSTACHE_YAML_TARGET})
mustache_apply_build_options(test_spec)

add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils PRIVATE mustache_static)
mustache_apply_build_options(test_utils)

add_executable(test_misc test_misc.cpp)
target_link_libraries(test_misc PRIVATE mustache_static)
mustache_apply_build_options(test_misc)

add_executable(test_data_behavior test_data_behavior.cpp)
target_link_libraries(test_data_behavior PRIVATE mustache_static)
mustache_apply_build_options(test_data_behavior)

add_executable(test_node_serialization test_node_serialization.cpp)
target_link_libraries(test_node_serialization PRIVATE mustache_static)
mustache_apply_build_options(test_node_serialization)

add_test(NAME spec COMMAND test_spec "${MUSTACHE_SPEC_DIR}")
add_test(NAME utils COMMAND test_utils)
add_test(NAME misc COMMAND test_misc)
add_test(NAME data_behavior COMMAND test_data_behavior)
add_test(NAME node_serialization COMMAND test_node_serialization)
