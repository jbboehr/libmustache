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

extern "C" {
// System xxHash may lack the strict-aliasing fix backported to our bundled
// header. Preserve this boundary for those builds, including under GCC LTO
// and in the single-translation-unit configure probe.
// https://github.com/jbboehr/libmustache/issues/29
#if !defined(XXH_IMPLEMENTATION) && defined(__GNUC__) && !defined(__clang__)
#if __has_attribute(noipa)
__attribute__((noipa))
#endif
#endif
std::uint64_t mustache_cista_xxh3_64bits_with_seed(const void * data, std::size_t size, std::uint64_t seed) noexcept
{
  return XXH3_64bits_withSeed(data, size, seed);
}
}
