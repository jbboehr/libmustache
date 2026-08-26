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
- The CMake minimum is consistently 3.18 across the project and its integration
  fixtures.
- The flake applies its package-only source filter before passing the source to
  the derivation. Documentation, workflow, and Nix-only edits no longer
  invalidate the library derivations.
- The CMake specification test links directly to libyaml, so nonstandard yaml
  header locations are propagated correctly.
- Autotools warning, hardening, sanitizer, coverage, and profiling options use
  project `AM_*` flags. User `CXXFLAGS`, `CPPFLAGS`, and `LDFLAGS` remain
  authoritative, and an existing `_FORTIFY_SOURCE` definition is respected.
  Compiler-provided defaults are replaced without redefinition warnings by a
  checked, ordered `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3` pair in both
  Autotools and CMake optimized hardening builds.
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
  locates libyaml when YAML support was built, while respecting `QUIET`,
  `REQUIRED`, and optional component semantics. nlohmann/json is a private
  header-only build dependency when JSON support is enabled and does not
  appear in either installed component.
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

## Completed source-hardening follow-up

### Make command-line failures controlled and testable

`mustachec` is now explicitly render-only and its help, accepted options, and
README agree. The controller uses local RAII state, bounded file reads, owned
`Data`, immutable `CompiledTemplate` and `PartialMap` values, and the modern
length-aware parse/render APIs. A data file is optional and omitted data is a
null value. The unimplemented compile and execute modes are no longer
advertised; the old `-r` human-readable option remains temporarily as a
deprecated, warning no-op for command-line compatibility.

All library, standard, and unknown exceptions are contained at the command
boundary and become prefixed diagnostics with nonzero exit status. Portable
C++ regression tests exercise successful JSON/YAML rendering, partials, output
files, optional null data, empty templates, invalid options and bounded repeat
counts, missing and empty files, invalid JSON/YAML, tokenization failure,
recursive-partial rendering failure, buffered output failure, exact binary
input/output, truncation, and read-buffer boundaries under both CMake and
Autotools. Output files are opened in binary mode so rendered bytes are not
subject to newline translation. The argument controller is tested directly so
these cases do not require BATS or another Unix-only shell dependency;
installed executable smoke tests retain process-level coverage.

The custom argument parser also removes the Windows-only `getopt` build and
vcpkg dependency.

### Keep project builds warning-clean

The remaining GCC, Clang, and MSVC diagnostics were resolved as correctness
work rather than disabled. Node reconstruction now makes its intentional
fallthrough and non-rendering enum values explicit. Utility loops use container
size types instead of narrowing to `int`, test diagnostics use the matching
`size_t` format, specification files retain their full checked length, and
unused or shadowed names were removed.

CMake's `MUSTACHE_WARNINGS_AS_ERRORS` option and Autotools'
`--enable-warnings-as-errors` switch apply `-Werror` or `/WX` only to project
targets and remain opt-in for ordinary downstream builds. GitHub Actions,
AppVeyor, and checked Nix builds enable the policy across GCC, Clang,
AppleClang, and MSVC so new diagnostics fail CI.

### Run production sources through focused static analysis

CMake's opt-in `MUSTACHE_ENABLE_CLANG_TIDY` option runs clang-tidy against the
static library implementation and command-line executable. The shared library
uses the same implementation sources and is intentionally omitted to avoid
duplicate diagnostics; tests and downstream fixtures are outside this
production-source policy.

The pinned Nix `libmustache-clang-tidy` check builds with LLVM and treats every
enabled finding as an error. Because GitHub Actions derives its jobs from the
flake checks, the same analysis runs in CI. The Nix development shell also
provides `clang-tidy`. Reproduce the CI lane with:

```console
nix build .#checks.x86_64-linux.libmustache-clang-tidy
```

The policy enables the Clang analyzer plus the `bugprone`, `performance`, and
`portability` families. It excludes `bugprone-easily-swappable-parameters`, a
subjective API-naming heuristic; `performance-enum-size`, an ABI-sensitive
layout optimization rather than a safety check; and
`portability-template-virtual-member-function`, which diagnoses the private
nlohmann/json SAX interface rather than project code. The intentional
pass-by-value lambda render context has one local suppression because retaining
that value is part of the capability's lifetime contract.

The initial analysis made `noexcept` data queries use non-throwing variant
access, validated optional node data before dereferencing it, widened resource
limit arithmetic before multiplication, replaced dynamically initialized
utility constants with borrowed string views, clarified tokenizer control flow,
and made best-effort command-line error reporting paths explicit.

Automake release archives include the clang-tidy policy and every CMake
subdirectory manifest, while excluding Autotools' generated configuration
header. A release archive can therefore be configured, analyzed, built, and
tested directly with CMake.

### Make the Windows DLL boundary explicit

The checked-in `MUSTACHE_API` macro now exports only public library operations
from the Windows DLL. Stateful public classes annotate their out-of-line
methods rather than exporting their STL-bearing layouts wholesale, preserving
strict MSVC warning checks. CMake propagates the static-consumer definition
through `mustache::mustache_static`, while shared consumers import the explicit
surface by default. Autotools selects and propagates the matching definition
for static-only builds, including through the generated pkg-config metadata.

The Windows test suite audits the resulting DLL with `dumpbin` or
`llvm-readobj`: representative C, value, tokenizer, renderer,
compiled-template, lambda, and utility symbols must be present, while template
instantiations, private state types, parser helpers, and renderer internals
must be absent. This replaces
`WINDOWS_EXPORT_ALL_SYMBOLS` and makes future boundary expansion intentional.

## Remaining actionable work

No build-system hardening findings from this follow-up remain open.

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
