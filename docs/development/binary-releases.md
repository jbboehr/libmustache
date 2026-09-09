# Binary releases

The `ci` workflow runs on pushes to `master`, `develop`, `release`, and
`release/**`, pull requests targeting those branches, manual runs, and `v*` tag
pushes. After all its checks succeed, it calls `release.yml` for the same commit
and ref to build the Windows and Unix packages. CI's Nix caches are saved before
packaging tries to restore them. The existing CI checks keep their names;
packaging and publishing appear under the `Release` job in the same run.

Release branch pushes create or update a draft for the package version;
manual runs can opt in with the **draft** checkbox. Only a pushed `vX.Y.Z` tag
matching the package metadata automatically publishes release assets.
All 13 matrix entries must pass before publication; each archive has a SHA-256
sidecar. Other runs retain the same packages as Actions artifacts.

To inspect a release before tagging:

1. Update the package version consistently (for example, to `0.6.1`) and push
   the commit to `release/0.6.1`, or manually run `ci` on that commit's
   branch with **draft** enabled.
2. Wait for the workflow to finish, then open the `v0.6.1` draft in GitHub
   Releases to inspect its binaries and edit its title or notes. The draft
   targets the exact build commit and does not create the Git tag. Further
   release branch pushes refresh its assets and target, preserving edited notes.
3. Push `v0.6.1` at the commit you approved. Its workflow rebuilds and validates
   the packages, uploads them, and publishes the draft without replacing your
   title or notes.

Manual runs can also draft an existing version tag by selecting it and enabling
**draft** in `ci`. The new manual input becomes available once these workflow
changes are on the default branch (`master`). A draft run refuses to modify an
already published release.
Release uploads are serialized and queued so branch drafts and tag publication
cannot overlap or displace pending uploads. New runs do not cancel active release
branch, manual draft, or tag runs.

Draft creation uses the workflow's normal `GITHUB_TOKEN` unless the repository
secret `RELEASE_TOKEN` is set. The normal token suffices once the workflow changes
are on `master`. If the draft's target commit has workflow files that differ from
`master`, GitHub's [release creation API](https://docs.github.com/en/rest/releases/releases#create-a-release)
also requires **Workflows: write**; provide `RELEASE_TOKEN` with both
**Contents: write** and **Workflows: write** repository permissions for that case.
Only the draft step uses this optional token.

CI and release Nix jobs share a GitHub Actions cache through
`nix-community/cache-nix-action`; no external cache account or token is required.
Caches are separated by operating system and architecture. Each matrix entry
saves under its own commit-specific key, while restores merge available caches
for the same `flake.lock`, including dependencies built by other jobs. A fallback
to an older cache also allows unchanged dependencies to survive lockfile updates.
Nix still determines which store paths match the requested build.

GitHub's cache scope rules apply: jobs can read caches from their current ref
and the default branch (`master`); pull requests can also read their base branch.
Tag runs reuse caches saved by their own CI stage and can also reuse `master`
caches, but cannot directly read caches that exist only on `develop`.
Cache entries are subject to GitHub's storage and retention limits. The cache
covers Nix inputs and outputs, including the musl job's JSON inputs and the
Windows cross-builds. Native MSVC builds are outside it.
macOS CI uses Homebrew, so it does not populate the macOS Nix cache; those release
jobs can reuse caches from earlier macOS package builds.

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
then creates or updates the GitHub release. Draft publication happens only after
asset uploads succeed. Updating an existing release preserves its title and notes.
