# Binary releases

The `release` workflow builds on pushes to `master` and `develop`, pull
requests targeting those branches, manual runs, and `v*` tag pushes. Only a
pushed `vX.Y.Z` tag matching the package metadata publishes release assets.
All 13 matrix entries must pass before publication; each archive has a SHA-256
sidecar. Other runs retain the same packages as Actions artifacts.

CI and release Nix jobs share a GitHub Actions cache through
`nix-community/cache-nix-action`; no external cache account or token is required.
Caches are separated by operating system and architecture. Each matrix entry
saves under its own commit-specific key, while restores merge available caches
for the same `flake.lock`, including dependencies built by other jobs. A fallback
to an older cache also allows unchanged dependencies to survive lockfile updates.
Nix still determines which store paths match the requested build.

GitHub's cache scope rules apply: jobs can read caches from their current ref
and the default branch (`master`); pull requests can also read their base branch.
Tag runs can therefore reuse `master` caches, but cannot directly read caches
that exist only on `develop`. Cache entries are subject to GitHub's storage and
retention limits. The cache covers Nix inputs and outputs, including the musl
job's JSON inputs and the Windows cross-builds. Native MSVC builds are outside it.

Windows uses MSVC v142/v143 on x86/x64. Linux glibc and macOS arm64 use the
locked Nix inputs:

```sh
nix build .#release-linux-x64-glibc-static
nix build .#release-linux-x64-glibc-shared
# On an Apple Silicon Mac:
nix build .#release-macos-aarch64-static
nix build .#release-macos-aarch64-shared
```

The musl static package is cross-built by Nix with its pinned musl toolchain.
Each Nix derivation runs the project test suite and packaging checks. Windows
also checks the extracted SDK and CLI in its build script.

## Windows cross-build checks

On x86_64 Linux, the flake also exposes Windows x64 packages built with
clang-cl and nixpkgs' xwin-backed `windows.sdk`:

```sh
nix build .#libmustache-windows-x64-xwin-static
nix build .#libmustache-windows-x64-xwin-shared
```

Both install the headers, CMake metadata, static library, DLL/import library,
and `mustachec.exe`. The suffix selects whether the executable links libmustache
statically or through its DLL. Both use the DLL C++ runtime (`/MD`), matching
PHP's Windows builds. JSON and archived templates are enabled; YAML is disabled,
as in the native Windows release builds.

These packages are also flake checks, so `nix flake check` and the generated CI
matrix run them. During each Nix build, Wine runs the archive compatibility
probe, the Windows executables in the registered CTest suite, an installed
CMake consumer, and CLI version/render checks. Native-only CMake tests and the
YAML specification tests are not registered in this configuration.

The existing `flake.lock` pins the SDK fetches, LLVM, and Wine. The Windows
package set enables the Microsoft SDK license acceptance required by nixpkgs.
These checks supplement the native MSVC release jobs; they do not publish
additional release assets.

`.github/scripts/publish-release.py` validates the entire artifact matrix and checksums,
then creates the tagged GitHub release or updates its binary assets. Updating
an existing release preserves its title and notes.
