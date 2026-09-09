# Linux and macOS binaries

Each tarball contains headers, CMake and pkg-config metadata, `mustachec`, and
either the static library or the shared library with its versioned symlinks.
Extract each package into its own directory. Nix is not required to use it.

Choose `linux-x64-glibc` for glibc Linux, `linux-x64-musl` for musl Linux such as
Alpine, or `macos-aarch64` for Apple Silicon. The glibc builds use GCC 13; musl
builds use GCC 15. Both use the libstdc++ C++11 ABI. macOS builds use libc++ and
require macOS 14 or newer.
The glibc packages require glibc 2.38 or newer and libstdc++ providing
`GLIBCXX_3.4.32` or newer; CI verifies them on Ubuntu 24.04.
The musl static package is built with Nix's pinned musl toolchain.
Use the matching system C++ runtime. Static packages link libmustache statically;
the executable still uses the system C++ runtime and libc.

JSON and archived-template support are enabled; YAML is disabled. Static
libraries contain position-independent code suitable for linking into a PHP
extension or another shared library. No additional parser libraries are needed.

For CMake, set `CMAKE_PREFIX_PATH` to the extracted directory:

```cmake
find_package(mustache 0.6 CONFIG REQUIRED COMPONENTS static)
target_link_libraries(example PRIVATE mustache::mustache_static)
```

For shared packages, request `COMPONENTS shared` and link `mustache::mustache`.
Set `PKG_CONFIG_PATH` to the extracted `lib/pkgconfig` directory for pkg-config
consumers. Configure your application's runtime library search path for shared
linkage. The packaged executable locates libmustache relative to its own path.

```sh
bin/mustachec -t template.mustache -d data.json
```

Every tarball has a companion SHA-256 checksum. Preserve `LICENSE.md` and the
notices in `licenses/` when redistributing these binaries.
