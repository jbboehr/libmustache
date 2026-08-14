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
- json-c 0.12 or newer
- libyaml
- CMake 3.18 or Autoconf 2.69 with Automake and Libtool
- getopt on Windows when building `mustachec`

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

Installed CMake packages expose separate components. The shared component is
the default and does not require dependency development packages at consume
time:

```cmake
find_package(mustache 0.6 CONFIG REQUIRED)
target_link_libraries(example PRIVATE mustache::mustache)
```

Static consumers request the static component, which also locates json-c and
libyaml:

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
specification submodule. `./configure --help` lists the hardening, sanitizer,
coverage, and profiling options.

## Nix

```sh
nix build .#libmustache
nix build .#libmustache-cmake
nix flake check
nix run .#mustachec -- -v
```

The default Nix package uses Autotools; `libmustache-cmake` exercises the CMake
packaging path.

The installed `mustache_config.h` defines `MUSTACHE_CXX_STANDARD` as `17` and
defines `MUSTACHE_HAVE_CXX17`. `MUSTACHE_HAVE_CXX11` remains defined as a
deprecated source-compatibility alias because the required C++17 baseline
necessarily includes C++11 support.

Template source, delimiters, lambda section text, and serialized AST input
have length-aware C++17 entry points. Prefer `std::string_view` for text and
the pointer-plus-length `Node::unserialize` overload for raw bytes. Existing
`std::string` and `std::vector<uint8_t>` calls remain supported as adapters.

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

Code migrating from ABI 5 should replace direct representation access as
follows:

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

`Node` is move-only and owns its complete compatibility AST. Text and dotted
name components are stored as values; `children`, `child`, and `partials` use
`std::unique_ptr<Node>` to make ownership explicit. Construct manually supplied
nodes with `std::make_unique<Node>()` and move them into those containers. The
tokenizer and decoder publish complete trees transactionally.

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

```sh
mustachec -t template.mustache -d data.json
mustachec -t template.mustache -d data.yml
mustachec -v
```

Run `mustachec -h` for the complete option list.

## Credits

- [John Boehr](https://github.com/jbboehr)
- [Adam Baratz](https://github.com/adambaratz)
- [All contributors](https://github.com/jbboehr/libmustache/graphs/contributors)

## License

libmustache is released under the [MIT License](LICENSE.md).
