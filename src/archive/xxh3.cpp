#include <cstddef>
#include <cstdint>

#include <xxhash.h>

extern "C" std::uint64_t mustache_cista_xxh3_64bits_with_seed(
    const void * data, std::size_t size, std::uint64_t seed) noexcept
{
  return XXH3_64bits_withSeed(data, size, seed);
}
