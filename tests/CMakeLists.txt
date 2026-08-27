if(MUSTACHE_HAVE_LIBYAML)
    add_executable(test_spec
        test_spec.cpp
        test_spec.hpp
        spec_expectations.hpp
        fixtures/lambdas.cpp
        fixtures/lambdas.hpp
    )
    target_link_libraries(test_spec PRIVATE mustache_static ${MUSTACHE_YAML_TARGET})
    mustache_apply_build_options(test_spec)
endif()

add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils PRIVATE mustache_static)
mustache_apply_build_options(test_utils)

add_executable(test_misc test_misc.cpp)
target_link_libraries(test_misc PRIVATE mustache_static)
mustache_apply_build_options(test_misc)

add_executable(test_data_behavior test_data_behavior.cpp)
target_link_libraries(test_data_behavior PRIVATE mustache_static)
mustache_apply_build_options(test_data_behavior)

if(MUSTACHE_HAVE_LIBJSON)
    add_executable(test_json_numeric_locale test_json_numeric_locale.cpp)
    target_link_libraries(test_json_numeric_locale PRIVATE mustache_static)
    mustache_apply_build_options(test_json_numeric_locale)
endif()

add_executable(test_node_serialization test_node_serialization.cpp)
target_link_libraries(test_node_serialization PRIVATE mustache_static)
mustache_apply_build_options(test_node_serialization)

add_executable(test_node_template test_node_template.cpp)
target_link_libraries(test_node_template PRIVATE mustache_static)
mustache_apply_build_options(test_node_template)

add_executable(test_ownership test_ownership.cpp)
target_link_libraries(test_ownership PRIVATE mustache_static)
mustache_apply_build_options(test_ownership)

add_executable(test_length_aware_api test_length_aware_api.cpp)
target_link_libraries(test_length_aware_api PRIVATE mustache_static)
mustache_apply_build_options(test_length_aware_api)

add_executable(test_tokenizer_safety test_tokenizer_safety.cpp)
target_link_libraries(test_tokenizer_safety PRIVATE mustache_static)
mustache_apply_build_options(test_tokenizer_safety)

add_executable(test_compiled_template test_compiled_template.cpp)
target_link_libraries(test_compiled_template PRIVATE mustache_static)
mustache_apply_build_options(test_compiled_template)

add_executable(test_renderer_safety test_renderer_safety.cpp)
target_link_libraries(test_renderer_safety PRIVATE mustache_static)
mustache_apply_build_options(test_renderer_safety)

add_executable(test_allocation_failure test_allocation_failure.cpp)
target_link_libraries(test_allocation_failure PRIVATE mustache_static)
mustache_apply_build_options(test_allocation_failure)
if(NOT MSVC)
    # Replacement new/delete use malloc/free; GCC -Werror treats that pairing
    # as mismatched-new-delete after inlining into std::make_unique.
    target_compile_options(test_allocation_failure PRIVATE
        -Wno-mismatched-new-delete)
endif()

if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES OR MUSTACHE_ENABLE_CISTA_BENCHMARK)
    add_executable(test_cista_archive
        "${CMAKE_CURRENT_SOURCE_DIR}/../benchmarks/test-cista-archive.cpp")
    target_link_libraries(test_cista_archive PRIVATE
        mustache_cista_archive_experiment)
    mustache_apply_build_options(test_cista_archive)
endif()

if(MUSTACHE_BUILD_CLI)
    add_executable(test_cli
        test_cli.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../bin/cli.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../bin/cli.hpp"
    )
    target_include_directories(test_cli PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../bin")
    target_link_libraries(test_cli PRIVATE mustache_static)
    mustache_apply_build_options(test_cli)
endif()

if(MUSTACHE_HAVE_LIBYAML)
    add_test(NAME spec COMMAND test_spec "${MUSTACHE_SPEC_DIR}")
endif()
if(WIN32 AND MUSTACHE_HAVE_LIBYAML)
    add_test(NAME spec_crlf
        COMMAND "${CMAKE_COMMAND}"
            "-DTEST_SPEC_EXECUTABLE=$<TARGET_FILE:test_spec>"
            "-DMUSTACHE_SPEC_DIR=${MUSTACHE_SPEC_DIR}"
            "-DCRLF_SPEC_DIR=${CMAKE_CURRENT_BINARY_DIR}/spec-crlf"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_spec_crlf.cmake")
endif()
if(MSVC)
    get_filename_component(MUSTACHE_MSVC_TOOL_DIR
        "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if(CMAKE_DUMPBIN AND EXISTS "${CMAKE_DUMPBIN}")
        set(MUSTACHE_EXPORT_INSPECTOR "${CMAKE_DUMPBIN}")
    else()
        find_program(MUSTACHE_EXPORT_INSPECTOR
            NAMES dumpbin dumpbin.exe llvm-readobj llvm-readobj.exe
            HINTS "${MUSTACHE_MSVC_TOOL_DIR}")
    endif()
    if(NOT MUSTACHE_EXPORT_INSPECTOR)
        message(FATAL_ERROR
            "dumpbin or llvm-readobj is required to audit the Windows DLL export boundary")
    endif()
    add_test(NAME windows_exports
        COMMAND "${CMAKE_COMMAND}"
            "-DEXPORT_INSPECTOR=${MUSTACHE_EXPORT_INSPECTOR}"
            "-DMUSTACHE_DLL=$<TARGET_FILE:mustache>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_windows_exports.cmake")
endif()
add_test(NAME utils COMMAND test_utils)
add_test(NAME misc COMMAND test_misc)
add_test(NAME data_behavior COMMAND test_data_behavior)
if(MUSTACHE_HAVE_LIBJSON)
    add_test(NAME json_numeric_locale COMMAND test_json_numeric_locale)
    set_tests_properties(json_numeric_locale PROPERTIES SKIP_RETURN_CODE 77)
endif()
add_test(NAME node_serialization COMMAND test_node_serialization)
add_test(NAME node_template COMMAND test_node_template)
add_test(NAME ownership COMMAND test_ownership)
add_test(NAME length_aware_api COMMAND test_length_aware_api)
add_test(NAME tokenizer_safety COMMAND test_tokenizer_safety)
add_test(NAME compiled_template COMMAND test_compiled_template)
add_test(NAME renderer_safety COMMAND test_renderer_safety)
add_test(NAME allocation_failure COMMAND test_allocation_failure)
if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES OR MUSTACHE_ENABLE_CISTA_BENCHMARK)
    add_test(NAME cista_archive COMMAND test_cista_archive)
endif()
if(MUSTACHE_BUILD_CLI)
    add_test(NAME cli COMMAND test_cli)
endif()

if(MUSTACHE_ENABLE_FUZZING)
    function(mustache_configure_fuzz_target target)
        target_link_libraries(${target} PRIVATE mustache_static)
        mustache_apply_build_options(${target})
        target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
        target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    endfunction()

    add_executable(fuzz_node_unserialize fuzz_node_unserialize.cpp)
    mustache_configure_fuzz_target(fuzz_node_unserialize)

    add_executable(fuzz_tokenizer fuzz_tokenizer.cpp)
    mustache_configure_fuzz_target(fuzz_tokenizer)

    if(MUSTACHE_HAVE_LIBJSON OR MUSTACHE_HAVE_LIBYAML)
        add_executable(fuzz_data_parser fuzz_data_parser.cpp)
        mustache_configure_fuzz_target(fuzz_data_parser)
    endif()

    set(MUSTACHE_FUZZ_CORPUS_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/node_unserialize")
    set(MUSTACHE_FUZZ_CORPUS_BINARY
        "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus/node_unserialize")
    set(MUSTACHE_FUZZ_ARTIFACTS
        "${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts/node_unserialize")
    set(MUSTACHE_TOKENIZER_FUZZ_CORPUS_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/tokenizer")
    set(MUSTACHE_TOKENIZER_FUZZ_CORPUS_BINARY
        "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus/tokenizer")
    set(MUSTACHE_TOKENIZER_FUZZ_ARTIFACTS
        "${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts/tokenizer")
    file(MAKE_DIRECTORY
        "${MUSTACHE_FUZZ_CORPUS_BINARY}"
        "${MUSTACHE_FUZZ_ARTIFACTS}"
        "${MUSTACHE_TOKENIZER_FUZZ_CORPUS_BINARY}"
        "${MUSTACHE_TOKENIZER_FUZZ_ARTIFACTS}")
    if(MUSTACHE_HAVE_LIBJSON OR MUSTACHE_HAVE_LIBYAML)
        set(MUSTACHE_DATA_FUZZ_CORPUS_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/data_parser")
        set(MUSTACHE_DATA_FUZZ_CORPUS_BINARY
            "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus/data_parser")
        set(MUSTACHE_DATA_FUZZ_ARTIFACTS
            "${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts/data_parser")
        file(MAKE_DIRECTORY
            "${MUSTACHE_DATA_FUZZ_CORPUS_BINARY}"
            "${MUSTACHE_DATA_FUZZ_ARTIFACTS}")
    endif()
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

    file(COPY "${MUSTACHE_TOKENIZER_FUZZ_CORPUS_SOURCE}/"
        DESTINATION "${MUSTACHE_TOKENIZER_FUZZ_CORPUS_BINARY}")
    add_test(NAME fuzz_tokenizer_smoke
        COMMAND fuzz_tokenizer
            -runs=1000
            -max_len=4096
            -timeout=5
            "-dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/tokenizer.dict"
            "-artifact_prefix=${MUSTACHE_TOKENIZER_FUZZ_ARTIFACTS}/"
            "${MUSTACHE_TOKENIZER_FUZZ_CORPUS_BINARY}")
    set_tests_properties(fuzz_tokenizer_smoke PROPERTIES
        LABELS fuzz
        RUN_SERIAL TRUE
        TIMEOUT 30)

    if(MUSTACHE_HAVE_LIBJSON OR MUSTACHE_HAVE_LIBYAML)
        file(COPY "${MUSTACHE_DATA_FUZZ_CORPUS_SOURCE}/"
            DESTINATION "${MUSTACHE_DATA_FUZZ_CORPUS_BINARY}")
        add_test(NAME fuzz_data_parser_smoke
            COMMAND fuzz_data_parser
                -runs=1000
                -max_len=4096
                -timeout=5
                "-dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/data_parser.dict"
                "-artifact_prefix=${MUSTACHE_DATA_FUZZ_ARTIFACTS}/"
                "${MUSTACHE_DATA_FUZZ_CORPUS_BINARY}")
        set_tests_properties(fuzz_data_parser_smoke PROPERTIES
            LABELS fuzz
            RUN_SERIAL TRUE
            TIMEOUT 30)
    endif()
endif()
