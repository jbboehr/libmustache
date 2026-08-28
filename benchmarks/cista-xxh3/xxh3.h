#ifndef MUSTACHE_BENCHMARK_CISTA_XXH3_H
#define MUSTACHE_BENCHMARK_CISTA_XXH3_H

/* Cista 0.16 expects its development-era xxh3.h and hashes addresses of
   function-local parameters. Keep the modern xxHash implementation behind a
   separately compiled function so optimizing compilers cannot incorrectly
   treat those initialized bytes as indeterminate. */
#include <cstddef>
#include <cstdint>

using XXH64_hash_t = std::uint64_t;

extern "C" std::uint64_t mustache_cista_xxh3_64bits_with_seed(
    const void * data, std::size_t size, std::uint64_t seed) noexcept;

#define XXH3_64bits_withSeed ::mustache_cista_xxh3_64bits_with_seed

#endif
