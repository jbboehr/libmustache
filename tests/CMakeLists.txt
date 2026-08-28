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
    if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES OR
            MUSTACHE_CISTA_BENCHMARK_BUILTIN_XXH3)
        target_compile_definitions(test_cista_archive PRIVATE
            MUSTACHE_CISTA_RUNTIME_VERSION_XXH3=1)
    endif()
    mustache_apply_build_options(test_cista_archive)
endif()
if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES)
    add_executable(test_archived_template test_archived_template.cpp)
    target_link_libraries(test_archived_template PRIVATE mustache_static)
    mustache_apply_build_options(test_archived_template)
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
    set_tests_properties(cista_archive PROPERTIES
        ENVIRONMENT "top_srcdir=${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()
if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES)
    add_test(NAME archived_template COMMAND test_archived_template)

    # Exercise the installed shared library only when the configuration can
    # safely be redirected beneath the test build and the selected zlib is a
    # non-system shared dependency. This keeps the test sensitive to the
    # installed library's runtime search path.
    set(MUSTACHE_SHARED_INSTALL_TEST_SAFE TRUE)
    if(CMAKE_SKIP_RPATH OR CMAKE_SKIP_INSTALL_RPATH OR
            IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}" OR
            IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}" OR
            (MUSTACHE_BUILD_CLI AND IS_ABSOLUTE "${CMAKE_INSTALL_BINDIR}"))
        set(MUSTACHE_SHARED_INSTALL_TEST_SAFE FALSE)
    endif()

    set(MUSTACHE_TEST_ZLIB_LIBRARY "")
    get_target_property(MUSTACHE_TEST_ZLIB_LIBRARY
        ZLIB::ZLIB IMPORTED_LOCATION)
    if(NOT MUSTACHE_TEST_ZLIB_LIBRARY OR
            MUSTACHE_TEST_ZLIB_LIBRARY MATCHES "-NOTFOUND$")
        get_target_property(MUSTACHE_TEST_ZLIB_CONFIGURATIONS
            ZLIB::ZLIB IMPORTED_CONFIGURATIONS)
        foreach(MUSTACHE_TEST_ZLIB_CONFIGURATION
                IN LISTS MUSTACHE_TEST_ZLIB_CONFIGURATIONS)
            string(TOUPPER "${MUSTACHE_TEST_ZLIB_CONFIGURATION}"
                MUSTACHE_TEST_ZLIB_CONFIGURATION)
            get_target_property(MUSTACHE_TEST_ZLIB_CANDIDATE
                ZLIB::ZLIB
                "IMPORTED_LOCATION_${MUSTACHE_TEST_ZLIB_CONFIGURATION}")
            if(MUSTACHE_TEST_ZLIB_CANDIDATE AND
                    NOT MUSTACHE_TEST_ZLIB_CANDIDATE MATCHES "-NOTFOUND$")
                set(MUSTACHE_TEST_ZLIB_LIBRARY
                    "${MUSTACHE_TEST_ZLIB_CANDIDATE}")
                break()
            endif()
        endforeach()
    endif()

    set(MUSTACHE_TEST_ZLIB_IS_NONDEFAULT FALSE)
    if(MUSTACHE_TEST_ZLIB_LIBRARY AND
            EXISTS "${MUSTACHE_TEST_ZLIB_LIBRARY}" AND
            NOT MUSTACHE_TEST_ZLIB_LIBRARY MATCHES
                "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
        get_filename_component(MUSTACHE_TEST_ZLIB_LIBRARY_DIRECTORY
            "${MUSTACHE_TEST_ZLIB_LIBRARY}" DIRECTORY)
        get_filename_component(MUSTACHE_TEST_ZLIB_LIBRARY_DIRECTORY
            "${MUSTACHE_TEST_ZLIB_LIBRARY_DIRECTORY}" REALPATH)
        get_filename_component(MUSTACHE_TEST_ZLIB_LIBRARY
            "${MUSTACHE_TEST_ZLIB_LIBRARY}" REALPATH)
        set(MUSTACHE_TEST_ZLIB_IS_NONDEFAULT TRUE)
        foreach(MUSTACHE_TEST_IMPLICIT_LINK_DIRECTORY
                IN LISTS CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
            get_filename_component(MUSTACHE_TEST_IMPLICIT_LINK_DIRECTORY
                "${MUSTACHE_TEST_IMPLICIT_LINK_DIRECTORY}" REALPATH)
            if(MUSTACHE_TEST_ZLIB_LIBRARY_DIRECTORY STREQUAL
                    MUSTACHE_TEST_IMPLICIT_LINK_DIRECTORY)
                set(MUSTACHE_TEST_ZLIB_IS_NONDEFAULT FALSE)
                break()
            endif()
        endforeach()
    endif()

    # --prefix can safely redirect only relative install destinations. Nix and
    # other split-output packagers verify their absolute destinations during
    # their own install checks.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
            MUSTACHE_SHARED_INSTALL_TEST_SAFE AND
            MUSTACHE_TEST_ZLIB_IS_NONDEFAULT)
        add_test(NAME installed_shared_archive_consumer
            COMMAND "${CMAKE_COMMAND}"
                "-DMUSTACHE_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
                "-DMUSTACHE_BUILD_CONFIG=$<CONFIG>"
                "-DMUSTACHE_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/cmake-consumer"
                "-DMUSTACHE_ENABLE_SANITIZERS=${MUSTACHE_ENABLE_SANITIZERS}"
                "-DMUSTACHE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
                "-DMUSTACHE_SHARED_LIBRARY_FILE_NAME=$<TARGET_FILE_NAME:mustache>"
                "-DMUSTACHE_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/installed-shared-archive-consumer"
                "-DMUSTACHE_ZLIB_LIBRARY=${MUSTACHE_TEST_ZLIB_LIBRARY}"
                "-DMUSTACHE_ZLIB_LIBRARY_DIRECTORY=${MUSTACHE_TEST_ZLIB_LIBRARY_DIRECTORY}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_shared_consumer.cmake")
        set_tests_properties(installed_shared_archive_consumer PROPERTIES
            RUN_SERIAL TRUE
            TIMEOUT 60)
    endif()
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

    if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES)
        add_executable(fuzz_cista_archive fuzz_cista_archive.cpp)
        mustache_configure_fuzz_target(fuzz_cista_archive)
        target_link_libraries(fuzz_cista_archive PRIVATE
            mustache_cista_archive_experiment)
    endif()

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
    if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES)
        set(MUSTACHE_CISTA_FUZZ_CORPUS_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/cista_archive")
        set(MUSTACHE_CISTA_FUZZ_SEED_CORPUS
            "${CMAKE_CURRENT_BINARY_DIR}/fuzz-seeds/cista_archive")
        set(MUSTACHE_CISTA_FUZZ_SCRATCH_PARENT
            "${CMAKE_CURRENT_BINARY_DIR}/fuzz-scratch")
        set(MUSTACHE_CISTA_FUZZ_SCRATCH_ROOT
            "${MUSTACHE_CISTA_FUZZ_SCRATCH_PARENT}/cista_archive")
        set(MUSTACHE_CISTA_FUZZ_ARTIFACTS
            "${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts/cista_archive")
    endif()
    file(MAKE_DIRECTORY
        "${MUSTACHE_FUZZ_CORPUS_BINARY}"
        "${MUSTACHE_FUZZ_ARTIFACTS}"
        "${MUSTACHE_TOKENIZER_FUZZ_CORPUS_BINARY}"
        "${MUSTACHE_TOKENIZER_FUZZ_ARTIFACTS}")
    if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES)
        file(MAKE_DIRECTORY
            "${MUSTACHE_CISTA_FUZZ_SEED_CORPUS}"
            "${MUSTACHE_CISTA_FUZZ_SCRATCH_PARENT}"
            "${MUSTACHE_CISTA_FUZZ_ARTIFACTS}")
    endif()
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

    if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES)
        file(REMOVE_RECURSE "${MUSTACHE_CISTA_FUZZ_SEED_CORPUS}")
        file(MAKE_DIRECTORY "${MUSTACHE_CISTA_FUZZ_SEED_CORPUS}")
        file(COPY "${MUSTACHE_CISTA_FUZZ_CORPUS_SOURCE}/"
            DESTINATION "${MUSTACHE_CISTA_FUZZ_SEED_CORPUS}")
        file(READ
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cista-archive-v1-x86_64-le-itanium.hex"
            MUSTACHE_CISTA_GOLDEN_HEX)
        file(WRITE "${MUSTACHE_CISTA_FUZZ_SEED_CORPUS}/golden-v1"
            "hex:${MUSTACHE_CISTA_GOLDEN_HEX}")
        file(WRITE "${MUSTACHE_CISTA_FUZZ_SEED_CORPUS}/integrity-repair-v1"
            "corrupt-integrity:${MUSTACHE_CISTA_GOLDEN_HEX}")
        add_test(NAME fuzz_cista_archive_smoke
            COMMAND "${CMAKE_COMMAND}"
                "-DFUZZ_EXECUTABLE=$<TARGET_FILE:fuzz_cista_archive>"
                "-DFUZZ_DICTIONARY=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/cista_archive.dict"
                "-DFUZZ_SEED_CORPUS=${MUSTACHE_CISTA_FUZZ_SEED_CORPUS}"
                "-DFUZZ_ALLOWED_SCRATCH_PARENT=${MUSTACHE_CISTA_FUZZ_SCRATCH_PARENT}"
                "-DFUZZ_SCRATCH_ROOT=${MUSTACHE_CISTA_FUZZ_SCRATCH_ROOT}"
                "-DFUZZ_ARTIFACT_DIRECTORY=${MUSTACHE_CISTA_FUZZ_ARTIFACTS}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_fuzzer_smoke.cmake")
        set_tests_properties(fuzz_cista_archive_smoke PROPERTIES
            LABELS fuzz
            RUN_SERIAL TRUE
            TIMEOUT 30)
        add_test(NAME fuzz_cista_archive_runner_safety
            COMMAND "${CMAKE_COMMAND}"
                "-DFUZZ_EXECUTABLE=$<TARGET_FILE:fuzz_cista_archive>"
                "-DFUZZ_DICTIONARY=${CMAKE_CURRENT_SOURCE_DIR}/fuzz/cista_archive.dict"
                "-DFUZZ_RUNNER=${CMAKE_CURRENT_SOURCE_DIR}/run_fuzzer_smoke.cmake"
                "-DFUZZ_SAFETY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fuzz-runner-safety/cista_archive"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_fuzzer_smoke_safety.cmake")
        set_tests_properties(fuzz_cista_archive_runner_safety PROPERTIES
            LABELS fuzz
            RUN_SERIAL TRUE
            TIMEOUT 30)
    endif()

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
