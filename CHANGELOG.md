# Release notes

## [Unreleased]

- Avoid GCC/AArch64 internal compiler errors during LTO by using xxHash's
  `memcpy` read mode.
- Fix inconsistent archived-template type hashes with GCC 16 LTO by backporting
  xxHash's strict-aliasing fix and preserving the private adapter's optimization
  boundary for system xxHash builds (fixes #29).

## [0.6.2] - 2026-09-11

- Add opt-in link-time optimization support: `MUSTACHE_ENABLE_LTO` for CMake
  (interprocedural optimization, applied only when the toolchain supports it)
  and `--enable-lto` for Autotools (`-flto=auto -ffat-lto-objects` on GCC,
  `-flto` elsewhere). LTO stays off by default because the installed static
  archive would embed compiler-specific objects.
- Skip the apostrophe-prefixed pkg-config install checks when LTO flags reach
  the consumer build. GCC's `lto-wrapper` cannot quote apostrophes in the
  paths it passes to its LTRANS recipes, so those installs fail inside GCC
  under `-flto` even though the pkg-config escaping under test is correct
  (fixes #28).
- Run a Nix check that builds and tests the library with Fedora's default
  LTO flags (`-flto=auto -ffat-lto-objects`).
- Type YAML 1.1 plain scalars during data parsing: `true`/`false`/`yes`/`no`/
  `on`/`off` (and their case variants) become boolean data, `null`/`~` and
  empty values become null data, and decimal, hexadecimal (`0x`), octal
  (leading zero), and binary (`0b`) integers plus dotted decimals become
  integer and floating-point data. Quoted scalars are unaffected and remain
  strings, and integer spellings such as `0x1F` render as their decimal value.
  Explicit `!!bool`, `!!int`, `!!float`, and `!!null` tags are honored,
  including on quoted scalars; invalid tagged values fall back to strings.
  Because libyaml reports implicit scalars with the `!!str` tag, an explicit
  `!!str` tag cannot force string treatment of a plain value — use quotes
  instead. This matches the JSON adapter and the mustache spec's falsiness
  rules (fixes #18).

## [0.6.1] - 2026-09-10

Shared-library ABI 6 and the public C++ API are unchanged from 0.6.0.

- Fix archived-template builds on systems with newer fmt headers installed.
- Add binary packages containing libmustache, public headers, package metadata,
  and `mustachec` for Windows x86/x64, Linux x64 (glibc/musl), and macOS arm64.
  Static and shared variants are available, except musl, which is static only.
  See the [Windows](docs/windows-binaries.md) and
  [Linux/macOS](docs/unix-binaries.md) package guides for requirements and usage.

## [0.6.0] - 2026-09-07

Version 0.6.0 introduces shared-library ABI 6 and intentionally breaks binary
and source compatibility with 0.5. Rebuild C++ applications and language
bindings against the new headers and library. The
[ABI 6 migration guide](docs/development/abi-6-source-migration-2026-08-20.md)
lists the source changes and retained compatibility APIs.

### Build requirements

- C++17 is required, including floating-point `std::to_chars` support in the
  standard library.
- nlohmann/json 3.10.5 or newer replaces json-c as the optional JSON parser.
  It is a private build dependency. Installed consumers do not need its headers.
- JSON and YAML adapters are independently auto-detected. Both build systems
  support explicitly requiring or disabling either adapter. Disabled parser
  entry points remain available and throw `mustache::Exception`.
- Installed CMake packages provide shared and static components. Static
  consumers must link any additional libraries required by their libmustache
  build.
  Windows static consumers using their own build metadata must define
  `MUSTACHE_STATIC_DEFINE`.

### Library changes

- `Data` owns its values and preserves scalar types. Replace direct field and
  pointer access with factories, accessors, and container operations. `Node`
  owns its child nodes and is move-only. `Mustache` and `Renderer` are
  non-copyable and non-movable.
- Floating-point values render with locale-independent, shortest round-trip
  formatting.
- `CompiledTemplate` owns an immutable template for repeated rendering without
  recompiling. `PartialMap` owns reusable compiled partials. Prefer
  value-returning parsing, rendering, and serialization APIs and explicitly
  sized `std::string_view` input.
- Tokenization, data parsing, rendering, and serialization enforce resource
  limits. Zero is a hard maximum, never an unlimited setting. Existing callers
  may need explicit limits for workloads that exceed the defaults.
- `LambdaRenderContext` limits rendering helpers to an active callback.
  `LambdaResult` distinguishes literal output from template source, and
  `renderTemplate()` preserves the literal interpretation of rendered output.
  Ordinary callback strings still use template evaluation by default.
- Section lambdas receive the exact original section body, including tag
  spacing. Standalone whitespace, partial indentation, dotted-name lookup, and
  interpolation-lambda escaping have been corrected. Dynamic names and
  inheritance remain unsupported extensions.
- JSON parsing rejects invalid UTF-8, byte-order marks, non-finite numeric
  results, and trailing input. YAML parsing accepts one document and rejects
  recursive aliases. Both reject raw NUL bytes in explicitly sized input.

### Persistence and compatibility

Checked legacy AST reads remain supported throughout 0.6.x. Legacy writers
remain available, but reject sections whose custom delimiters or original
callback text cannot be preserved by that format. Keep template source for
recompilation, and version cache keys when ABI 5 and ABI 6 processes coexist.

Experimental archived-template support is enabled automatically on supported
little-endian targets. `ArchivedTemplate` provides reusable handles for cached
templates. Loaded handles remain valid after the input buffer is released.

Archive bytes depend on the platform and library build and are not a durable
interchange format. Include `archivedTemplateCompatibilityTag()` verbatim in
cache keys and regenerate incompatible entries from source. Experimental
archive bytes may change while the C++ API remains compatible.

The exported compatibility APIs listed in the migration guide remain through
0.6.x. This release adds no deprecation warnings for those APIs. Removing these
APIs or legacy AST reads requires a separately announced incompatible release.

### Command line and distribution

`mustachec` reports invalid input and file I/O failures with a nonzero exit
status and a diagnostic. Output files preserve rendered bytes without newline
translation. The legacy `-r` option is a deprecated no-op.

Source distributions now include the Mustache specification license alongside
the fixtures.

[Unreleased]: https://github.com/jbboehr/libmustache/compare/v0.6.2...HEAD
[0.6.2]: https://github.com/jbboehr/libmustache/compare/v0.6.1...v0.6.2
[0.6.1]: https://github.com/jbboehr/libmustache/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/jbboehr/libmustache/compare/v0.5.0...v0.6.0
