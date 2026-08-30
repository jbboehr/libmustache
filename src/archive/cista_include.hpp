#ifndef MUSTACHE_ARCHIVE_CISTA_INCLUDE_HPP
#define MUSTACHE_ARCHIVE_CISTA_INCLUDE_HPP

#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3) && defined(CISTA_FNV1A)
#undef CISTA_FNV1A
#endif
#if defined(CISTA_FMT)
#undef CISTA_FMT
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
#include <cstdint>
#include <intrin.h>

namespace {

std::uint64_t mustacheCistaInterlockedOr64(std::int64_t * block, std::uint64_t mask) noexcept
{
  volatile __int64 * target = reinterpret_cast<volatile __int64 *>(block);
  __int64 observed = _InterlockedCompareExchange64(target, 0, 0);
  for (;;) {
    const __int64 desired = static_cast<__int64>(static_cast<std::uint64_t>(observed) | mask);
    const __int64 previous = _InterlockedCompareExchange64(target, desired, observed);
    if (previous == observed) {
      return static_cast<std::uint64_t>(observed);
    }
    observed = previous;
  }
}

std::uint64_t mustacheCistaInterlockedAnd64(std::int64_t * block, std::uint64_t mask) noexcept
{
  volatile __int64 * target = reinterpret_cast<volatile __int64 *>(block);
  __int64 observed = _InterlockedCompareExchange64(target, 0, 0);
  for (;;) {
    const __int64 desired = static_cast<__int64>(static_cast<std::uint64_t>(observed) & mask);
    const __int64 previous = _InterlockedCompareExchange64(target, desired, observed);
    if (previous == observed) {
      return static_cast<std::uint64_t>(observed);
    }
    observed = previous;
  }
}

} // namespace

// Cista 0.16 uses x64-only MSVC intrinsics in inline helpers even when those
// helpers are not instantiated by the archive schema. Supply equivalent CAS
// loops while parsing the dependency header so Win32 can compile it.
#define _InterlockedOr64 mustacheCistaInterlockedOr64
#define _InterlockedAnd64 mustacheCistaInterlockedAnd64
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#if defined(MUSTACHE_USE_VENDORED_CISTA)
#include <cista.h>
#else
#include <cista/serialization.h>
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
#undef _InterlockedAnd64
#undef _InterlockedOr64
#endif

#endif
