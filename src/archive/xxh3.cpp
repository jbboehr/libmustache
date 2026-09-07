#include <cstddef>
#include <cstdint>

#include <xxhash.h>

#if defined(XXH_IMPLEMENTATION) && defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) &&                 \
    XXH_VECTOR == XXH_NEON
// xxHash deliberately uses an unaligned vector load on GCC/AArch64. Suppress
// only its alignment check; keep ASan and the remaining UBSan checks enabled.
// Redeclare the selected helper so the vendored header stays unchanged.
// https://github.com/Cyan4973/xxHash/pull/651
extern "C" {
XXH_FORCE_INLINE uint64x2_t XXH_vld1q_u64(void const * ptr) __attribute__((no_sanitize("alignment")));
}
#endif

extern "C" std::uint64_t mustache_cista_xxh3_64bits_with_seed(
    const void * data, std::size_t size, std::uint64_t seed) noexcept
{
  return XXH3_64bits_withSeed(data, size, seed);
}
