# Windows binaries

The ZIP packages contain a Release build of libmustache, `mustachec.exe`, public
headers, CMake package files, and license notices. Extract each package into its
own directory. Package names identify the architecture, Visual C++ toolset,
runtime, and library linkage:

- `x86` or `x64` must match the application or PHP build.
- `v142` uses the Visual Studio 2019 toolset; `v143` uses Visual Studio 2022.
- `md` uses the dynamic MSVC runtime (`/MD`), including in static packages.
- `static` includes `lib/mustache_static.lib` and a CLI that links libmustache
  statically. `shared` includes `lib/mustache.lib`, `bin/mustache.dll`, and a CLI
  that uses that DLL.

Both variants require the appropriate Microsoft Visual C++ Redistributable.
JSON and archived-template support are enabled. YAML support is disabled.
There are no additional parser DLLs or libraries to install or link.

For php-mustache's Windows build, choose the **static** package matching PHP's
architecture and toolset, then pass its extracted directory as
`--with-libmustache=C:\path\to\libmustache`. The current extension configuration
expects `mustache_static.lib` and defines `MUSTACHE_STATIC_DEFINE`. TS and NTS
PHP builds can use the same matching libmustache package. Debug builds require
a separate library build with the matching debug runtime.

CMake consumers set `CMAKE_PREFIX_PATH` to the extracted directory and request
the package's linkage explicitly:

```cmake
find_package(mustache 0.6 CONFIG REQUIRED COMPONENTS static)
target_link_libraries(example PRIVATE mustache::mustache_static)
```

For the shared package, request `COMPONENTS shared` and link
`mustache::mustache`. Keep `mustache.dll` beside the consuming executable or in
its DLL search path. Manual static consumers must define
`MUSTACHE_STATIC_DEFINE` when including libmustache headers.

The executable accepts JSON data files:

```bat
bin\mustachec.exe -t template.mustache -d data.json
```

Each ZIP has a companion `.sha256` file. Preserve `LICENSE.md` and the notices
under `licenses/` when redistributing these binaries.
