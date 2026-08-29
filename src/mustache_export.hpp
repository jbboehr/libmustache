#ifndef MUSTACHE_EXPORT_HPP
#define MUSTACHE_EXPORT_HPP

/*
 * Public shared-library symbol visibility. Windows uses DLL import/export;
 * GCC and Clang override the hidden implementation default for public APIs.
 * Static consumers must define MUSTACHE_STATIC_DEFINE; the exported CMake
 * static target does this automatically.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(MUSTACHE_STATIC_DEFINE)
#define MUSTACHE_API
#elif defined(MUSTACHE_BUILDING_LIBRARY)
#define MUSTACHE_API __declspec(dllexport)
#else
#define MUSTACHE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define MUSTACHE_API __attribute__((visibility("default")))
#else
#define MUSTACHE_API
#endif

#endif
