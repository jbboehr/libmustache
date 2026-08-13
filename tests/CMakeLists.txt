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

add_test(NAME spec COMMAND test_spec "${MUSTACHE_SPEC_DIR}")
add_test(NAME utils COMMAND test_utils)
add_test(NAME misc COMMAND test_misc)
