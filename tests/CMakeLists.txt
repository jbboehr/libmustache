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

add_executable(test_ownership test_ownership.cpp)
target_link_libraries(test_ownership PRIVATE mustache_static)
mustache_apply_build_options(test_ownership)

add_executable(test_length_aware_api test_length_aware_api.cpp)
target_link_libraries(test_length_aware_api PRIVATE mustache_static)
mustache_apply_build_options(test_length_aware_api)

add_executable(test_tokenizer_safety test_tokenizer_safety.cpp)
target_link_libraries(test_tokenizer_safety PRIVATE mustache_static)
mustache_apply_build_options(test_tokenizer_safety)

add_test(NAME spec COMMAND test_spec "${MUSTACHE_SPEC_DIR}")
add_test(NAME utils COMMAND test_utils)
add_test(NAME misc COMMAND test_misc)
add_test(NAME data_behavior COMMAND test_data_behavior)
add_test(NAME node_serialization COMMAND test_node_serialization)
add_test(NAME ownership COMMAND test_ownership)
add_test(NAME length_aware_api COMMAND test_length_aware_api)
add_test(NAME tokenizer_safety COMMAND test_tokenizer_safety)

if(MUSTACHE_ENABLE_FUZZING)
    add_executable(fuzz_node_unserialize fuzz_node_unserialize.cpp)
    target_link_libraries(fuzz_node_unserialize PRIVATE mustache_static)
    mustache_apply_build_options(fuzz_node_unserialize)
    target_compile_options(fuzz_node_unserialize PRIVATE -fsanitize=fuzzer)
    target_link_options(fuzz_node_unserialize PRIVATE -fsanitize=fuzzer)

    set(MUSTACHE_FUZZ_CORPUS_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/node_unserialize")
    set(MUSTACHE_FUZZ_CORPUS_BINARY
        "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus/node_unserialize")
    set(MUSTACHE_FUZZ_ARTIFACTS
        "${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts/node_unserialize")
    file(MAKE_DIRECTORY
        "${MUSTACHE_FUZZ_CORPUS_BINARY}"
        "${MUSTACHE_FUZZ_ARTIFACTS}")
    file(COPY "${MUSTACHE_FUZZ_CORPUS_SOURCE}/"
        DESTINATION "${MUSTACHE_FUZZ_CORPUS_BINARY}")

    add_test(NAME fuzz_node_unserialize_smoke
        COMMAND fuzz_node_unserialize
            -runs=1000
            -max_len=4096
            -timeout=5
            "-dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/node_unserialize.dict"
            "-artifact_prefix=${MUSTACHE_FUZZ_ARTIFACTS}/"
            "${MUSTACHE_FUZZ_CORPUS_BINARY}")
    set_tests_properties(fuzz_node_unserialize_smoke PROPERTIES
        LABELS fuzz
        RUN_SERIAL TRUE
        TIMEOUT 30)
endif()
