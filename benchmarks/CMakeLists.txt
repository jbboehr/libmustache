add_executable(mustache_ast_cache_vs_source ast-cache-vs-source.cpp)
target_link_libraries(mustache_ast_cache_vs_source PRIVATE mustache_static)
mustache_apply_build_options(mustache_ast_cache_vs_source)
mustache_enable_clang_tidy(mustache_ast_cache_vs_source)

if(MUSTACHE_ENABLE_CISTA_BENCHMARK)
    add_library(mustache_cista_archive_experiment STATIC cista-archive.cpp)
    target_include_directories(mustache_cista_archive_experiment PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(mustache_cista_archive_experiment
        PUBLIC mustache_static
        PRIVATE cista::cista ZLIB::ZLIB xxHash::xxhash)
    if(MUSTACHE_CISTA_BENCHMARK_BUILTIN_XXH3)
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

    target_link_libraries(mustache_ast_cache_vs_source PRIVATE
        mustache_cista_archive_experiment)
    target_compile_definitions(mustache_ast_cache_vs_source PRIVATE
        MUSTACHE_CISTA_BENCHMARK=1)
endif()
