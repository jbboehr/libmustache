if(MUSTACHE_ENABLE_CISTA_BENCHMARK)
    add_executable(mustache_ast_cache_vs_source ast-cache-vs-source.cpp)
else()
    add_executable(mustache_ast_cache_vs_source EXCLUDE_FROM_ALL
        ast-cache-vs-source.cpp)
endif()
target_link_libraries(mustache_ast_cache_vs_source PRIVATE mustache_static)
mustache_apply_build_options(mustache_ast_cache_vs_source)
mustache_enable_clang_tidy(mustache_ast_cache_vs_source)

if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES OR MUSTACHE_ENABLE_CISTA_BENCHMARK)
    add_library(mustache_cista_archive_experiment STATIC cista-archive.cpp)
    target_include_directories(mustache_cista_archive_experiment PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(mustache_cista_archive_experiment
        PUBLIC mustache_static
        PRIVATE ${MUSTACHE_CISTA_TARGET} ${MUSTACHE_XXHASH_TARGET} ZLIB::ZLIB)
    if(NOT MUSTACHE_USE_SYSTEM_CISTA)
        target_compile_definitions(mustache_cista_archive_experiment
            PRIVATE MUSTACHE_USE_VENDORED_CISTA=1)
    endif()
    if(MUSTACHE_ENABLE_ARCHIVED_TEMPLATES OR MUSTACHE_CISTA_BENCHMARK_BUILTIN_XXH3)
        target_include_directories(mustache_cista_archive_experiment BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/cista-xxh3")
        target_compile_definitions(mustache_cista_archive_experiment
            PRIVATE CISTA_XXH3=1
            PUBLIC MUSTACHE_CISTA_RUNTIME_VERSION_XXH3=1)
    endif()
    # nixpkgs' Cista 0.16 package enables optional fmt integration without
    # propagating fmt to consumers. This experiment does not use that feature;
    # undefine it locally instead of mutating the imported target globally.
    if(MSVC)
        target_compile_options(mustache_cista_archive_experiment PRIVATE
            /UCISTA_FMT)
    else()
        target_compile_options(mustache_cista_archive_experiment PRIVATE
            -UCISTA_FMT)
    endif()
    mustache_apply_build_options(mustache_cista_archive_experiment)
    mustache_enable_clang_tidy(mustache_cista_archive_experiment)
    if(MUSTACHE_ENABLE_FUZZING)
        # The archive fuzzer links this static library, so the decoder and
        # archive-backed renderer need SanitizerCoverage instrumentation too.
        target_compile_options(mustache_cista_archive_experiment PRIVATE
            -fsanitize=fuzzer-no-link)
    endif()

    if(MUSTACHE_ENABLE_CISTA_BENCHMARK)
        target_link_libraries(mustache_ast_cache_vs_source PRIVATE
            mustache_cista_archive_experiment)
        target_compile_definitions(mustache_ast_cache_vs_source PRIVATE
            MUSTACHE_CISTA_BENCHMARK=1)
    endif()
endif()
