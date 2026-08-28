# libmustache

[![Software License](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)](LICENSE.md)
[![GitHub CI Build Status](https://github.com/jbboehr/libmustache/workflows/ci/badge.svg)](https://github.com/jbboehr/libmustache/actions?query=workflow%3Aci)
[![Appveyor Build status](https://ci.appveyor.com/api/projects/status/1bwyjyo1cel03b2r?svg=true)](https://ci.appveyor.com/project/jbboehr/libmustache)

libmustache is a C++17 implementation of
[Mustache](https://mustache.github.com/). It provides shared and static
libraries plus the `mustachec` command-line renderer. It was originally written
for [php-mustache](https://github.com/jbboehr/php-mustache), but can be consumed
as an ordinary C++ library.

## Requirements

- A C++17 compiler
- nlohmann/json 3.10.5 or newer for JSON input (optional; build only)
- libyaml for YAML input (optional)
- zlib for experimental archived templates (optional)
- xxHash 0.8 or newer when selecting a system xxHash installation (optional)
- CMake 3.18 or Autoconf 2.69 with Automake and Libtool

Clone the specification submodule when tests are required:

```sh
git clone --recurse-submodules https://github.com/jbboehr/libmustache.git
cd libmustache
```

## Build with CMake

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /usr/local
```

Top-level builds enable the CLI and tests by default. When libmustache is added
with `add_subdirectory()`, both default to off. They can always be controlled
explicitly:

```sh
cmake -S . -B build \
  -DMUSTACHE_BUILD_CLI=OFF \
  -DMUSTACHE_ENABLE_TESTS=OFF
```

Maintainer and CI builds can add `-DMUSTACHE_WARNINGS_AS_ERRORS=ON` to promote
project warnings to errors without changing the default for downstream builds.

JSON and YAML support default to `AUTO`: each adapter is built when its
dependency is detected and omitted otherwise. Use `ON` to require a format or
`OFF` to disable it even when its dependency is installed:

```sh
cmake -S . -B build \
  -DMUSTACHE_ENABLE_JSON=ON \
  -DMUSTACHE_ENABLE_YAML=OFF
```

The accepted values are `AUTO`, `ON`, and `OFF`. A format set to `ON` makes
configuration fail if its dependency is unavailable.

Experimental archived-template internals and their tests are default-off. They
use pinned private Cista and xxHash snapshots by default; packagers can
independently select system installations:

```sh
cmake -S . -B build \
  -DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=ON \
  -DMUSTACHE_USE_SYSTEM_CISTA=ON \
  -DMUSTACHE_USE_SYSTEM_XXHASH=ON
```

Cista and xxHash remain private in every mode and are not required by
installed-library consumers. Custom system locations can be supplied through
`CMAKE_PREFIX_PATH`, `cista_DIR`, and `xxHash_DIR`. The feature always requires
zlib for its selected integrity policy. The experimental format preamble and
Cista validation responsibilities are specified in the
[archive format document](docs/development/cista-archive-format-v1.md).

Installed CMake packages expose separate components. The shared component is
the default and does not require dependency development packages at consume
time:

```cmake
find_package(mustache 0.6 CONFIG REQUIRED)
target_link_libraries(example PRIVATE mustache::mustache)
```

Static consumers request the static component. It also locates libyaml when
the installed library was built with YAML support. The header-only JSON parser
is compiled entirely into libmustache and is not required when consuming an
installed library:

```cmake
find_package(mustache 0.6 CONFIG REQUIRED COMPONENTS static)
target_link_libraries(example PRIVATE mustache::mustache_static)
```

## Build with Autotools

```sh
autoreconf --force --install --verbose
mkdir build-autotools
cd build-autotools
../configure
make --jobs="$(nproc)"
make check
sudo make install
```

Use `--without-mustache-spec` when configuring a source tree without the
specification submodule. `./configure --help` lists the warning-as-error,
hardening, sanitizer, coverage, and profiling options.

JSON and YAML support are independently auto-detected by default. Use
`--with-json=yes` or `--with-yaml=yes` to require the corresponding dependency,
and `--without-json` or `--without-yaml` to disable an adapter explicitly.

Use `--enable-archived-templates` for the experimental archived-template
internals and tests. This selects the bundled Cista and xxHash snapshots; add
`--with-system-cista` and/or `--with-system-xxhash` to require system
installations instead. Use `CISTA_CFLAGS`, `XXHASH_CFLAGS` and `XXHASH_LIBS`,
or `PKG_CONFIG_PATH` for custom locations.

## Nix

```sh
nix build .#libmustache
nix build .#libmustache-cmake
nix flake check
nix run .#mustachec -- -v
```

The default Nix package uses Autotools; `libmustache-cmake` exercises the CMake
packaging path. When importing `default.nix`, pass `nlohmann_json = null` or
`libyaml = null` to omit that dependency and explicitly disable the
corresponding adapter. Set `archivedTemplateSupport = true` to exercise the
experimental archive implementation, and set `useSystemCista = true` only when
supplying a non-null `cista` package; the bundled snapshot is the default.
Likewise, `useSystemXxhash = true` requires a non-null `xxhash`; pass
`xxhash = null` to exercise the bundled default.
`cistaBenchmarkSupport` is CMake-only and requires `cmakeSupport = true`.

The parsing entry points remain available regardless of the selected feature
set so builds have a stable C++ ABI. Calling `Data::fromJSON()` or
`Data::fromYAML()` when its adapter was omitted throws `mustache::Exception`.
The installed `mustache_config.h` advertises enabled adapters with
`MUSTACHE_HAVE_LIBJSON` and `MUSTACHE_HAVE_LIBYAML`; `mustachec -v` reports the
same feature state.

The installed `mustache_config.h` defines `MUSTACHE_CXX_STANDARD` as `17` and
defines `MUSTACHE_HAVE_CXX17`. `MUSTACHE_HAVE_CXX11` remains defined as a
deprecated source-compatibility alias because the required C++17 baseline
necessarily includes C++11 support.

Template source, delimiters, lambda section text, and serialized AST input
have length-aware C++17 entry points. Prefer `std::string_view` for text and
`Node::unserializeOwned()` for complete serialized AST byte strings. Prefer
`Node::serializeValue()` when legacy AST persistence is required. The original
owning-pointer `serialize()` and `unserialize()` calls remain supported as
compatibility adapters.

Template parsing also accepts `Tokenizer::Limits`. The compatibility defaults
permit 64 MiB of input, 62 nested sections, 100,000 AST nodes excluding the
root, 1 MiB per tag, and 1 KiB per delimiter. Each value is a hard maximum;
zero means that a template consuming that resource is rejected, not unlimited.
Applications processing untrusted templates should supply limits appropriate
for their workload. The 62-section default leaves room for the root and closing
node within the serializer's default 64-node root-to-leaf limit.

## Library use

Prefer `CompiledTemplate` for application and extension code. It owns an
immutable parsed template, is cheap to copy, and can be rendered repeatedly
without exposing the AST representation:

```cpp
#include <mustache/mustache.hpp>

#include <string>

mustache::Data data =
    mustache::Data::fromJSON("{\"name\":\"world\"}");
mustache::CompiledTemplate greeting =
    mustache::compile("Hello {{name}}!");
std::string output = mustache::render(greeting, data);
```

Partials have explicit shared ownership through compiled handles as well:

```cpp
mustache::PartialMap partials;
partials.emplace("greeting", mustache::compile("Hello {{name}}!"));
mustache::CompiledTemplate page = mustache::compile("<p>{{>greeting}}</p>");
std::string output = mustache::render(page, data, partials);
```

`Data` is a copyable owned value. It preserves null, boolean, integer,
floating-point, string, array, object, and lambda types without public
ownership fields:

```cpp
mustache::Data data = mustache::Data::object({
    {"name", mustache::Data::string("Ada")},
    {"enabled", mustache::Data::boolean(true)},
    {"values", mustache::Data::array({
        mustache::Data::integer(1),
        mustache::Data::integer(2)
    })}
});
data.set("project", mustache::Data::string("libmustache"));
```

Objects and arrays own their nested `Data` values directly. Lambda factories
accept `std::unique_ptr<Lambda>`; copies of the resulting `Data` value share
the callback through an internal RAII handle. The pointer-returning
`createFromJSON()` and `createFromYAML()` entry points remain as compatibility
facades, but new code should use the value-returning `fromJSON()` and
`fromYAML()` forms.

Both parsers accept explicitly sized `std::string_view` input and a shared
resource policy:

```cpp
mustache::Data::ParseLimits limits;
limits.maxInputBytes = 1024 * 1024;
limits.maxNestingDepth = 32;
limits.maxNodes = 10000;
limits.maxStringBytes = 1024 * 1024;
limits.maxContainerEntries = 10000;

mustache::Data data = mustache::Data::fromJSON(input, limits);
```

The defaults are 64 MiB of input, 32 value nodes along a
root-to-leaf path, 100,000 expanded value nodes, 64 MiB of aggregate string
and object-key bytes, and 100,000 aggregate container entries. Every field is
a hard maximum; zero never means unlimited. JSON object keys and preserved
floating-point spellings count toward string bytes. YAML aliases are expanded
into owned values, so every expansion counts again toward nodes, strings, and
container entries; recursive aliases are rejected. JSON is converted directly
into owned values by a bounded SAX adapter, without an intermediate DOM. YAML
uses a bounded structural preflight before libyaml constructs its document and
rechecks the policy during conversion. Parsing retains an implementation
safety ceiling of 256 value nodes along any path even if a caller supplies a
higher nesting limit.

Explicitly sized input containing raw NUL bytes is rejected with a dedicated
error. JSON strings and object keys may contain an escaped `\u0000` and retain
its byte. JSON also rejects invalid UTF-8, byte-order marks, non-finite numeric
results, and trailing non-whitespace input. YAML accepts one document only: a
second document marker is rejected even when the additional document is empty,
while malformed content after an explicit document end is reported as trailing
content. This single-document rule is stricter than ABI 5, which ignored input
after the first YAML document.

Rendering also uses an explicit hard resource policy:

```cpp
mustache::RenderLimits limits;
limits.maxOutputBytes = 1024 * 1024;
limits.maxNestingDepth = 64;
limits.maxNodeVisits = 100000;
limits.maxLambdaTemplateBytes = 1024 * 1024;

std::string output = mustache::render(compiled, data, partials, limits);
```

The defaults permit 64 MiB of output, 256 active template-node levels,
1,000,000 aggregate node visits, and 64 MiB of aggregate template text
processed by lambdas. Every field is a hard maximum and zero never means
unlimited. The root is the first node visit and first nesting level. Repeated
list sections, partials, and lambda-generated templates share the node-visit
budget. Every AST node parsed from lambda output is charged once when parsed
and again if traversed, including nodes in false sections. Section text sent
to callbacks and template text returned from callbacks share the
lambda-template byte budget across the complete render. An implementation
ceiling rejects nesting beyond 256 active nodes even if a caller configures a
higher maximum.

For the legacy output-buffer API, `maxOutputBytes` applies to the initial
top-level buffer plus all bytes appended to every output buffer during the
render, including buffers passed to `renderForLambda()`. Escaped output is
sized before it is appended, so exhausting the limit cannot partially append
one escaped scalar. A failed render can leave the caller's output buffer
holding a prefix of the intended result; discard or clear it before reuse.
Limit failures and callback exceptions restore the renderer's internal
temporary output pointer, lookup stack, and callback state, allowing the
renderer to be initialized and used again. `Renderer::renderForLambda()` is
accepted only while a section-lambda callback is active on that renderer; use
when no callback is active is rejected. `Renderer` and its containing
`Mustache` facade are non-copyable and non-movable so borrowed operation state
cannot be duplicated or relocated while active.

New section lambdas should override the scoped callback overload:

```cpp
class SectionLambda : public mustache::Lambda {
  public:
    std::string invoke() override { return std::string(); }

    std::string invoke(std::string_view,
        mustache::LambdaRenderContext context) override
    {
      mustache::Node name(
          mustache::Node::TypeVariable, "name", mustache::Node::FlagEscape);
      return context.render(name);
    }
};
```

`LambdaRenderContext` is copyable so a binding can retain it safely, but every
copy becomes inactive when that exact callback frame returns or throws. Later
calls fail with `Lambda render context is no longer active` without touching
the former renderer. Context rendering is synchronous and not intended for
concurrent use. The legacy `invoke(std::string *, Renderer *)` hook and
`Renderer::renderForLambda()` remain as compatibility adapters; a raw renderer
pointer obtained through that hook must never be retained.

Code migrating from ABI 5 should replace direct representation access as
follows:

The complete compatibility classification, AST-cache policy, and binding
checklist are in the
[ABI 6 source-migration guide](docs/development/abi-6-source-migration-2026-08-20.md).

| ABI 5 API | ABI 6 replacement |
|---|---|
| `data.type` | `data.type()` |
| `*data.val` | `data.stringValue()` |
| `data.data` | `data.objectItems()`, `data.find()`, or `data.set()` |
| `data.children` | `data.listItems()` or `data.push_back()` |
| `data.array` | `data.arrayItems()` or `data.push_back()` |
| `data.length` | `data.arrayItems().size()` |
| `data.lambda` | `data.lambdaValue()` or `Data::lambda()` |
| `searchStack()` / `searchStackNR()` | No public replacement; lookup is renderer-owned |

Container emptiness now follows the owned container's actual size rather than
the former independently mutable `length` field. Borrowed pointers and
container references must not be retained while structurally modifying the
owning data tree, and a tree must not be modified while it is being rendered.

Use `Mustache::compile()` when custom delimiters or tokenizer configuration are
needed. Both member and free `compile()` overloads accept `Tokenizer::Limits`.
The public `Node` tokenizer and renderer entry points remain available as a
source-compatibility surface for existing consumers.

`renderer.hpp` no longer supplies incidental declarations from
`tokenizer.hpp`, `utils.hpp`, or `<iostream>`; consumers using those APIs must
include their defining headers directly.

`Node` is move-only and owns its complete compatibility AST. Text and dotted
name components are stored as values; `children`, `child`, and `partials` use
`std::unique_ptr<Node>` to make ownership explicit. Construct manually supplied
nodes with `std::make_unique<Node>()` and move them into those containers. The
tokenizer and decoder publish complete trees transactionally.

The compatibility `Node::to_template_string()` and
`children_to_template_string()` helpers enforce `Node::TemplateStringLimits`.
Their defaults permit 64 MiB of reconstructed output, 64 nodes along a
root-to-leaf path, and 100,001 visited nodes including the receiver. Explicit
limits can be stricter, but a separate 256-level implementation ceiling always
protects the C++ call stack. The receiver counts even when only its children
are reconstructed, and skipped closing nodes still consume the node-work
budget. These helpers and the installed `Stack` template remain transitional
compatibility APIs; new rendering code should use `CompiledTemplate`.

## Windows

The repository contains a vcpkg manifest and is tested with Visual Studio 2022
for both `x86-windows` and `x64-windows`:

```bat
cmake -S . -B build -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix artifacts
```

## Command-line use

`mustachec` is a render-only command. It requires a template; when `-d` is
omitted, the template is rendered against null data. When a data file is given,
its format is selected case-insensitively from `.json`, `.yml`, or `.yaml`.

```sh
mustachec -t template.mustache -d data.json
mustachec -t template.mustache -d data.yml
mustachec -t template.mustache -d data.json \
  -l header=header.mustache -l footer=footer.mustache
mustachec -t template.mustache -d data.json -o rendered.txt
mustachec -v
```

`-l name=file` adds a named partial and may be repeated. `-n count` repeats the
render from 1 to 1,000,000 times for benchmarking and writes only the final
result. `-o` opens the destination in binary mode and writes the rendered bytes
verbatim, without newline translation. The legacy `-r` option is accepted as a
deprecated no-op and emits a warning. Long option names are also available;
run `mustachec -h` for the complete list.

An empty template is valid and produces empty output. Missing files, an empty
explicit data file, unsupported data-file extensions, malformed templates or
data, recursive partial exhaustion, and file I/O failures produce a concise
diagnostic on standard error and a nonzero exit status. Input parsing and
rendering use the library's documented default resource limits.

## Credits

- [John Boehr](https://github.com/jbboehr)
- [Adam Baratz](https://github.com/adambaratz)
- [All contributors](https://github.com/jbboehr/libmustache/graphs/contributors)

## License

libmustache is released under the [MIT License](LICENSE.md).
