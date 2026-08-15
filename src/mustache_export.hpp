#ifndef MUSTACHE_EXPORT_HPP
#define MUSTACHE_EXPORT_HPP

/*
 * Public symbol visibility for the Windows DLL. Static consumers must define
 * MUSTACHE_STATIC_DEFINE; the exported CMake static target does this
 * automatically. The shared-library build defines MUSTACHE_BUILDING_LIBRARY.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(MUSTACHE_STATIC_DEFINE)
#    define MUSTACHE_API
#  elif defined(MUSTACHE_BUILDING_LIBRARY)
#    define MUSTACHE_API __declspec(dllexport)
#  else
#    define MUSTACHE_API __declspec(dllimport)
#  endif
#else
#  define MUSTACHE_API
#endif

#endif
