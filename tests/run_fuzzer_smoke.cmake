foreach(MUSTACHE_REQUIRED_VARIABLE IN ITEMS
        FUZZ_EXECUTABLE
        FUZZ_DICTIONARY
        FUZZ_SEED_CORPUS
        FUZZ_ALLOWED_SCRATCH_PARENT
        FUZZ_SCRATCH_ROOT
        FUZZ_ARTIFACT_DIRECTORY)
    if(NOT DEFINED ${MUSTACHE_REQUIRED_VARIABLE} OR "${${MUSTACHE_REQUIRED_VARIABLE}}" STREQUAL "")
        message(FATAL_ERROR "${MUSTACHE_REQUIRED_VARIABLE} is required")
    endif()
    if(NOT IS_ABSOLUTE "${${MUSTACHE_REQUIRED_VARIABLE}}")
        message(FATAL_ERROR "${MUSTACHE_REQUIRED_VARIABLE} must be an absolute path")
    endif()
endforeach()

if(NOT EXISTS "${FUZZ_EXECUTABLE}")
    message(FATAL_ERROR "fuzzer executable does not exist: ${FUZZ_EXECUTABLE}")
endif()
if(NOT IS_DIRECTORY "${FUZZ_SEED_CORPUS}")
    message(FATAL_ERROR "fuzzer seed corpus does not exist: ${FUZZ_SEED_CORPUS}")
endif()
if(NOT EXISTS "${FUZZ_DICTIONARY}")
    message(FATAL_ERROR "fuzzer dictionary does not exist: ${FUZZ_DICTIONARY}")
endif()
if(NOT IS_DIRECTORY "${FUZZ_ALLOWED_SCRATCH_PARENT}")
    message(FATAL_ERROR "allowed scratch parent does not exist: ${FUZZ_ALLOWED_SCRATCH_PARENT}")
endif()
if(NOT IS_DIRECTORY "${FUZZ_ARTIFACT_DIRECTORY}")
    message(FATAL_ERROR "fuzzer artifact directory does not exist: ${FUZZ_ARTIFACT_DIRECTORY}")
endif()
if(IS_SYMLINK "${FUZZ_SCRATCH_ROOT}")
    message(FATAL_ERROR "FUZZ_SCRATCH_ROOT must not be a symlink")
endif()
if(EXISTS "${FUZZ_SCRATCH_ROOT}" AND NOT IS_DIRECTORY "${FUZZ_SCRATCH_ROOT}")
    message(FATAL_ERROR "FUZZ_SCRATCH_ROOT exists but is not a directory")
endif()

get_filename_component(FUZZ_ALLOWED_SCRATCH_PARENT_REAL "${FUZZ_ALLOWED_SCRATCH_PARENT}" REALPATH)
get_filename_component(FUZZ_EXECUTABLE_REAL "${FUZZ_EXECUTABLE}" REALPATH)
get_filename_component(FUZZ_DICTIONARY_REAL "${FUZZ_DICTIONARY}" REALPATH)
get_filename_component(FUZZ_SEED_CORPUS_REAL "${FUZZ_SEED_CORPUS}" REALPATH)
get_filename_component(FUZZ_ARTIFACT_DIRECTORY_REAL "${FUZZ_ARTIFACT_DIRECTORY}" REALPATH)
get_filename_component(FUZZ_SCRATCH_PARENT "${FUZZ_SCRATCH_ROOT}" DIRECTORY)
get_filename_component(FUZZ_SCRATCH_PARENT_REAL "${FUZZ_SCRATCH_PARENT}" REALPATH)
get_filename_component(FUZZ_SCRATCH_NAME "${FUZZ_SCRATCH_ROOT}" NAME)

if(FUZZ_SCRATCH_NAME STREQUAL "" OR FUZZ_SCRATCH_NAME STREQUAL "." OR FUZZ_SCRATCH_NAME STREQUAL ".." OR
    NOT FUZZ_SCRATCH_PARENT_REAL STREQUAL FUZZ_ALLOWED_SCRATCH_PARENT_REAL)
    message(FATAL_ERROR "FUZZ_SCRATCH_ROOT must be a strict direct child of the allowed scratch parent")
endif()
if(EXISTS "${FUZZ_SCRATCH_ROOT}")
    get_filename_component(FUZZ_SCRATCH_ROOT_REAL "${FUZZ_SCRATCH_ROOT}" REALPATH)
else()
    set(FUZZ_SCRATCH_ROOT_REAL "${FUZZ_ALLOWED_SCRATCH_PARENT_REAL}/${FUZZ_SCRATCH_NAME}")
endif()
get_filename_component(FUZZ_SCRATCH_ROOT_REAL_PARENT "${FUZZ_SCRATCH_ROOT_REAL}" DIRECTORY)
if(NOT FUZZ_SCRATCH_ROOT_REAL_PARENT STREQUAL FUZZ_ALLOWED_SCRATCH_PARENT_REAL)
    message(FATAL_ERROR "canonical scratch root escaped the allowed scratch parent")
endif()

function(mustache_paths_overlap MUSTACHE_LEFT MUSTACHE_RIGHT MUSTACHE_RESULT)
    if(MUSTACHE_LEFT STREQUAL MUSTACHE_RIGHT)
        set(${MUSTACHE_RESULT} TRUE PARENT_SCOPE)
        return()
    endif()
    file(RELATIVE_PATH MUSTACHE_LEFT_TO_RIGHT "${MUSTACHE_LEFT}" "${MUSTACHE_RIGHT}")
    file(RELATIVE_PATH MUSTACHE_RIGHT_TO_LEFT "${MUSTACHE_RIGHT}" "${MUSTACHE_LEFT}")
    if(NOT MUSTACHE_LEFT_TO_RIGHT MATCHES "^\\.\\.(/|$)" OR
        NOT MUSTACHE_RIGHT_TO_LEFT MATCHES "^\\.\\.(/|$)")
        set(${MUSTACHE_RESULT} TRUE PARENT_SCOPE)
    else()
        set(${MUSTACHE_RESULT} FALSE PARENT_SCOPE)
    endif()
endfunction()

foreach(FUZZ_PROTECTED_PATH IN ITEMS
        "${FUZZ_EXECUTABLE_REAL}"
        "${FUZZ_DICTIONARY_REAL}"
        "${FUZZ_SEED_CORPUS_REAL}"
        "${FUZZ_ARTIFACT_DIRECTORY_REAL}")
    mustache_paths_overlap("${FUZZ_SCRATCH_ROOT_REAL}" "${FUZZ_PROTECTED_PATH}" FUZZ_PATHS_OVERLAP)
    if(FUZZ_PATHS_OVERLAP)
        message(FATAL_ERROR "scratch root overlaps protected path: ${FUZZ_PROTECTED_PATH}")
    endif()
endforeach()

file(REMOVE_RECURSE "${FUZZ_SCRATCH_ROOT}")
set(FUZZ_OUTPUT_CORPUS "${FUZZ_SCRATCH_ROOT}/corpus")
file(MAKE_DIRECTORY "${FUZZ_OUTPUT_CORPUS}" "${FUZZ_ARTIFACT_DIRECTORY}")

execute_process(
    COMMAND "${FUZZ_EXECUTABLE}"
        -runs=1000
        -seed=424242
        -max_len=4096
        -timeout=5
        "-dict=${FUZZ_DICTIONARY}"
        "-artifact_prefix=${FUZZ_ARTIFACT_DIRECTORY}/"
        "${FUZZ_OUTPUT_CORPUS}"
        "${FUZZ_SEED_CORPUS}"
    RESULT_VARIABLE FUZZ_RESULT)
if(NOT FUZZ_RESULT STREQUAL "0")
    message(FATAL_ERROR "fuzzer smoke test failed with status ${FUZZ_RESULT}")
endif()
