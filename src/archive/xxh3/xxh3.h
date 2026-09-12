#ifndef MUSTACHE_BENCHMARK_CISTA_XXH3_H
#define MUSTACHE_BENCHMARK_CISTA_XXH3_H

/* Cista 0.16 expects its development-era xxh3.h. Route its calls through a
   private adapter to the selected modern xxHash implementation. The adapter
   also provides an optimization boundary for system xxHash versions that
   lack the strict-aliasing fix backported to the bundled header. */
#include <cstddef>
#include <cstdint>

using XXH64_hash_t = std::uint64_t;

extern "C" std::uint64_t mustache_cista_xxh3_64bits_with_seed(
    const void * data, std::size_t size, std::uint64_t seed) noexcept;

#define XXH3_64bits_withSeed ::mustache_cista_xxh3_64bits_with_seed

#endif
