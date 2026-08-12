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

- the `Data::~Data()` `TypeList` to `TypeArray` fallthrough;
- `Node` member-initialization order;
- incomplete enum switches in `Data`, `Node`, and `Renderer`;
- signed/unsigned comparisons in tokenization and deserialization;
- the intentional-looking but unannotated fallthrough in
  `Node::to_template_string`;
- unused variables and parameters in the CLI, utilities, and tests.

The deserialization warnings must be handled together with full buffer-boundary
validation. Merely changing integer types would leave the security issue
described in the repository review unresolved.

### 3. Improve installed CMake package failure semantics

On non-MSVC systems, `mustacheConfig.cmake` uses required pkg-config lookups for
both dependencies. This makes `find_package(mustache QUIET)` fail noisily, and
consumers of only the shared target still need pkg-config and dependency
development metadata because the shared and static targets are exported
together.

Evaluate either component-aware exports or separate shared/static package
metadata. Dependency lookup failures should set `mustache_FOUND` and a useful
`mustache_NOT_FOUND_MESSAGE` while respecting `QUIET` and `REQUIRED`.

Imported-target `GLOBAL` scope is not, by itself, a fix: the exported Mustache
targets have the same directory visibility as their dependency targets.

### 4. Make CMake subproject behavior explicit

`include(CTest)` creates the global `BUILD_TESTING` option, and
`MUSTACHE_ENABLE_TESTS` currently inherits it. Decide and test whether embedding
libmustache with `add_subdirectory()` should build its CLI and tests by default.
A common policy is to default developer tools to on only when libmustache is the
top-level project, while retaining explicit override options.

### 5. Finish small Nix derivation cleanup

- Confirm and remove the duplicate direct `buildInputs` entries when
  `propagatedBuildInputs` already supplies json-c and libyaml.
- Correct the visual indentation of the downstream consumer build commands.
- Consider moving the GitHub Actions matrix under a conventional flake output if
  eliminating the expected “unknown flake output `githubActions`” warning is
  worth changing the workflow interface.

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
