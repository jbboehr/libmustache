# Build and hardening follow-up

**Date:** 2026-08-11  
**Status:** The immediate build-system findings are resolved in the current
working tree. This document records the actionable work intentionally left for
the source-hardening and packaging follow-ups.

The broader security review remains the authoritative inventory of core library
risks: [Repository review](repository-review-2026-08-11.md).

## Resolved in the build-system work

- The CMake package template, downstream consumer, and vcpkg manifest are now
  tracked build inputs, so an ordinary Git-backed `nix build .#...` includes
  them.
- The CMake minimum is 3.18, matching its use of `find_path(... REQUIRED)` and
  `find_library(... REQUIRED)`.
- The flake applies its package-only source filter before passing the source to
  the derivation. Documentation, workflow, and Nix-only edits no longer
  invalidate the library derivations.
- The CMake specification test links directly to libyaml, so nonstandard yaml
  header locations are propagated correctly.
- Autotools warning, hardening, sanitizer, coverage, and profiling options use
  project `AM_*` flags. User `CXXFLAGS`, `CPPFLAGS`, and `LDFLAGS` remain
  authoritative, and an existing `_FORTIFY_SOURCE` definition is respected.
- The Autotools `render-*` helpers use `$(top_srcdir)` and work from a VPATH
  build.
- CMake-generated pkg-config metadata derives its prefix from `pcfiledir`, so a
  relocated tree and `cmake --install --prefix` remain usable. Absolute install
  directories, including Nix split outputs, remain absolute.
- The CMake test suite now enumerates specification files through the native
  Windows API on MSVC instead of unconditionally requiring POSIX `dirent.h`.
  The unrelated miscellaneous test also no longer inherits a libyaml header
  dependency through the specification-test header.
- Native Visual Studio 2022 builds pass for both `x86-windows` and
  `x64-windows`: compilation, all three CTest tests, installation, installed CLI
  execution, installed-package discovery, and downstream consumer execution.
  The validation used MSVC 19.44.35228.0, CMake 3.31.6-msvc6, and vcpkg
  2025-11-19 (`da1f056dc0775ac651bea7e3fbbf4066146a55f3`).
- Installed CMake exports are component-aware. The default `shared` component
  has no consume-time dependency-metadata requirement; the `static` component
  locates json-c and libyaml while respecting `QUIET`, `REQUIRED`, and optional
  component semantics.
- CMake top-level builds retain the CLI and test defaults, while embedded
  `add_subdirectory()` builds default both off. A dedicated fixture prevents
  regressions in this policy.
- Install checks compile and execute pkg-config, shared-CMake, and static-CMake
  consumers. They also exercise dependency-free shared discovery and quiet
  static-discovery failure.
- AppVeyor tests installed shared and static consumers and assembles dependency
  DLLs into the Windows artifact. GitHub Actions additionally covers macOS and
  Autotools' sanitizer option.
- The Nix derivation no longer duplicates propagated dependencies, and its
  generated GitHub Actions matrix remains derived from the flake checks.
- A release-metadata check keeps CMake, Autotools, Nix, vcpkg, consumer-test,
  and ABI/soname versions aligned. The README now documents every supported
  build path; the obsolete Doxygen configuration was removed.

## Remaining actionable work

### 1. Make command-line failures controlled and testable

`mustachec` still throws a bare integer when a file cannot be opened, and only
the data parser is currently protected by an exception handler. A missing
template or empty input can therefore terminate the process with `SIGABRT`.

Replace the integer exception with a typed exception, cover the complete
operation in `main`, print a concise diagnostic, and return a nonzero exit code.
Add CLI regression tests for missing template/data files, empty input, invalid
JSON/YAML, tokenization failure, and rendering failure.

### 2. Resolve compiler warnings as correctness work

The new warning flags expose pre-existing issues. Do not blanket-disable them.
Review and fix each class before enabling warnings-as-errors:

- incomplete enum switches in `Node`;
- signed/unsigned comparisons and narrowing conversions in data parsing,
  tokenization, deserialization, utilities, the CLI, and tests;
- the MSVC `test_utils` format mismatch (`%lu` for a `size_t`; use `%zu` or
  an explicitly matched type);
- the intentional-looking but unannotated fallthrough in
  `Node::to_template_string`;
- shadowed names plus unused variables and parameters in the CLI, library, and
  tests.

The deserialization warnings must be handled together with full buffer-boundary
validation. Merely changing integer types would leave the security issue
described in the repository review unresolved.

## Verified non-issues

- The Nix CMake package has nonempty `out`, `dev`, and `lib` outputs, and its CLI
  has a working library search path.
- The pinned GitHub Actions SHAs match their documented releases.
- `make distcheck` accepts the expanded `EXTRA_DIST` set.
- The Visual Studio generator ignoring `CMAKE_BUILD_TYPE` is harmless because
  AppVeyor builds and tests with `--config Release`.
- The Visual Studio install places the DLL and CLI in `bin`, import and static
  libraries in `lib`, and produces consumable CMake exports for both Win32 and
  x64.
- The top-level `githubActions` flake output and its corresponding schema
  warning follow the current `nix-github-actions` upstream integration pattern;
  hiding the matrix under an unrelated output would make the interface less
  clear without improving the builds.
